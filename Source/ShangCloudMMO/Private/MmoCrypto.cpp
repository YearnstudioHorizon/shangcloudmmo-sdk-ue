#include "MmoCrypto.h"

THIRD_PARTY_INCLUDES_START
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

TArray<uint8> FMmoCrypto::GenerateSeed()
{
	TArray<uint8> Seed;
	Seed.SetNumUninitialized(SeedSize);
	RAND_bytes(Seed.GetData(), SeedSize);
	return Seed;
}

TArray<uint8> FMmoCrypto::DeriveKey(const TArray<uint8>& Seed)
{
	TArray<uint8> Key;
	Key.SetNumUninitialized(KeySize);
	SHA256(Seed.GetData(), Seed.Num(), Key.GetData());
	return Key;
}

int32 FMmoCrypto::GetEncryptedSize(int32 DataLength)
{
	return NonceSize + TimestampSize + DataLength + TagSize;
}

TArray<uint8> FMmoCrypto::Encrypt(const TArray<uint8>& Key, const uint8* Data, int32 DataLen)
{
	TArray<uint8> Result;

	if (Key.Num() != KeySize)
	{
		UE_LOG(LogTemp, Error, TEXT("MmoCrypto::Encrypt: key must be 32 bytes"));
		return Result;
	}

	int32 PtLen = TimestampSize + DataLen;
	int32 TotalLen = NonceSize + PtLen + TagSize;

	Result.SetNumUninitialized(TotalLen);
	uint8* Out = Result.GetData();

	// [0..12) = nonce
	RAND_bytes(Out, NonceSize);

	// Build plaintext: [8B timestamp BE][data]
	TArray<uint8> Plaintext;
	Plaintext.SetNumUninitialized(PtLen);
	uint8* Pt = Plaintext.GetData();

	int64 Ms = FDateTime::UtcNow().ToUnixTimestamp() * 1000LL +
		FDateTime::UtcNow().GetMillisecond();
	WriteU64BE(Pt, static_cast<uint64>(Ms));

	if (DataLen > 0)
	{
		FMemory::Memcpy(Pt + TimestampSize, Data, DataLen);
	}

	// AES-256-GCM encrypt using OpenSSL EVP
	EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
	if (!Ctx)
	{
		Result.Reset();
		return Result;
	}

	int32 Len = 0;
	bool bSuccess = true;

	if (EVP_EncryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
		bSuccess = false;

	if (bSuccess && EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_IVLEN, NonceSize, nullptr) != 1)
		bSuccess = false;

	if (bSuccess && EVP_EncryptInit_ex(Ctx, nullptr, nullptr, Key.GetData(), Out) != 1)
		bSuccess = false;

	// Ciphertext output at Out[NonceSize..NonceSize+PtLen)
	if (bSuccess && EVP_EncryptUpdate(Ctx, Out + NonceSize, &Len, Pt, PtLen) != 1)
		bSuccess = false;

	int32 FinalLen = 0;
	if (bSuccess && EVP_EncryptFinal_ex(Ctx, Out + NonceSize + Len, &FinalLen) != 1)
		bSuccess = false;

	// Tag at Out[NonceSize+PtLen..NonceSize+PtLen+TagSize)
	if (bSuccess && EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_GET_TAG, TagSize, Out + NonceSize + PtLen) != 1)
		bSuccess = false;

	EVP_CIPHER_CTX_free(Ctx);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("MmoCrypto::Encrypt: AES-GCM encryption failed"));
		Result.Reset();
	}

	return Result;
}

TArray<uint8> FMmoCrypto::Decrypt(const TArray<uint8>& Key, const uint8* Packet, int32 PacketLen)
{
	TArray<uint8> Result;

	if (Key.Num() != KeySize)
	{
		UE_LOG(LogTemp, Error, TEXT("MmoCrypto::Decrypt: key must be 32 bytes"));
		return Result;
	}

	if (PacketLen < MinPacketSize)
	{
		UE_LOG(LogTemp, Error, TEXT("MmoCrypto::Decrypt: packet too short"));
		return Result;
	}

	const uint8* Nonce = Packet;
	int32 CtLen = PacketLen - NonceSize - TagSize;
	const uint8* Ciphertext = Packet + NonceSize;
	const uint8* Tag = Packet + PacketLen - TagSize;

	TArray<uint8> Plaintext;
	Plaintext.SetNumUninitialized(CtLen);

	EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
	if (!Ctx)
	{
		return Result;
	}

	int32 Len = 0;
	bool bSuccess = true;

	if (EVP_DecryptInit_ex(Ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
		bSuccess = false;

	if (bSuccess && EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_IVLEN, NonceSize, nullptr) != 1)
		bSuccess = false;

	if (bSuccess && EVP_DecryptInit_ex(Ctx, nullptr, nullptr, Key.GetData(), Nonce) != 1)
		bSuccess = false;

	if (bSuccess && EVP_DecryptUpdate(Ctx, Plaintext.GetData(), &Len, Ciphertext, CtLen) != 1)
		bSuccess = false;

	// Set expected tag
	if (bSuccess && EVP_CIPHER_CTX_ctrl(Ctx, EVP_CTRL_GCM_SET_TAG, TagSize, const_cast<uint8*>(Tag)) != 1)
		bSuccess = false;

	int32 FinalLen = 0;
	if (bSuccess)
	{
		// EVP_DecryptFinal_ex returns 0 if tag verification fails
		if (EVP_DecryptFinal_ex(Ctx, Plaintext.GetData() + Len, &FinalLen) <= 0)
			bSuccess = false;
	}

	EVP_CIPHER_CTX_free(Ctx);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("MmoCrypto::Decrypt: GCM auth/decrypt failed"));
		return Result;
	}

	// Strip 8-byte timestamp prefix
	if (CtLen <= TimestampSize)
	{
		return Result;
	}

	int32 DataLen = CtLen - TimestampSize;
	Result.SetNumUninitialized(DataLen);
	FMemory::Memcpy(Result.GetData(), Plaintext.GetData() + TimestampSize, DataLen);
	return Result;
}

void FMmoCrypto::WriteU32BE(uint8* Dst, uint32 Value)
{
	Dst[0] = static_cast<uint8>(Value >> 24);
	Dst[1] = static_cast<uint8>(Value >> 16);
	Dst[2] = static_cast<uint8>(Value >> 8);
	Dst[3] = static_cast<uint8>(Value);
}

void FMmoCrypto::WriteU64BE(uint8* Dst, uint64 Value)
{
	Dst[0] = static_cast<uint8>(Value >> 56);
	Dst[1] = static_cast<uint8>(Value >> 48);
	Dst[2] = static_cast<uint8>(Value >> 40);
	Dst[3] = static_cast<uint8>(Value >> 32);
	Dst[4] = static_cast<uint8>(Value >> 24);
	Dst[5] = static_cast<uint8>(Value >> 16);
	Dst[6] = static_cast<uint8>(Value >> 8);
	Dst[7] = static_cast<uint8>(Value);
}

uint32 FMmoCrypto::ReadU32BE(const uint8* Src)
{
	return (static_cast<uint32>(Src[0]) << 24) |
		   (static_cast<uint32>(Src[1]) << 16) |
		   (static_cast<uint32>(Src[2]) << 8) |
		   (static_cast<uint32>(Src[3]));
}

uint64 FMmoCrypto::ReadU64BE(const uint8* Src)
{
	return (static_cast<uint64>(Src[0]) << 56) |
		   (static_cast<uint64>(Src[1]) << 48) |
		   (static_cast<uint64>(Src[2]) << 40) |
		   (static_cast<uint64>(Src[3]) << 32) |
		   (static_cast<uint64>(Src[4]) << 24) |
		   (static_cast<uint64>(Src[5]) << 16) |
		   (static_cast<uint64>(Src[6]) << 8) |
		   (static_cast<uint64>(Src[7]));
}
