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

/** RFC 8628 device authorization response */
USTRUCT(BlueprintType)
struct SHANGCLOUDMMO_API FDeviceAuthorizationResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString DeviceCode;
	UPROPERTY(BlueprintReadOnly) FString UserCode;
	UPROPERTY(BlueprintReadOnly) FString VerificationUri;
	UPROPERTY(BlueprintReadOnly) FString VerificationUriComplete;
	UPROPERTY(BlueprintReadOnly) int32 ExpiresIn = 900;
	UPROPERTY(BlueprintReadOnly) int32 Interval = 5;

	void FromJson(const TSharedPtr<FJsonObject>& Json);
};

/** OAuth token response (device_code / refresh_token) */
USTRUCT(BlueprintType)
struct SHANGCLOUDMMO_API FOAuthTokenResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString AccessToken;
	UPROPERTY(BlueprintReadOnly) FString TokenType;
	UPROPERTY(BlueprintReadOnly) int32 ExpiresIn = 0;
	UPROPERTY(BlueprintReadOnly) FString RefreshToken;
	UPROPERTY(BlueprintReadOnly) FString Scope;
	UPROPERTY(BlueprintReadOnly) FString IdToken;

	void FromJson(const TSharedPtr<FJsonObject>& Json);
};
