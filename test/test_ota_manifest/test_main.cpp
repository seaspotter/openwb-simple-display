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
    doc["firmware"]["signature"] = "c0ffee";
    doc["firmware"]["size_bytes"] = 123456;

    Manifest manifest;
    TEST_ASSERT_TRUE(ParseManifestJson(doc, manifest));
    TEST_ASSERT_EQUAL_STRING("1.2.0", manifest.latest_version);
    TEST_ASSERT_EQUAL_STRING("c0ffee", manifest.firmware_signature);
    TEST_ASSERT_EQUAL_UINT32(123456, manifest.firmware_size_bytes);
}

// Rejecting a missing signature is StartInstall()'s job, not ParseManifestJson()'s.
void test_manifest_without_signature_still_parses_but_field_is_empty() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";
    doc["firmware"]["url"] = "https://example.github.io/openwb-display/firmware/fw.bin";

    Manifest manifest;
    TEST_ASSERT_TRUE(ParseManifestJson(doc, manifest));
    TEST_ASSERT_EQUAL_STRING("", manifest.firmware_signature);
}

void test_manifest_missing_firmware_url_is_rejected() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";

    Manifest manifest;
    TEST_ASSERT_FALSE(ParseManifestJson(doc, manifest));
}

void test_filesystem_field_parses_when_present() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";
    doc["firmware"]["url"] = "https://example.github.io/openwb-display/firmware/fw.bin";
    doc["filesystem"]["url"] = "https://example.github.io/openwb-display/firmware/littlefs.bin";
    doc["filesystem"]["sha256"] = "abc123";
    doc["filesystem"]["signature"] = "sig456";
    doc["filesystem"]["size_bytes"] = 654321;

    Manifest manifest;
    TEST_ASSERT_TRUE(ParseManifestJson(doc, manifest));
    TEST_ASSERT_EQUAL_STRING("https://example.github.io/openwb-display/firmware/littlefs.bin",
                              manifest.filesystem_url);
    TEST_ASSERT_EQUAL_STRING("abc123", manifest.filesystem_sha256);
    TEST_ASSERT_EQUAL_UINT32(654321, manifest.filesystem_size_bytes);
}

// A manifest without "filesystem" must still parse successfully — ota_manager.cpp
// treats an empty filesystem_url as "skip that step", not a parse failure.
void test_filesystem_field_absent_leaves_empty_fields() {
    JsonDocument doc;
    doc["latest_version"] = "1.2.0";
    doc["firmware"]["url"] = "https://example.github.io/openwb-display/firmware/fw.bin";

    Manifest manifest;
    TEST_ASSERT_TRUE(ParseManifestJson(doc, manifest));
    TEST_ASSERT_EQUAL_STRING("", manifest.filesystem_url);
    TEST_ASSERT_EQUAL_UINT32(0, manifest.filesystem_size_bytes);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_manifest_parses);
    RUN_TEST(test_manifest_without_signature_still_parses_but_field_is_empty);
    RUN_TEST(test_manifest_missing_firmware_url_is_rejected);
    RUN_TEST(test_filesystem_field_parses_when_present);
    RUN_TEST(test_filesystem_field_absent_leaves_empty_fields);
    return UNITY_END();
}
