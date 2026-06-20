#pragma once

#include "MmoTransport.h"
#include "IWebSocket.h"

class SHANGCLOUDMMO_API FMmoWebSocketTransport : public FMmoTransport
{
public:
	virtual ~FMmoWebSocketTransport() override;

	void ConnectWithUrl(const FString& Url, const FString& ConnectKey);

	virtual void Connect(const FString& Host, int32 Port, const FString& ConnectKey) override;
	virtual void Disconnect() override;
	virtual void Poll(float DeltaTime) override;
	virtual void Send(const uint8* Data, int32 Length) override;

private:
	TSharedPtr<IWebSocket> WebSocket;
	bool bSeedSent = false;
	bool bAuthSent = false;

	TArray<uint8> PendingSeed;

	void OnWsConnected();
	void OnWsConnectionError(const FString& Error);
	void OnWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnWsRawMessage(const void* Data, SIZE_T Size, SIZE_T BytesRemaining);
};
