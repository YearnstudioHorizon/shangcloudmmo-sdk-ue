#include "ShangCloudMmoComponent.h"
#include "MmoTcpTransport.h"
#include "MmoUdpTransport.h"
#include "MmoWebSocketTransport.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

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
	InterpEngine.Clear();
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

void UShangCloudMmoComponent::SendBroadcast(const FString& Uid, const FString& Message, const FString& Extra)
{
	if (!Transport.IsValid() || Transport->GetState() != EMmoConnectionState::Connected)
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: cannot send broadcast, not connected"));
		return;
	}

	// wire 格式（参考 core.js 的 sendMmoMessage）：{"uid","message","extra"}，无 type 字段
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("uid"), Uid);
	Json->SetStringField(TEXT("message"), Message);
	Json->SetStringField(TEXT("extra"), Extra);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	SendMessage(Out);
}

void UShangCloudMmoComponent::SendSyncVar(const FString& Uid, const TMap<FString, FString>& Vars, const TArray<FString>& Interp)
{
	if (!Transport.IsValid() || Transport->GetState() != EMmoConnectionState::Connected)
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: cannot send sync_var, not connected"));
		return;
	}

	// wire 格式（参考 core.js 的 __sync_var__）：{"type":"__sync_var__","uid","vars","interp"}
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("__sync_var__"));
	Json->SetStringField(TEXT("uid"), Uid);

	TSharedPtr<FJsonObject> VarsJson = MakeShared<FJsonObject>();
	for (const TTuple<FString, FString>& Pair : Vars)
	{
		VarsJson->SetStringField(Pair.Key, Pair.Value);
	}
	Json->SetObjectField(TEXT("vars"), VarsJson);

	TArray<TSharedPtr<FJsonValue>> InterpValues;
	InterpValues.Reserve(Interp.Num());
	for (const FString& Name : Interp)
	{
		InterpValues.Add(MakeShared<FJsonValueString>(Name));
	}
	Json->SetArrayField(TEXT("interp"), InterpValues);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	SendMessage(Out);
}

void UShangCloudMmoComponent::SendJoinAnnouncement(const FString& Uid, const FString& Nickname)
{
	if (!Transport.IsValid() || Transport->GetState() != EMmoConnectionState::Connected)
	{
		UE_LOG(LogTemp, Error, TEXT("ShangCloudMMO: cannot send join announcement, not connected"));
		return;
	}

	// wire 格式：{"type":"__join__","uid","nickname"}
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("type"), TEXT("__join__"));
	Json->SetStringField(TEXT("uid"), Uid);
	Json->SetStringField(TEXT("nickname"), Nickname);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	SendMessage(Out);
}

double UShangCloudMmoComponent::GetSyncVar(const FString& Uid, const FString& VarName) const
{
	return InterpEngine.GetSyncVar(Uid, VarName);
}

FString UShangCloudMmoComponent::GetSyncVarRaw(const FString& Uid, const FString& VarName) const
{
	return InterpEngine.GetSyncVarRaw(Uid, VarName);
}

void UShangCloudMmoComponent::ClearSyncVarState(const FString& Uid)
{
	InterpEngine.ClearUid(Uid);
}

void UShangCloudMmoComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 先推进插帧引擎（收到 sync_var 后逐帧把 current → target）
	TArray<FMmoInterpEngine::FChange> InterpChanges = InterpEngine.Tick(DeltaTime);
	for (const FMmoInterpEngine::FChange& C : InterpChanges)
	{
		OnSyncVarInterpolated.Broadcast(C.Uid, C.VarName, C.Value);
	}

	if (!Transport.IsValid())
	{
		return;
	}

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
			Json->TryGetStringField(TEXT("type"), Type);

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
				InterpEngine.ClearUid(Uid);
				OnUserLeft.Broadcast(Uid);
				return;
			}
			if (Type == TEXT("__sync_var__"))
			{
				// 同步变量：{"type":"__sync_var__","uid","vars","interp"}
				FMmoSyncVarPayload Payload;
				Json->TryGetStringField(TEXT("uid"), Payload.Uid);

				const TSharedPtr<FJsonObject>* VarsObj;
				if (Json->TryGetObjectField(TEXT("vars"), VarsObj) && VarsObj->IsValid())
				{
					for (const auto& Pair : (*VarsObj)->Values)
					{
						if (Pair.Value.IsValid())
						{
							Payload.Vars.Add(Pair.Key, Pair.Value->AsString());
						}
					}
				}

				const TArray<TSharedPtr<FJsonValue>>* InterpArr;
				if (Json->TryGetArrayField(TEXT("interp"), InterpArr))
				{
					for (const TSharedPtr<FJsonValue>& Item : *InterpArr)
					{
						if (Item.IsValid())
						{
							Payload.Interp.Add(Item->AsString());
						}
					}
				}

				// 先喂给插帧引擎（数值且在 interp 中的变量会进入平滑状态）
				InterpEngine.ApplySync(Payload.Uid, Payload.Vars, &Payload.Interp);
				OnSyncVarReceived.Broadcast(Payload);
				return;
			}

			// 广播消息（参考 core.js 的普通消息处理）：{"uid","message","extra"}，无 type 字段
			if (Type.IsEmpty())
			{
				const TSharedPtr<FJsonValue>* MsgVal = Json->Values.Find(TEXT("message"));
				if (MsgVal != nullptr)
				{
					FString Uid, Body, Extra;
					Json->TryGetStringField(TEXT("uid"), Uid);
					Json->TryGetStringField(TEXT("message"), Body);
					Json->TryGetStringField(TEXT("extra"), Extra);
					OnBroadcastReceived.Broadcast(Uid, Body, Extra);
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
