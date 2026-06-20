#pragma once

#include "CoreMinimal.h"

class SHANGCLOUDMMO_API FMmoCrypto
{
public:
	static constexpr int32 SeedSize = 32;
	static constexpr int32 KeySize = 32;
	static constexpr int32 NonceSize = 12;
	static constexpr int32 TagSize = 16;
	static constexpr int32 TimestampSize = 8;
	static constexpr int32 MinPacketSize = NonceSize + TimestampSize + TagSize; // 36

	static TArray<uint8> GenerateSeed();
	static TArray<uint8> DeriveKey(const TArray<uint8>& Seed);

	/**
	 * Encrypts data with AES-256-GCM.
	 * Output: [12B nonce][ciphertext(8B timestamp + data)][16B tag]
	 * @return Encrypted bytes, or empty array on failure.
	 */
	static TArray<uint8> Encrypt(const TArray<uint8>& Key, const uint8* Data, int32 DataLen);

	/**
	 * Decrypts an AES-256-GCM packet. Strips 8B timestamp.
	 * @return Decrypted data bytes, or empty array on failure.
	 */
	static TArray<uint8> Decrypt(const TArray<uint8>& Key, const uint8* Packet, int32 PacketLen);

	static int32 GetEncryptedSize(int32 DataLength);

	// Big-endian helpers
	static void WriteU32BE(uint8* Dst, uint32 Value);
	static void WriteU64BE(uint8* Dst, uint64 Value);
	static uint32 ReadU32BE(const uint8* Src);
	static uint64 ReadU64BE(const uint8* Src);
};
