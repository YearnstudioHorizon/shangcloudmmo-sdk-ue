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
