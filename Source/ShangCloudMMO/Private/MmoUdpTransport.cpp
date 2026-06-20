#include "MmoUdpTransport.h"
#include "MmoCrypto.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

FMmoUdpTransport::~FMmoUdpTransport()
{
	Disconnect();
}

void FMmoUdpTransport::Connect(const FString& Host, int32 Port, const FString& ConnectKey)
{
	Disconnect();

	ConnectKeyStr = ConnectKey;
	EdgePort = Port;
	ConnectId = 0;
	HeartbeatTimer = 0.f;
	TimeoutTimer = 0.f;
	bStopping = false;
	bPacketReceived = false;

	// Resolve hostname
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FAddressInfoResult Result = SocketSub->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None);
	if (Result.ReturnCode != SE_NO_ERROR || Result.Results.Num() == 0)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(FString::Printf(TEXT("Failed to resolve hostname: %s"), *Host));
		return;
	}

	ResolvedAddr = Result.Results[0].Address->Clone();
	ResolvedAddr->SetPort(EdgePort);

	if (!CreateAndBindSocket())
		return;

	State = EMmoConnectionState::Connecting;
	SendAuthPacket();

	if (State == EMmoConnectionState::Error)
		return;

	State = EMmoConnectionState::Authenticating;

	Thread = FRunnableThread::Create(this, TEXT("MMO-UDP-Recv"), 0, TPri_Normal);
}

void FMmoUdpTransport::Disconnect()
{
	bStopping = true;
	State = EMmoConnectionState::Disconnected;
	ConnectId = 0;

	CleanupSocket();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FMmoUdpTransport::Poll(float DeltaTime)
{
	if (State == EMmoConnectionState::Authenticating)
	{
		TimeoutTimer += DeltaTime;
		if (TimeoutTimer >= AuthTimeout)
		{
			State = EMmoConnectionState::Error;
			OnError.ExecuteIfBound(TEXT("UDP authentication timed out"));
		}
		return;
	}

	if (State == EMmoConnectionState::Connected)
	{
		HeartbeatTimer += DeltaTime;
		if (HeartbeatTimer >= HeartbeatInterval)
		{
			HeartbeatTimer = 0.f;
			SendHeartbeat();
		}

		if (bPacketReceived.Exchange(false))
		{
			TimeoutTimer = 0.f;
		}
		else
		{
			TimeoutTimer += DeltaTime;
		}

		if (TimeoutTimer >= HbTimeout)
		{
			ReconnectSocket();
			TimeoutTimer = 0.f;
		}
	}
}

void FMmoUdpTransport::Send(const uint8* Data, int32 Length)
{
	if (State != EMmoConnectionState::Connected || !Socket) return;

	TArray<uint8> Encrypted = EncryptData(Data, Length);
	if (Encrypted.Num() == 0) return;

	// [8B connectId BE][encrypted payload]
	TArray<uint8> Packet;
	Packet.SetNumUninitialized(8 + Encrypted.Num());
	FMmoCrypto::WriteU64BE(Packet.GetData(), ConnectId);
	FMemory::Memcpy(Packet.GetData() + 8, Encrypted.GetData(), Encrypted.Num());

	int32 Sent = 0;
	Socket->SendTo(Packet.GetData(), Packet.Num(), Sent, *ResolvedAddr);
}

void FMmoUdpTransport::Stop()
{
	bStopping = true;
}

uint32 FMmoUdpTransport::Run()
{
	TArray<uint8> RecvBuffer;
	RecvBuffer.SetNumUninitialized(65536);

	while (!bStopping)
	{
		int32 Read = 0;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

		if (!Socket)
		{
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		// Check if data is available (with small timeout)
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
			continue;

		if (!Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), Read, *Sender))
			continue;

		if (Read < 8) continue;

		bPacketReceived = true;

		uint64 PktConnectId = FMmoCrypto::ReadU64BE(RecvBuffer.GetData());
		int32 PayloadSize = Read - 8;
		if (PayloadSize <= 0) continue;

		TArray<uint8> Decrypted = DecryptData(RecvBuffer.GetData() + 8, PayloadSize);
		if (Decrypted.Num() == 0) continue;

		if (State == EMmoConnectionState::Authenticating)
		{
			ConnectId = PktConnectId;
		}

		ProcessDecryptedMessage(Decrypted);

		if (State == EMmoConnectionState::Disconnected) break;
	}

	return 0;
}

void FMmoUdpTransport::SendAuthPacket()
{
	FTCHARToUTF8 KeyUtf8(*ConnectKeyStr);
	if (KeyUtf8.Length() > MaxConnectKeyBytes)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("connect_key byte length exceeds 256 bytes"));
		return;
	}

	TArray<uint8> Seed = FMmoCrypto::GenerateSeed();
	AesKey = FMmoCrypto::DeriveKey(Seed);

	TArray<uint8> EncKey = FMmoCrypto::Encrypt(AesKey,
		reinterpret_cast<const uint8*>(KeyUtf8.Get()), KeyUtf8.Length());
	if (EncKey.Num() == 0)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to encrypt connect_key"));
		return;
	}

	// [8B connectId=0 BE][32B seed][encrypted connect_key]
	int32 TotalSize = 8 + FMmoCrypto::SeedSize + EncKey.Num();
	TArray<uint8> AuthPacket;
	AuthPacket.SetNumUninitialized(TotalSize);

	FMmoCrypto::WriteU64BE(AuthPacket.GetData(), 0ULL);
	FMemory::Memcpy(AuthPacket.GetData() + 8, Seed.GetData(), FMmoCrypto::SeedSize);
	FMemory::Memcpy(AuthPacket.GetData() + 8 + FMmoCrypto::SeedSize, EncKey.GetData(), EncKey.Num());

	int32 Sent = 0;
	Socket->SendTo(AuthPacket.GetData(), AuthPacket.Num(), Sent, *ResolvedAddr);
}

void FMmoUdpTransport::ReconnectSocket()
{
	CleanupSocket();
	CreateAndBindSocket();
}

bool FMmoUdpTransport::CreateAndBindSocket()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Socket = SocketSub->CreateSocket(NAME_DGram, TEXT("MMO-UDP"), ResolvedAddr->GetProtocolType());
	if (!Socket)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to create UDP socket"));
		return false;
	}

	Socket->SetNonBlocking(false);
	Socket->SetReuseAddr(true);

	TSharedRef<FInternetAddr> BindAddr = SocketSub->CreateInternetAddr();
	BindAddr->SetAnyAddress();
	BindAddr->SetPort(0);

	if (!Socket->Bind(*BindAddr))
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("UDP bind failed"));
		CleanupSocket();
		return false;
	}

	return true;
}

void FMmoUdpTransport::CleanupSocket()
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}
