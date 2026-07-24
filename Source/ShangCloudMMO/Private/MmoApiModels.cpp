#include "MmoApiModels.h"
#include "Dom/JsonObject.h"

void FMmoNewRoomResponse::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid()) return;
	Json->TryGetStringField(TEXT("connect_key"), ConnectKey);
	Json->TryGetStringField(TEXT("edge_url"), EdgeUrl);
	Json->TryGetStringField(TEXT("room_id"), RoomId);
	Json->TryGetStringField(TEXT("protocol"), Protocol);
}

void FMmoJoinRoomResponse::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid()) return;
	Json->TryGetStringField(TEXT("connect_key"), ConnectKey);
	Json->TryGetStringField(TEXT("edge_url"), EdgeUrl);
	Json->TryGetStringField(TEXT("room_id"), RoomId);
	Json->TryGetStringField(TEXT("protocol"), Protocol);
	Json->TryGetStringField(TEXT("assigned_uid"), AssignedUid);
}

void FDeviceAuthorizationResponse::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid()) return;
	Json->TryGetStringField(TEXT("device_code"), DeviceCode);
	Json->TryGetStringField(TEXT("user_code"), UserCode);
	Json->TryGetStringField(TEXT("verification_uri"), VerificationUri);
	Json->TryGetStringField(TEXT("verification_uri_complete"), VerificationUriComplete);
	double Expires = 900.0;
	double Intv = 5.0;
	if (Json->TryGetNumberField(TEXT("expires_in"), Expires))
	{
		ExpiresIn = static_cast<int32>(Expires);
	}
	if (Json->TryGetNumberField(TEXT("interval"), Intv))
	{
		Interval = static_cast<int32>(Intv);
	}
	if (ExpiresIn <= 0) ExpiresIn = 900;
	if (Interval <= 0) Interval = 5;
}

void FOAuthTokenResponse::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	if (!Json.IsValid()) return;
	Json->TryGetStringField(TEXT("access_token"), AccessToken);
	Json->TryGetStringField(TEXT("token_type"), TokenType);
	Json->TryGetStringField(TEXT("refresh_token"), RefreshToken);
	Json->TryGetStringField(TEXT("scope"), Scope);
	Json->TryGetStringField(TEXT("id_token"), IdToken);
	double Exp = 0.0;
	if (Json->TryGetNumberField(TEXT("expires_in"), Exp))
	{
		ExpiresIn = static_cast<int32>(Exp);
	}
}
