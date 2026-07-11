#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MmoTypes.h"
#include "MmoMessageQueue.h"
#include "MmoInterpEngine.h"
#include "ShangCloudMmoComponent.generated.h"

class FMmoTransport;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMmoConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMmoDisconnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMmoConnectionError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMmoMessageReceived, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMmoRawMessageReceived, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMmoUserJoined, const FString&, Uid, const FString&, Nickname);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMmoUserLeft, const FString&, Uid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMmoServerClosed);

// 高级封装事件（参考 core.js 的广播与同步变量协议）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMmoBroadcastReceived, const FString&, Uid, const FString&, Message, const FString&, Extra);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMmoSyncVarReceived, const FMmoSyncVarPayload&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMmoSyncVarInterpolated, const FString&, Uid, const FString&, VarName, double, Value);

UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class SHANGCLOUDMMO_API UShangCloudMmoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShangCloudMmoComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShangCloud|MMO")
	EMmoProtocol Protocol = EMmoProtocol::TCP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShangCloud|MMO")
	FString ConnectKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShangCloud|MMO")
	FString EdgeHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShangCloud|MMO")
	int32 EdgePort = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShangCloud|MMO")
	FString EdgeUrl;

	UPROPERTY(BlueprintReadOnly, Category = "ShangCloud|MMO")
	EMmoConnectionState ConnectionState = EMmoConnectionState::Idle;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoConnectionError OnConnectionError;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoMessageReceived OnMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoRawMessageReceived OnRawMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoUserJoined OnUserJoined;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoUserLeft OnUserLeft;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoServerClosed OnServerClosed;

	// 高级封装事件（参考 core.js 的广播与同步变量协议）
	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoBroadcastReceived OnBroadcastReceived;

	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoSyncVarReceived OnSyncVarReceived;

	/** 插帧引擎逐帧推进时触发（Uid, VarName, Value），调用方据此回写场景对象。移植自 core.js 的 _ensureInterpLoop。 */
	UPROPERTY(BlueprintAssignable, Category = "ShangCloud|MMO|Events")
	FOnMmoSyncVarInterpolated OnSyncVarInterpolated;

	/** Configures connection parameters from an API response. */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void ConfigureFromApiResponse(const FString& InConnectKey, const FString& InEdgeUrl, const FString& InProtocol);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void ConnectToEdge();

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void DisconnectFromEdge();

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void SendMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void SendRaw(const TArray<uint8>& Data);

	/**
	 * 发送广播消息。wire 格式（参考 core.js 的 sendMmoMessage）：
	 * {"uid":"...","message":"...","extra":"..."} —— 不含 type 字段。
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void SendBroadcast(const FString& Uid, const FString& Message, const FString& Extra);

	/**
	 * 发送同步变量。wire 格式（参考 core.js 的 __sync_var__）：
	 * {"type":"__sync_var__","uid":"...","vars":{...},"interp":["x",...]}
	 * @param Vars   变量名→值（值会被转为字符串，与 core.js 一致）
	 * @param Interp 需要接收端插帧平滑的变量名列表
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void SendSyncVar(const FString& Uid, const TMap<FString, FString>& Vars, const TArray<FString>& Interp);

	/** 发送加入房间通知。wire 格式：{"type":"__join__","uid":"...","nickname":"..."} */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void SendJoinAnnouncement(const FString& Uid, const FString& Nickname);

	/**
	 * 读取指定 uid 的同步变量当前值（插帧变量的 current，平滑后）。
	 * 若该变量不在插帧集合或尚未建立状态，回退到最近原始值。
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	double GetSyncVar(const FString& Uid, const FString& VarName) const;

	/** 读取指定 uid 的同步变量原始字符串值（不做插帧）。 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	FString GetSyncVarRaw(const FString& Uid, const FString& VarName) const;

	/** 清理指定 uid 的插帧状态（玩家离开时调用）。 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|MMO")
	void ClearSyncVarState(const FString& Uid);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TUniquePtr<FMmoTransport> Transport;
	FMmoMessageQueue MessageQueue;
	FMmoInterpEngine InterpEngine;

	void CleanupTransport();
	void ProcessBusinessMessage(const FString& Message);
	void ParseEdgeUrl(const FString& Url);
};
