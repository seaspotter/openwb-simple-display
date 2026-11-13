// Public half of the Ed25519 release-signing key; ota_manager.cpp verifies
// firmware.signature against it before flashing. Private key lives only as the
// OTA_SIGNING_PRIVATE_KEY GitHub Actions secret. To rotate:
//   openssl genpkey -algorithm ed25519 -out ota_signing_private.pem
//   openssl pkey -in ota_signing_private.pem -pubout -outform DER | tail -c 32 | xxd -i
#pragma once

#include <cstdint>

inline constexpr uint8_t kOtaSigningPublicKey[32] = {
    0x9f, 0x99, 0x59, 0xc9, 0x31, 0x5f, 0x04, 0xd3, 0x88, 0x93, 0xde, 0x98, 0x63, 0x5b, 0x97, 0xd1,
    0x98, 0xf3, 0x8b, 0xa9, 0x2c, 0xe8, 0x4e, 0xbf, 0x74, 0x5f, 0x01, 0xa5, 0x17, 0x25, 0x77, 0x91,
};
