#pragma once

#include "MmoTransport.h"
#include "HAL/Runnable.h"

class FSocket;

class SHANGCLOUDMMO_API FMmoUdpTransport : public FMmoTransport, public FRunnable
{
public:
	virtual ~FMmoUdpTransport() override;

	virtual void Connect(const FString& Host, int32 Port, const FString& ConnectKey) override;
	virtual void Disconnect() override;
	virtual void Poll(float DeltaTime) override;
	virtual void Send(const uint8* Data, int32 Length) override;

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	FSocket* Socket = nullptr;
	FRunnableThread* Thread = nullptr;
	TAtomic<bool> bStopping{false};
	TAtomic<bool> bPacketReceived{false};

	TSharedPtr<FInternetAddr> RemoteAddr;
	TSharedPtr<FInternetAddr> ResolvedAddr;
	int32 EdgePort = 0;
	uint64 ConnectId = 0;
	float TimeoutTimer = 0.f;

	static constexpr float AuthTimeout = 10.0f;
	static constexpr float HbTimeout = 15.0f;
	static constexpr int32 MaxConnectKeyBytes = 256;

	void SendAuthPacket();
	void ReconnectSocket();
	void CleanupSocket();
	bool CreateAndBindSocket();
};
