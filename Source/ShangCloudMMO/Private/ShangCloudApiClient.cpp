#include "ShangCloudApiClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

#include <openssl/rand.h>
#include <openssl/sha.h>

namespace
{
	const TCHAR* DeviceCodeGrantType = TEXT("urn:ietf:params:oauth:grant-type:device_code");
	const TCHAR* DefaultDeviceScope = TEXT("openid profile mmo");
}

void UShangCloudApiClient::NewRoom(const FString& Protocol, FOnApiNewRoom OnComplete)
{
	SendRequest(TEXT("/api/mmo/room/new"), TEXT("{}"), TEXT(""), Protocol,
		[OnComplete](const FString& Body, const FString& Error)
		{
			FMmoNewRoomResponse Response;
			if (Error.IsEmpty())
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				if (FJsonSerializer::Deserialize(Reader, Json))
				{
					Response.FromJson(Json);
				}
			}
			OnComplete.ExecuteIfBound(Response, Error);
		});
}

void UShangCloudApiClient::JoinRoom(const FString& RoomId, const FString& Protocol, FOnApiJoinRoom OnComplete)
{
	SendRequest(TEXT("/api/mmo/room/join"), TEXT("{}"), RoomId, Protocol,
		[OnComplete](const FString& Body, const FString& Error)
		{
			FMmoJoinRoomResponse Response;
			if (Error.IsEmpty())
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				if (FJsonSerializer::Deserialize(Reader, Json))
				{
					Response.FromJson(Json);
				}
			}
			OnComplete.ExecuteIfBound(Response, Error);
		});
}

void UShangCloudApiClient::SetRoomConfig(const FString& RoomId, bool bAllowMultiLogin, FOnApiSimple OnComplete)
{
	FString JsonBody = FString::Printf(TEXT("{\"allow_multi_login\":%s}"),
		bAllowMultiLogin ? TEXT("true") : TEXT("false"));

	SendRequest(TEXT("/api/mmo/room/config"), JsonBody, RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Error);
		});
}

void UShangCloudApiClient::SetRoomData(const FString& RoomId, const FString& Key,
	const FString& Value, const FString& Type, FOnApiSimple OnComplete)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetStringField(TEXT("key"), Key);
	JsonObj->SetStringField(TEXT("value"), Value);
	if (!Type.IsEmpty())
	{
		JsonObj->SetStringField(TEXT("type"), Type);
	}

	FString JsonBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	SendRequest(TEXT("/api/mmo/room/data/set"), JsonBody, RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Error);
		});
}

void UShangCloudApiClient::GetRoomData(const FString& RoomId, FOnApiGetRoomData OnComplete)
{
	SendRequest(TEXT("/api/mmo/room/data/get"), TEXT("{}"), RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			if (Error.IsEmpty())
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				if (FJsonSerializer::Deserialize(Reader, Json))
				{
					const TSharedPtr<FJsonObject>* ExtraData;
					if (Json->TryGetObjectField(TEXT("extra_data"), ExtraData))
					{
						FString ExtraDataStr;
						TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ExtraDataStr);
						FJsonSerializer::Serialize(ExtraData->ToSharedRef(), Writer);
						OnComplete.ExecuteIfBound(ExtraDataStr, TEXT(""));
						return;
					}
				}
				OnComplete.ExecuteIfBound(TEXT("{}"), TEXT(""));
			}
			else
			{
				OnComplete.ExecuteIfBound(TEXT(""), Error);
			}
		});
}

void UShangCloudApiClient::DeleteRoomData(const FString& RoomId, const FString& Key, FOnApiSimple OnComplete)
{
	FString JsonBody = FString::Printf(TEXT("{\"key\":\"%s\"}"), *Key);

	SendRequest(TEXT("/api/mmo/room/data/delete"), JsonBody, RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Error);
		});
}

void UShangCloudApiClient::KickUser(const FString& RoomId, const FString& TargetUid, FOnApiSimple OnComplete)
{
	FString JsonBody = FString::Printf(TEXT("{\"target_uid\":\"%s\"}"), *TargetUid);

	SendRequest(TEXT("/api/mmo/room/kick"), JsonBody, RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Error);
		});
}

void UShangCloudApiClient::GetRoomUserCount(const FString& RoomId, FOnApiUserCount OnComplete)
{
	SendRequest(TEXT("/api/mmo/room/usercount"), TEXT("{}"), RoomId, TEXT(""),
		[OnComplete](const FString& Body, const FString& Error)
		{
			int32 Count = 0;
			if (Error.IsEmpty())
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				if (FJsonSerializer::Deserialize(Reader, Json))
				{
					Json->TryGetNumberField(TEXT("user_count"), Count);
				}
			}
			OnComplete.ExecuteIfBound(Count, Error);
		});
}

void UShangCloudApiClient::RequestDeviceAuthorization(const FString& InClientId, const FString& Scope, FOnApiDeviceAuthStart OnComplete)
{
	RequestDeviceAuthorizationInternal(InClientId, Scope,
		[OnComplete](const FDeviceAuthorizationResponse& Response, const FString& CodeVerifier, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Response, CodeVerifier, Error);
		});
}

void UShangCloudApiClient::RequestDeviceAuthorizationInternal(const FString& InClientId, const FString& Scope,
	TFunction<void(const FDeviceAuthorizationResponse& Response, const FString& CodeVerifier, const FString& Error)> Callback)
{
	const FString UseClientId = InClientId.IsEmpty() ? ClientId : InClientId;
	if (UseClientId.IsEmpty())
	{
		Callback(FDeviceAuthorizationResponse(), TEXT(""), TEXT("client_id is required"));
		return;
	}

	FString CodeVerifier;
	FString CodeChallenge;
	MakePkce(CodeVerifier, CodeChallenge);

	TMap<FString, FString> Form;
	Form.Add(TEXT("client_id"), UseClientId);
	Form.Add(TEXT("scope"), Scope.IsEmpty() ? FString(DefaultDeviceScope) : Scope);
	Form.Add(TEXT("code_challenge"), CodeChallenge);
	Form.Add(TEXT("code_challenge_method"), TEXT("S256"));

	ClientId = UseClientId;

	SendFormRequest(TEXT("/oauth/device_authorization"), Form,
		[Callback, CodeVerifier](int32 StatusCode, const FString& Body)
		{
			FDeviceAuthorizationResponse Response;
			if (StatusCode < 200 || StatusCode >= 300)
			{
				Callback(Response, TEXT(""),
					FString::Printf(TEXT("Server returned error status: %d, body: %s"), StatusCode, *Body));
				return;
			}

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			{
				Callback(Response, TEXT(""), TEXT("Invalid device_authorization response"));
				return;
			}

			Response.FromJson(Json);
			if (Response.DeviceCode.IsEmpty())
			{
				Callback(Response, TEXT(""), TEXT("Invalid device_authorization response"));
				return;
			}

			Callback(Response, CodeVerifier, TEXT(""));
		});
}

void UShangCloudApiClient::PollDeviceTokenOnce(const FString& DeviceCode, const FString& CodeVerifier,
	const FString& InClientId, FOnApiOAuthToken OnComplete)
{
	PollDeviceTokenOnceInternal(DeviceCode, CodeVerifier, InClientId,
		[OnComplete](const FOAuthTokenResponse& Response, const FString& Error)
		{
			OnComplete.ExecuteIfBound(Response, Error);
		});
}

void UShangCloudApiClient::PollDeviceTokenOnceInternal(const FString& DeviceCode, const FString& CodeVerifier,
	const FString& InClientId, TFunction<void(const FOAuthTokenResponse& Response, const FString& Error)> Callback)
{
	const FString UseClientId = InClientId.IsEmpty() ? ClientId : InClientId;
	if (UseClientId.IsEmpty() || DeviceCode.IsEmpty() || CodeVerifier.IsEmpty())
	{
		Callback(FOAuthTokenResponse(), TEXT("client_id, device_code and code_verifier are required"));
		return;
	}

	TMap<FString, FString> Form;
	Form.Add(TEXT("grant_type"), DeviceCodeGrantType);
	Form.Add(TEXT("device_code"), DeviceCode);
	Form.Add(TEXT("client_id"), UseClientId);
	Form.Add(TEXT("code_verifier"), CodeVerifier);

	TWeakObjectPtr<UShangCloudApiClient> WeakThis(this);
	SendFormRequest(TEXT("/oauth/token"), Form,
		[WeakThis, Callback](int32 StatusCode, const FString& Body)
		{
			FOAuthTokenResponse Response;
			if (StatusCode >= 200 && StatusCode < 300)
			{
				TSharedPtr<FJsonObject> Json;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
				if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
				{
					Response.FromJson(Json);
				}
				if (Response.AccessToken.IsEmpty())
				{
					Callback(Response,
						FString::Printf(TEXT("Server returned error status: %d, body: %s"), StatusCode, *Body));
					return;
				}
				if (WeakThis.IsValid())
				{
					WeakThis->ApplyTokenResponse(Response);
				}
				Callback(Response, TEXT(""));
				return;
			}

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			FString ErrorCode;
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				Json->TryGetStringField(TEXT("error"), ErrorCode);
			}
			if (ErrorCode == TEXT("authorization_pending") || ErrorCode == TEXT("slow_down"))
			{
				Callback(Response, ErrorCode);
				return;
			}

			Callback(Response,
				FString::Printf(TEXT("Server returned error status: %d, body: %s"), StatusCode, *Body));
		});
}

void UShangCloudApiClient::LoginWithDeviceAuth(const FString& InClientId, const FString& Scope,
	FOnApiDeviceUserCode OnUserCode, FOnApiOAuthToken OnComplete)
{
	CancelDeviceLogin();

	const FString UseClientId = InClientId.IsEmpty() ? ClientId : InClientId;
	if (UseClientId.IsEmpty())
	{
		OnComplete.ExecuteIfBound(FOAuthTokenResponse(), TEXT("client_id is required"));
		return;
	}

	DeviceLoginOnComplete = OnComplete;
	bDeviceLoginActive = true;

	TWeakObjectPtr<UShangCloudApiClient> WeakThis(this);
	RequestDeviceAuthorizationInternal(UseClientId, Scope,
		[WeakThis, OnUserCode](const FDeviceAuthorizationResponse& Response, const FString& CodeVerifier, const FString& Error)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			UShangCloudApiClient* Self = WeakThis.Get();
			if (!Self->bDeviceLoginActive)
			{
				return;
			}
			if (!Error.IsEmpty())
			{
				Self->bDeviceLoginActive = false;
				Self->DeviceLoginOnComplete.ExecuteIfBound(FOAuthTokenResponse(), Error);
				return;
			}

			Self->DeviceLoginDeviceCode = Response.DeviceCode;
			Self->DeviceLoginCodeVerifier = CodeVerifier;
			Self->DeviceLoginClientId = Self->ClientId;
			Self->DeviceLoginInterval = Response.Interval > 0 ? Response.Interval : 5;
			Self->DeviceLoginDeadlineSeconds = FPlatformTime::Seconds() + (Response.ExpiresIn > 0 ? Response.ExpiresIn : 900);

			OnUserCode.ExecuteIfBound(Response.UserCode, Response.VerificationUri, Response.VerificationUriComplete);
			Self->ScheduleDevicePoll();
		});
}

void UShangCloudApiClient::CancelDeviceLogin()
{
	bDeviceLoginActive = false;
	if (DeviceLoginTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeviceLoginTickerHandle);
		DeviceLoginTickerHandle.Reset();
	}
	DeviceLoginDeviceCode.Empty();
	DeviceLoginCodeVerifier.Empty();
}

void UShangCloudApiClient::RefreshAccessToken(const FString& InRefreshToken, const FString& InClientId, FOnApiOAuthToken OnComplete)
{
	const FString UseClientId = InClientId.IsEmpty() ? ClientId : InClientId;
	const FString UseRefresh = InRefreshToken.IsEmpty() ? RefreshToken : InRefreshToken;
	if (UseClientId.IsEmpty() || UseRefresh.IsEmpty())
	{
		OnComplete.ExecuteIfBound(FOAuthTokenResponse(), TEXT("client_id and refresh_token are required"));
		return;
	}

	TMap<FString, FString> Form;
	Form.Add(TEXT("grant_type"), TEXT("refresh_token"));
	Form.Add(TEXT("refresh_token"), UseRefresh);
	Form.Add(TEXT("client_id"), UseClientId);

	TWeakObjectPtr<UShangCloudApiClient> WeakThis(this);
	const FString PrevRefresh = UseRefresh;
	SendFormRequest(TEXT("/oauth/token"), Form,
		[WeakThis, OnComplete, PrevRefresh](int32 StatusCode, const FString& Body)
		{
			FOAuthTokenResponse Response;
			if (StatusCode < 200 || StatusCode >= 300)
			{
				OnComplete.ExecuteIfBound(Response,
					FString::Printf(TEXT("Server returned error status: %d, body: %s"), StatusCode, *Body));
				return;
			}

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				Response.FromJson(Json);
			}
			if (Response.AccessToken.IsEmpty())
			{
				OnComplete.ExecuteIfBound(Response, TEXT("Invalid refresh_token response"));
				return;
			}
			if (Response.RefreshToken.IsEmpty())
			{
				Response.RefreshToken = PrevRefresh;
			}
			if (WeakThis.IsValid())
			{
				WeakThis->ApplyTokenResponse(Response);
			}
			OnComplete.ExecuteIfBound(Response, TEXT(""));
		});
}

void UShangCloudApiClient::ScheduleDevicePoll()
{
	if (!bDeviceLoginActive)
	{
		return;
	}

	DeviceLoginNextPollSeconds = FPlatformTime::Seconds() + DeviceLoginInterval;

	if (!DeviceLoginTickerHandle.IsValid())
	{
		DeviceLoginTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UShangCloudApiClient::DeviceLoginTick), 0.25f);
	}
}

bool UShangCloudApiClient::DeviceLoginTick(float DeltaTime)
{
	if (!bDeviceLoginActive)
	{
		DeviceLoginTickerHandle.Reset();
		return false;
	}

	if (FPlatformTime::Seconds() < DeviceLoginNextPollSeconds)
	{
		return true;
	}

	DoDevicePoll();
	return bDeviceLoginActive;
}

void UShangCloudApiClient::DoDevicePoll()
{
	if (!bDeviceLoginActive)
	{
		return;
	}

	if (FPlatformTime::Seconds() >= DeviceLoginDeadlineSeconds)
	{
		bDeviceLoginActive = false;
		if (DeviceLoginTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(DeviceLoginTickerHandle);
			DeviceLoginTickerHandle.Reset();
		}
		DeviceLoginOnComplete.ExecuteIfBound(FOAuthTokenResponse(),
			TEXT("Device authorization timed out (device_code expired)"));
		return;
	}

	// Avoid overlapping polls while request is in flight
	DeviceLoginNextPollSeconds = FPlatformTime::Seconds() + DeviceLoginInterval;

	TWeakObjectPtr<UShangCloudApiClient> WeakThis(this);
	PollDeviceTokenOnceInternal(DeviceLoginDeviceCode, DeviceLoginCodeVerifier, DeviceLoginClientId,
		[WeakThis](const FOAuthTokenResponse& Response, const FString& Error)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			UShangCloudApiClient* Self = WeakThis.Get();
			if (!Self->bDeviceLoginActive)
			{
				return;
			}

			if (Error.IsEmpty() && !Response.AccessToken.IsEmpty())
			{
				Self->bDeviceLoginActive = false;
				if (Self->DeviceLoginTickerHandle.IsValid())
				{
					FTSTicker::GetCoreTicker().RemoveTicker(Self->DeviceLoginTickerHandle);
					Self->DeviceLoginTickerHandle.Reset();
				}
				Self->DeviceLoginOnComplete.ExecuteIfBound(Response, TEXT(""));
				return;
			}

			if (Error == TEXT("authorization_pending"))
			{
				Self->ScheduleDevicePoll();
				return;
			}
			if (Error == TEXT("slow_down"))
			{
				Self->DeviceLoginInterval += 5;
				Self->ScheduleDevicePoll();
				return;
			}

			Self->bDeviceLoginActive = false;
			if (Self->DeviceLoginTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(Self->DeviceLoginTickerHandle);
				Self->DeviceLoginTickerHandle.Reset();
			}
			Self->DeviceLoginOnComplete.ExecuteIfBound(Response, Error);
		});
}

void UShangCloudApiClient::ApplyTokenResponse(const FOAuthTokenResponse& Token)
{
	AccessToken = Token.AccessToken;
	if (!Token.TokenType.IsEmpty())
	{
		TokenType = Token.TokenType;
	}
	if (!Token.RefreshToken.IsEmpty())
	{
		RefreshToken = Token.RefreshToken;
	}
}

void UShangCloudApiClient::MakePkce(FString& OutVerifier, FString& OutChallenge)
{
	TArray<uint8> RandomBytes;
	RandomBytes.SetNumUninitialized(32);
	if (RAND_bytes(RandomBytes.GetData(), RandomBytes.Num()) != 1)
	{
		for (int32 i = 0; i < RandomBytes.Num(); ++i)
		{
			RandomBytes[i] = static_cast<uint8>(FMath::RandHelper(256));
		}
	}

	OutVerifier = Base64UrlEncode(RandomBytes);

	FTCHARToUTF8 VerifierUtf8(*OutVerifier);
	unsigned char Hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(VerifierUtf8.Get()), VerifierUtf8.Length(), Hash);

	TArray<uint8> HashBytes;
	HashBytes.Append(Hash, SHA256_DIGEST_LENGTH);
	OutChallenge = Base64UrlEncode(HashBytes);
}

FString UShangCloudApiClient::Base64UrlEncode(const TArray<uint8>& Data)
{
	FString Encoded = FBase64::Encode(Data);
	Encoded.ReplaceInline(TEXT("+"), TEXT("-"));
	Encoded.ReplaceInline(TEXT("/"), TEXT("_"));
	Encoded.ReplaceInline(TEXT("="), TEXT(""));
	return Encoded;
}

FString UShangCloudApiClient::UrlEncodeForm(const TMap<FString, FString>& FormFields)
{
	FString Result;
	bool bFirst = true;
	for (const TPair<FString, FString>& Pair : FormFields)
	{
		if (!bFirst)
		{
			Result += TEXT("&");
		}
		bFirst = false;
		Result += FGenericPlatformHttp::UrlEncode(Pair.Key);
		Result += TEXT("=");
		Result += FGenericPlatformHttp::UrlEncode(Pair.Value);
	}
	return Result;
}

void UShangCloudApiClient::SendFormRequest(const FString& Path, const TMap<FString, FString>& FormFields,
	TFunction<void(int32 StatusCode, const FString& ResponseBody)> Callback)
{
	FString Url = BaseUrl + Path;
	FString Body = UrlEncodeForm(FormFields);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(Body);
	Request->SetTimeout(10.f);

	Request->OnProcessRequestComplete().BindLambda(
		[Callback](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Resp.IsValid())
			{
				Callback(0, TEXT("Request failed: no connection"));
				return;
			}
			Callback(Resp->GetResponseCode(), Resp->GetContentAsString());
		});

	Request->ProcessRequest();
}

void UShangCloudApiClient::SendRequest(const FString& Path, const FString& JsonBody,
	const FString& RoomId, const FString& Protocol,
	TFunction<void(const FString& ResponseBody, const FString& Error)> Callback)
{
	FString Url = BaseUrl + Path;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("%s %s"), *TokenType, *AccessToken));

	if (!RoomId.IsEmpty())
	{
		Request->SetHeader(TEXT("X-MMO-Room"), RoomId);
	}
	if (!Protocol.IsEmpty())
	{
		Request->SetHeader(TEXT("X-MMO-Protoctl"), Protocol);
	}

	Request->SetContentAsString(JsonBody);
	Request->SetTimeout(10.f);

	Request->OnProcessRequestComplete().BindLambda(
		[Callback](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully || !Resp.IsValid())
			{
				Callback(TEXT(""), TEXT("Request failed: no connection"));
				return;
			}

			int32 StatusCode = Resp->GetResponseCode();
			FString Body = Resp->GetContentAsString();

			if (StatusCode < 200 || StatusCode >= 300)
			{
				Callback(Body, FString::Printf(TEXT("Server returned error status: %d, body: %s"),
					StatusCode, *Body));
				return;
			}

			Callback(Body, TEXT(""));
		});

	Request->ProcessRequest();
}
