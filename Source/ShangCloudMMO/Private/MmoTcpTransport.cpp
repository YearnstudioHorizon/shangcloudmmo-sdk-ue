#include "MmoTcpTransport.h"
#include "MmoCrypto.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/PlatformProcess.h"

class FMmoTcpHeartbeatRunnable : public FRunnable
{
public:
	explicit FMmoTcpHeartbeatRunnable(FMmoTcpTransport* InOwner)
		: Owner(InOwner)
	{
	}

	virtual uint32 Run() override
	{
		return Owner ? Owner->HeartbeatRun() : 0;
	}

	virtual void Stop() override
	{
		if (Owner)
		{
			Owner->Stop();
		}
	}

private:
	FMmoTcpTransport* Owner = nullptr;
};

FMmoTcpTransport::~FMmoTcpTransport()
{
	Disconnect();
}

void FMmoTcpTransport::Connect(const FString& Host, int32 Port, const FString& ConnectKey)
{
	Disconnect();

	ConnectKeyStr = ConnectKey;
	HostStr = Host;
	PortNum = Port;
	HeartbeatTimer = 0.f;
	bStopping = false;
	State = EMmoConnectionState::Connecting;

	Thread = FRunnableThread::Create(this, TEXT("MMO-TCP-Recv"), 0, TPri_Normal);
	HeartbeatRunnable = new FMmoTcpHeartbeatRunnable(this);
	HeartbeatThread = FRunnableThread::Create(HeartbeatRunnable, TEXT("MMO-TCP-Heartbeat"), 0, TPri_Normal);
}

void FMmoTcpTransport::Disconnect()
{
	bStopping = true;
	State = EMmoConnectionState::Disconnected;
	StopHeartbeatThread();
	CleanupSocket();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FMmoTcpTransport::Poll(float DeltaTime)
{
	// TCP heartbeats are sent from a background thread so the server does
	// not drop the connection when the component tick is paused or disabled.
}

void FMmoTcpTransport::Send(const uint8* Data, int32 Length)
{
	if (State != EMmoConnectionState::Connected) return;

	TArray<uint8> Encrypted = EncryptData(Data, Length);
	if (Encrypted.Num() > 0)
	{
		SendFrame(Encrypted.GetData(), Encrypted.Num());
	}
}

void FMmoTcpTransport::Stop()
{
	bStopping = true;
}

uint32 FMmoTcpTransport::Run()
{
	// Resolve address
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();

	FAddressInfoResult Result = SocketSub->GetAddressInfo(*HostStr, nullptr, EAddressInfoFlags::Default, NAME_None);
	if (Result.ReturnCode != SE_NO_ERROR || Result.Results.Num() == 0)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(FString::Printf(TEXT("Failed to resolve hostname: %s"), *HostStr));
		return 1;
	}

	Addr = Result.Results[0].Address->Clone();
	Addr->SetPort(PortNum);

	// Create TCP socket
	Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("MMO-TCP"), Addr->GetProtocolType());
	if (!Socket)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to create TCP socket"));
		return 1;
	}

	Socket->SetNonBlocking(false);
	Socket->SetNoDelay(true);

	if (!Socket->Connect(*Addr))
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("TCP connect failed"));
		CleanupSocket();
		return 1;
	}

	// Step 1: Send 32-byte seed (plaintext)
	TArray<uint8> Seed = FMmoCrypto::GenerateSeed();
	AesKey = FMmoCrypto::DeriveKey(Seed);
	if (!SendRawBytes(Seed.GetData(), Seed.Num()))
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to send seed"));
		CleanupSocket();
		return 1;
	}
	State = EMmoConnectionState::Handshake;

	// Step 2: Send encrypted connect_key with length-prefix frame
	FTCHARToUTF8 KeyUtf8(*ConnectKeyStr);
	TArray<uint8> Encrypted = FMmoCrypto::Encrypt(AesKey,
		reinterpret_cast<const uint8*>(KeyUtf8.Get()), KeyUtf8.Length());
	if (Encrypted.Num() == 0 || !SendFrame(Encrypted.GetData(), Encrypted.Num()))
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to send encrypted connect_key"));
		CleanupSocket();
		return 1;
	}
	State = EMmoConnectionState::Authenticating;

	// Step 3: Receive loop
	uint8 LenBuf[4];
	while (!bStopping)
	{
		if (!ReadExact(LenBuf, 4)) break;

		uint32 PayloadLen = FMmoCrypto::ReadU32BE(LenBuf);
		if (PayloadLen > static_cast<uint32>(MaxFrameSize))
		{
			State = EMmoConnectionState::Error;
			OnError.ExecuteIfBound(TEXT("TCP frame length exceeds 1MB limit"));
			break;
		}

		TArray<uint8> EncPayload;
		EncPayload.SetNumUninitialized(PayloadLen);
		if (!ReadExact(EncPayload.GetData(), PayloadLen)) break;

		TArray<uint8> Decrypted = DecryptData(EncPayload.GetData(), PayloadLen);
		if (Decrypted.Num() > 0)
		{
			ProcessDecryptedMessage(Decrypted);
		}

		if (State == EMmoConnectionState::Disconnected) break;
	}

	if (State != EMmoConnectionState::Error && State != EMmoConnectionState::Disconnected)
	{
		State = EMmoConnectionState::Disconnected;
		OnDisconnected.ExecuteIfBound();
	}

	CleanupSocket();
	return 0;
}

bool FMmoTcpTransport::SendRawBytes(const uint8* Data, int32 Length)
{
	FScopeLock Lock(&SendLock);
	if (!Socket) return false;

	int32 TotalSent = 0;
	while (TotalSent < Length)
	{
		int32 Sent = 0;
		if (!Socket->Send(Data + TotalSent, Length - TotalSent, Sent))
			return false;
		TotalSent += Sent;
	}
	return true;
}

bool FMmoTcpTransport::SendFrame(const uint8* Payload, int32 Length)
{
	uint8 Header[4];
	FMmoCrypto::WriteU32BE(Header, static_cast<uint32>(Length));

	FScopeLock Lock(&SendLock);
	if (!Socket) return false;

	int32 Sent = 0;
	if (!Socket->Send(Header, 4, Sent) || Sent != 4)
		return false;

	int32 TotalSent = 0;
	while (TotalSent < Length)
	{
		if (!Socket->Send(Payload + TotalSent, Length - TotalSent, Sent))
			return false;
		TotalSent += Sent;
	}
	return true;
}

bool FMmoTcpTransport::ReadExact(uint8* Buffer, int32 Count)
{
	int32 TotalRead = 0;
	while (TotalRead < Count && !bStopping)
	{
		int32 Read = 0;
		if (!Socket || !Socket->Recv(Buffer + TotalRead, Count - TotalRead, Read))
			return false;
		if (Read <= 0) return false;
		TotalRead += Read;
	}
	return TotalRead == Count;
}

uint32 FMmoTcpTransport::HeartbeatRun()
{
	float Elapsed = 0.f;
	while (!bStopping &&
		State != EMmoConnectionState::Disconnected &&
		State != EMmoConnectionState::Error)
	{
		FPlatformProcess::Sleep(0.1f);
		Elapsed += 0.1f;

		if (bStopping ||
			State == EMmoConnectionState::Disconnected ||
			State == EMmoConnectionState::Error)
		{
			break;
		}

		if (Elapsed >= HeartbeatInterval)
		{
			Elapsed = 0.f;
			if (State == EMmoConnectionState::Connected)
			{
				SendHeartbeat();
			}
		}
	}

	return 0;
}

void FMmoTcpTransport::StopHeartbeatThread()
{
	if (HeartbeatThread)
	{
		HeartbeatThread->WaitForCompletion();
		delete HeartbeatThread;
		HeartbeatThread = nullptr;
	}

	if (HeartbeatRunnable)
	{
		delete HeartbeatRunnable;
		HeartbeatRunnable = nullptr;
	}
}

void FMmoTcpTransport::CleanupSocket()
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}
