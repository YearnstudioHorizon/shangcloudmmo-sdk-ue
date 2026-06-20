#include "MmoTransport.h"
#include "MmoCrypto.h"

TArray<uint8> FMmoTransport::EncryptData(const uint8* Data, int32 Length)
{
	return FMmoCrypto::Encrypt(AesKey, Data, Length);
}

TArray<uint8> FMmoTransport::DecryptData(const uint8* Encrypted, int32 Length)
{
	return FMmoCrypto::Decrypt(AesKey, Encrypted, Length);
}

void FMmoTransport::ProcessDecryptedMessage(const TArray<uint8>& Decrypted)
{
	if (Decrypted.Num() == 0) return;

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Decrypted.GetData()), Decrypted.Num());
	FString Msg(Converter.Length(), Converter.Get());

	if (Msg == TEXT("__auth_ok__"))
	{
		State = EMmoConnectionState::Connected;
		OnConnected.ExecuteIfBound();
		return;
	}

	if (Msg == TEXT("__hb__"))
		return;

	if (Msg == TEXT("__closed__"))
	{
		State = EMmoConnectionState::Disconnected;
		OnServerClosed.ExecuteIfBound();
		OnDisconnected.ExecuteIfBound();
		return;
	}

	if (MessageQueue)
	{
		MessageQueue->Enqueue(FMmoMessage::CreateText(Msg));
	}
}

void FMmoTransport::SendHeartbeat()
{
	static const uint8 HbData[] = { '_', '_', 'h', 'b', '_', '_' };
	Send(HbData, sizeof(HbData));
}
