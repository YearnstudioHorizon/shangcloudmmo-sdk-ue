#pragma once

#include "CoreMinimal.h"
#include "MmoApiModels.h"
#include "ShangCloudApiClient.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiNewRoom, const FMmoNewRoomResponse&, Response, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiJoinRoom, const FMmoJoinRoomResponse&, Response, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnApiSimple, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiGetRoomData, const FString&, JsonData, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiUserCount, int32, UserCount, const FString&, Error);

UCLASS(BlueprintType)
class SHANGCLOUDMMO_API UShangCloudApiClient : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ShangCloud|API")
	FString BaseUrl = TEXT("https://api.yearnstudio.cn");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ShangCloud|API")
	FString AccessToken;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ShangCloud|API")
	FString TokenType = TEXT("Bearer");

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void NewRoom(const FString& Protocol, FOnApiNewRoom OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void JoinRoom(const FString& RoomId, const FString& Protocol, FOnApiJoinRoom OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void SetRoomConfig(const FString& RoomId, bool bAllowMultiLogin, FOnApiSimple OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void SetRoomData(const FString& RoomId, const FString& Key, const FString& Value, const FString& Type, FOnApiSimple OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void GetRoomData(const FString& RoomId, FOnApiGetRoomData OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void DeleteRoomData(const FString& RoomId, const FString& Key, FOnApiSimple OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void KickUser(const FString& RoomId, const FString& TargetUid, FOnApiSimple OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|API")
	void GetRoomUserCount(const FString& RoomId, FOnApiUserCount OnComplete);

private:
	void SendRequest(const FString& Path, const FString& JsonBody,
		const FString& RoomId, const FString& Protocol,
		TFunction<void(const FString& ResponseBody, const FString& Error)> Callback);
};
