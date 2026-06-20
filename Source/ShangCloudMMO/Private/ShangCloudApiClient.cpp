#include "ShangCloudApiClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
