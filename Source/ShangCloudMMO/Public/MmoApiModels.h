#pragma once

#include "CoreMinimal.h"
#include "MmoApiModels.generated.h"

USTRUCT(BlueprintType)
struct SHANGCLOUDMMO_API FMmoNewRoomResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString ConnectKey;
	UPROPERTY(BlueprintReadOnly) FString EdgeUrl;
	UPROPERTY(BlueprintReadOnly) FString RoomId;
	UPROPERTY(BlueprintReadOnly) FString Protocol;

	void FromJson(const TSharedPtr<FJsonObject>& Json);
};

USTRUCT(BlueprintType)
struct SHANGCLOUDMMO_API FMmoJoinRoomResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString ConnectKey;
	UPROPERTY(BlueprintReadOnly) FString EdgeUrl;
	UPROPERTY(BlueprintReadOnly) FString RoomId;
	UPROPERTY(BlueprintReadOnly) FString Protocol;
	UPROPERTY(BlueprintReadOnly) FString AssignedUid;

	void FromJson(const TSharedPtr<FJsonObject>& Json);
};
