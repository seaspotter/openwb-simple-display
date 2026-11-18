// Public half of the Ed25519 release-signing key; ota_manager.cpp verifies
// firmware.signature against it before flashing. Private key lives only as the
// OTA_SIGNING_PRIVATE_KEY GitHub Actions secret. To rotate:
//   openssl genpkey -algorithm ed25519 -out ota_signing_private.pem
//   openssl pkey -in ota_signing_private.pem -pubout -outform DER | tail -c 32 | xxd -i
#pragma once

#include <cstdint>

inline constexpr uint8_t kOtaSigningPublicKey[32] = {
    0xdb, 0x90, 0xf3, 0xd8, 0xb4, 0xd3, 0x7a, 0x9c, 0xce, 0x97, 0x95, 0x04, 0x54, 0x75, 0x41, 0xaa,
    0x77, 0x3a, 0x7e, 0x4e, 0x47, 0x8f, 0x01, 0xd0, 0x58, 0x35, 0xfb, 0xbb, 0x7a, 0xfa, 0x52, 0xe2,
};
