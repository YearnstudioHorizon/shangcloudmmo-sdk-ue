#pragma once

#include "MmoTransport.h"
#include "HAL/Runnable.h"

class FSocket;
class FMmoTcpHeartbeatRunnable;

class SHANGCLOUDMMO_API FMmoTcpTransport : public FMmoTransport, public FRunnable
{
public:
	virtual ~FMmoTcpTransport() override;

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
	FRunnableThread* HeartbeatThread = nullptr;
	FMmoTcpHeartbeatRunnable* HeartbeatRunnable = nullptr;
	FCriticalSection SendLock;
	TAtomic<bool> bStopping{false};

	FString HostStr;
	int32 PortNum = 0;

	bool SendRawBytes(const uint8* Data, int32 Length);
	bool SendFrame(const uint8* Payload, int32 Length);
	bool ReadExact(uint8* Buffer, int32 Count);
	uint32 HeartbeatRun();
	void StopHeartbeatThread();
	void CleanupSocket();

	friend class FMmoTcpHeartbeatRunnable;
};
