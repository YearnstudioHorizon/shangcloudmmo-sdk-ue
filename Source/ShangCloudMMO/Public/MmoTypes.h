#pragma once

#include "CoreMinimal.h"
#include "MmoTypes.generated.h"

UENUM(BlueprintType)
enum class EMmoConnectionState : uint8
{
	Idle,
	Connecting,
	Handshake,
	Authenticating,
	Connected,
	Disconnected,
	Error
};

UENUM(BlueprintType)
enum class EMmoProtocol : uint8
{
	TCP,
	UDP,
	WebSocket
};

struct FMmoMessage
{
	enum EType { Text, Binary };

	EType Type;
	FString TextData;
	TArray<uint8> BinaryData;

	static FMmoMessage CreateText(const FString& InText)
	{
		FMmoMessage Msg;
		Msg.Type = Text;
		Msg.TextData = InText;
		return Msg;
	}

	static FMmoMessage CreateBinary(TArray<uint8>&& InData)
	{
		FMmoMessage Msg;
		Msg.Type = Binary;
		Msg.BinaryData = MoveTemp(InData);
		return Msg;
	}
};
