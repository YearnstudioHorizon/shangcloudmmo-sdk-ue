#include "ShangCloudMmoComponent.h"
#include "MmoTcpTransport.h"
#include "MmoUdpTransport.h"
#include "MmoWebSocketTransport.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UShangCloudMmoComponent::UShangCloudMmoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UShangCloudMmoComponent::ConfigureFromApiResponse(const FString& InConnectKey,
	const FString& InEdgeUrl, const FString& InProtocol)
{
	ConnectKey = InConnectKey;
	EdgeUrl = InEdgeUrl;

	FString ProtoLower = InProtocol.ToLower();
	if (ProtoLower == TEXT("websocket") || ProtoLower == TEXT("ws"))
	{
		Protocol = EMmoProtocol::WebSocket;
	}
	else if (ProtoLower == TEXT("udp"))
	{
		Protocol = EMmoProtocol::UDP;
	}
	else
	{
		Protocol = EMmoProtocol::TCP;
	}

	ParseEdgeUrl(InEdgeUrl);
}

void UShangCloudMmoComponent::ConnectToEdge()
{
	if (ConnectKey.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: connect_key must be set before connecting"));
		return;
	}

	CleanupTransport();

	switch (Protocol)
	{
	case EMmoProtocol::TCP:
		Transport = MakeUnique<FMmoTcpTransport>();
		break;
	case EMmoProtocol::UDP:
		Transport = MakeUnique<FMmoUdpTransport>();
		break;
	case EMmoProtocol::WebSocket:
		Transport = MakeUnique<FMmoWebSocketTransport>();
		break;
	}

	Transport->SetMessageQueue(&MessageQueue);

	Transport->OnConnected.BindLambda([this]()
	{
		ConnectionState = EMmoConnectionState::Connected;
		OnConnected.Broadcast();
	});
	Transport->OnDisconnected.BindLambda([this]()
	{
		ConnectionState = EMmoConnectionState::Disconnected;
		OnDisconnected.Broadcast();
	});
	Transport->OnError.BindLambda([this](const FString& Error)
	{
		ConnectionState = EMmoConnectionState::Error;
		OnConnectionError.Broadcast(Error);
	});
	Transport->OnServerClosed.BindLambda([this]()
	{
		OnServerClosed.Broadcast();
	});

	if (Protocol == EMmoProtocol::WebSocket && !EdgeUrl.IsEmpty())
	{
		static_cast<FMmoWebSocketTransport*>(Transport.Get())->ConnectWithUrl(EdgeUrl, ConnectKey);
	}
	else
	{
		if (EdgeHost.IsEmpty() || EdgePort <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: EdgeHost and EdgePort must be set before connecting"));
			return;
		}
		Transport->Connect(EdgeHost, EdgePort, ConnectKey);
	}

	ConnectionState = Transport->GetState();
}

void UShangCloudMmoComponent::DisconnectFromEdge()
{
	if (Transport.IsValid())
	{
		Transport->Disconnect();
	}
	ConnectionState = EMmoConnectionState::Disconnected;
}

void UShangCloudMmoComponent::SendMessage(const FString& Message)
{
	if (!Transport.IsValid() || Transport->GetState() != EMmoConnectionState::Connected)
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: cannot send message, not connected"));
		return;
	}

	FTCHARToUTF8 Utf8(*Message);
	Transport->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

void UShangCloudMmoComponent::SendRaw(const TArray<uint8>& Data)
{
	if (!Transport.IsValid() || Transport->GetState() != EMmoConnectionState::Connected)
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: cannot send data, not connected"));
		return;
	}

	Transport->Send(Data.GetData(), Data.Num());
}

void UShangCloudMmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Transport.IsValid()) return;

	Transport->Poll(DeltaTime);

	// Update state
	ConnectionState = Transport->GetState();

	// Drain message queue
	TArray<FMmoMessage> Messages = MessageQueue.DrainAll();
	for (const FMmoMessage& Msg : Messages)
	{
		switch (Msg.Type)
		{
		case FMmoMessage::Text:
			ProcessBusinessMessage(Msg.TextData);
			break;
		case FMmoMessage::Binary:
			OnRawMessageReceived.Broadcast(Msg.BinaryData);
			break;
		}
	}
}

void UShangCloudMmoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupTransport();
	Super::EndPlay(EndPlayReason);
}

void UShangCloudMmoComponent::CleanupTransport()
{
	if (Transport.IsValid())
	{
		Transport->Disconnect();
		Transport.Reset();
	}
}

void UShangCloudMmoComponent::ProcessBusinessMessage(const FString& Message)
{
	if (Message.Len() > 0 && Message[0] == TEXT('{'))
	{
		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
		{
			FString Type;
			if (Json->TryGetStringField(TEXT("type"), Type))
			{
				if (Type == TEXT("__join__"))
				{
					FString Uid, Nickname;
					Json->TryGetStringField(TEXT("uid"), Uid);
					Json->TryGetStringField(TEXT("nickname"), Nickname);
					OnUserJoined.Broadcast(Uid, Nickname);
					return;
				}
				if (Type == TEXT("__leave__"))
				{
					FString Uid;
					Json->TryGetStringField(TEXT("uid"), Uid);
					OnUserLeft.Broadcast(Uid);
					return;
				}
			}
		}
	}

	OnMessageReceived.Broadcast(Message);
}

void UShangCloudMmoComponent::ParseEdgeUrl(const FString& Url)
{
	if (Url.IsEmpty()) return;

	if (Url.StartsWith(TEXT("ws://")) || Url.StartsWith(TEXT("wss://")))
	{
		EdgeUrl = Url;
		// Also extract host:port
		FString HostPort = Url;
		HostPort.RemoveFromStart(TEXT("ws://"));
		HostPort.RemoveFromStart(TEXT("wss://"));
		int32 SlashIdx;
		if (HostPort.FindChar(TEXT('/'), SlashIdx))
			HostPort.LeftInline(SlashIdx);
		int32 ColonIdx;
		if (HostPort.FindLastChar(TEXT(':'), ColonIdx))
		{
			EdgeHost = HostPort.Left(ColonIdx);
			EdgePort = FCString::Atoi(*HostPort.Mid(ColonIdx + 1));
		}
		return;
	}

	FString HostPort = Url;
	// Strip protocol prefix
	int32 SchemeEnd;
	if (HostPort.FindChar(TEXT('/'), SchemeEnd) && SchemeEnd > 0 &&
		HostPort[SchemeEnd - 1] == TEXT(':') && HostPort.Len() > SchemeEnd + 1 && HostPort[SchemeEnd + 1] == TEXT('/'))
	{
		HostPort = HostPort.Mid(SchemeEnd + 2);
	}

	int32 ColonIdx;
	if (HostPort.FindLastChar(TEXT(':'), ColonIdx))
	{
		EdgeHost = HostPort.Left(ColonIdx);
		EdgePort = FCString::Atoi(*HostPort.Mid(ColonIdx + 1));
	}
	else
	{
		EdgeHost = HostPort;
	}
}
