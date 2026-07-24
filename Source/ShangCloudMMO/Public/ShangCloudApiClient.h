#pragma once

#include "CoreMinimal.h"
#include "MmoApiModels.h"
#include "ShangCloudApiClient.generated.h"

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiNewRoom, const FMmoNewRoomResponse&, Response, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiJoinRoom, const FMmoJoinRoomResponse&, Response, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnApiSimple, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiGetRoomData, const FString&, JsonData, const FString&, Error);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiUserCount, int32, UserCount, const FString&, Error);

/** Device auth start: Response + CodeVerifier (do not log verifier) + Error */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnApiDeviceAuthStart, const FDeviceAuthorizationResponse&, Response, const FString&, CodeVerifier, const FString&, Error);
/** OAuth token result */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnApiOAuthToken, const FOAuthTokenResponse&, Response, const FString&, Error);
/** User should open browser: UserCode, VerificationUri, VerificationUriComplete */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnApiDeviceUserCode, const FString&, UserCode, const FString&, VerificationUri, const FString&, VerificationUriComplete);

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ShangCloud|API")
	FString RefreshToken;

	/** OAuth client_id for public-client device auth / refresh (no secret) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ShangCloud|API")
	FString ClientId;

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

	/**
	 * Request device codes (public client + PKCE S256, no client_secret).
	 * Enable "allow public PKCE" for the app in developer console.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|OAuth")
	void RequestDeviceAuthorization(const FString& InClientId, const FString& Scope, FOnApiDeviceAuthStart OnComplete);

	/**
	 * Single token poll. Soft pending: Error is "authorization_pending" or "slow_down".
	 * Success: Error empty and Response.AccessToken set (also applied to this client).
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|OAuth")
	void PollDeviceTokenOnce(const FString& DeviceCode, const FString& CodeVerifier, const FString& InClientId, FOnApiOAuthToken OnComplete);

	/**
	 * Full device login with PKCE: request codes, fire OnUserCode, poll until token/timeout.
	 * Call CancelDeviceLogin() to stop.
	 */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|OAuth")
	void LoginWithDeviceAuth(const FString& InClientId, const FString& Scope, FOnApiDeviceUserCode OnUserCode, FOnApiOAuthToken OnComplete);

	UFUNCTION(BlueprintCallable, Category = "ShangCloud|OAuth")
	void CancelDeviceLogin();

	/** Refresh as public client (client_id only, no secret) */
	UFUNCTION(BlueprintCallable, Category = "ShangCloud|OAuth")
	void RefreshAccessToken(const FString& InRefreshToken, const FString& InClientId, FOnApiOAuthToken OnComplete);

private:
	void SendRequest(const FString& Path, const FString& JsonBody,
		const FString& RoomId, const FString& Protocol,
		TFunction<void(const FString& ResponseBody, const FString& Error)> Callback);

	void SendFormRequest(const FString& Path, const TMap<FString, FString>& FormFields,
		TFunction<void(int32 StatusCode, const FString& ResponseBody)> Callback);

	void ApplyTokenResponse(const FOAuthTokenResponse& Token);
	static void MakePkce(FString& OutVerifier, FString& OutChallenge);
	static FString Base64UrlEncode(const TArray<uint8>& Data);
	static FString UrlEncodeForm(const TMap<FString, FString>& FormFields);

	void RequestDeviceAuthorizationInternal(const FString& InClientId, const FString& Scope,
		TFunction<void(const FDeviceAuthorizationResponse& Response, const FString& CodeVerifier, const FString& Error)> Callback);

	void PollDeviceTokenOnceInternal(const FString& DeviceCode, const FString& CodeVerifier, const FString& InClientId,
		TFunction<void(const FOAuthTokenResponse& Response, const FString& Error)> Callback);

	void ScheduleDevicePoll();
	void DoDevicePoll();
	bool DeviceLoginTick(float DeltaTime);

	bool bDeviceLoginActive = false;
	FString DeviceLoginDeviceCode;
	FString DeviceLoginCodeVerifier;
	FString DeviceLoginClientId;
	int32 DeviceLoginInterval = 5;
	double DeviceLoginDeadlineSeconds = 0.0;
	double DeviceLoginNextPollSeconds = 0.0;
	FTSTicker::FDelegateHandle DeviceLoginTickerHandle;
	FOnApiOAuthToken DeviceLoginOnComplete;
};
