#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MmoTypes.h"
#include "MmoMessageQueue.h"
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

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TUniquePtr<FMmoTransport> Transport;
	FMmoMessageQueue MessageQueue;

	void CleanupTransport();
	void ProcessBusinessMessage(const FString& Message);
	void ParseEdgeUrl(const FString& Url);
};
