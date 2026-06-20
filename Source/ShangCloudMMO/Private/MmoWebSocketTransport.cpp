#include "MmoWebSocketTransport.h"
#include "MmoCrypto.h"
#include "WebSocketsModule.h"

FMmoWebSocketTransport::~FMmoWebSocketTransport()
{
	Disconnect();
}

void FMmoWebSocketTransport::ConnectWithUrl(const FString& Url, const FString& ConnectKey)
{
	Disconnect();

	ConnectKeyStr = ConnectKey;
	HeartbeatTimer = 0.f;
	bSeedSent = false;
	bAuthSent = false;
	State = EMmoConnectionState::Connecting;

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, TEXT(""));

	WebSocket->OnConnected().AddRaw(this, &FMmoWebSocketTransport::OnWsConnected);
	WebSocket->OnConnectionError().AddRaw(this, &FMmoWebSocketTransport::OnWsConnectionError);
	WebSocket->OnClosed().AddRaw(this, &FMmoWebSocketTransport::OnWsClosed);
	WebSocket->OnRawMessage().AddRaw(this, &FMmoWebSocketTransport::OnWsRawMessage);

	WebSocket->Connect();
}

void FMmoWebSocketTransport::Connect(const FString& Host, int32 Port, const FString& ConnectKey)
{
	FString Url = FString::Printf(TEXT("ws://%s:%d/ws"), *Host, Port);
	ConnectWithUrl(Url, ConnectKey);
}

void FMmoWebSocketTransport::Disconnect()
{
	State = EMmoConnectionState::Disconnected;
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		WebSocket->Close();
	}
	WebSocket.Reset();
}

void FMmoWebSocketTransport::Poll(float DeltaTime)
{
	if (State == EMmoConnectionState::Connected)
	{
		HeartbeatTimer += DeltaTime;
		if (HeartbeatTimer >= HeartbeatInterval)
		{
			HeartbeatTimer = 0.f;
			SendHeartbeat();
		}
	}
}

void FMmoWebSocketTransport::Send(const uint8* Data, int32 Length)
{
	if (State != EMmoConnectionState::Connected || !WebSocket.IsValid()) return;

	TArray<uint8> Encrypted = EncryptData(Data, Length);
	if (Encrypted.Num() > 0)
	{
		WebSocket->Send(Encrypted.GetData(), Encrypted.Num(), true);
	}
}

void FMmoWebSocketTransport::OnWsConnected()
{
	State = EMmoConnectionState::Handshake;

	// Step 1: Send 32-byte seed as binary message
	PendingSeed = FMmoCrypto::GenerateSeed();
	AesKey = FMmoCrypto::DeriveKey(PendingSeed);
	WebSocket->Send(PendingSeed.GetData(), PendingSeed.Num(), true);
	bSeedSent = true;

	// Step 2: Send encrypted connect_key
	FTCHARToUTF8 KeyUtf8(*ConnectKeyStr);
	TArray<uint8> Encrypted = FMmoCrypto::Encrypt(AesKey,
		reinterpret_cast<const uint8*>(KeyUtf8.Get()), KeyUtf8.Length());

	if (Encrypted.Num() == 0)
	{
		State = EMmoConnectionState::Error;
		OnError.ExecuteIfBound(TEXT("Failed to encrypt connect_key"));
		return;
	}

	WebSocket->Send(Encrypted.GetData(), Encrypted.Num(), true);
	bAuthSent = true;
	State = EMmoConnectionState::Authenticating;
}

void FMmoWebSocketTransport::OnWsConnectionError(const FString& Error)
{
	State = EMmoConnectionState::Error;
	OnError.ExecuteIfBound(FString::Printf(TEXT("WebSocket connection error: %s"), *Error));
	OnDisconnected.ExecuteIfBound();
}

void FMmoWebSocketTransport::OnWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	if (State != EMmoConnectionState::Error)
	{
		State = EMmoConnectionState::Disconnected;
	}
	OnServerClosed.ExecuteIfBound();
	OnDisconnected.ExecuteIfBound();
}

void FMmoWebSocketTransport::OnWsRawMessage(const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
{
	if (Size == 0) return;

	TArray<uint8> Decrypted = DecryptData(static_cast<const uint8*>(Data), static_cast<int32>(Size));
	if (Decrypted.Num() > 0)
	{
		ProcessDecryptedMessage(Decrypted);
	}
}
