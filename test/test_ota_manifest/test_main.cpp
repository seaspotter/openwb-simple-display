#include <unity.h>

#include "ota/ota_manifest.h"

using ota::Manifest;
using ota::ParseManifestJson;

void setUp() {}
void tearDown() {}

void test_valid_manifest_parses() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";
    doc["min_supported_version"] = "1.0.0";
    doc["firmware"]["url"] = "https://example.github.io/openwb-display/firmware/fw.bin";
    doc["firmware"]["sha256"] = "deadbeef";
    doc["firmware"]["size_bytes"] = 123456;

    Manifest manifest;
    TEST_ASSERT_TRUE(ParseManifestJson(doc, manifest));
    TEST_ASSERT_EQUAL_STRING("1.2.0", manifest.latest_version);
    TEST_ASSERT_EQUAL_UINT32(123456, manifest.firmware_size_bytes);
}

void test_manifest_missing_firmware_url_is_rejected() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";

    Manifest manifest;
    TEST_ASSERT_FALSE(ParseManifestJson(doc, manifest));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_manifest_parses);
    RUN_TEST(test_manifest_missing_firmware_url_is_rejected);
    return UNITY_END();
}
