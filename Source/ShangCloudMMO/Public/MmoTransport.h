#pragma once

#include "CoreMinimal.h"
#include "MmoTypes.h"
#include "MmoMessageQueue.h"

DECLARE_DELEGATE(FOnTransportConnected);
DECLARE_DELEGATE(FOnTransportDisconnected);
DECLARE_DELEGATE_OneParam(FOnTransportError, const FString&);
DECLARE_DELEGATE(FOnTransportServerClosed);

class SHANGCLOUDMMO_API FMmoTransport
{
public:
	virtual ~FMmoTransport() = default;

	virtual void Connect(const FString& Host, int32 Port, const FString& ConnectKey) = 0;
	virtual void Disconnect() = 0;
	virtual void Poll(float DeltaTime) = 0;
	virtual void Send(const uint8* Data, int32 Length) = 0;

	EMmoConnectionState GetState() const { return State; }

	void SetMessageQueue(FMmoMessageQueue* InQueue) { MessageQueue = InQueue; }

	FOnTransportConnected OnConnected;
	FOnTransportDisconnected OnDisconnected;
	FOnTransportError OnError;
	FOnTransportServerClosed OnServerClosed;

protected:
	EMmoConnectionState State = EMmoConnectionState::Idle;
	TArray<uint8> AesKey;
	FString ConnectKeyStr;
	FMmoMessageQueue* MessageQueue = nullptr;
	float HeartbeatTimer = 0.f;

	static constexpr float HeartbeatInterval = 3.0f;
	static constexpr int32 MaxFrameSize = 1024 * 1024;

	TArray<uint8> EncryptData(const uint8* Data, int32 Length);
	TArray<uint8> DecryptData(const uint8* Encrypted, int32 Length);
	void ProcessDecryptedMessage(const TArray<uint8>& Decrypted);
	void SendHeartbeat();
};
