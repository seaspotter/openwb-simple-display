#include <cstring>

#include <unity.h>

#include "config/config_store.h"

using config::AppConfig;
using config::ConfigStore;

void setUp() {}
void tearDown() {}

// NVS calls are compiled out under UNIT_TEST, so Load() always starts from DefaultConfig().

void test_get_returns_defaults_on_first_call() {
    ConfigStore store;
    const AppConfig& cfg = store.Get();
    TEST_ASSERT_EQUAL_UINT8(config::kCurrentSchemaVersion, cfg.schema_version);
    TEST_ASSERT_EQUAL_UINT16(1883, cfg.mqtt_port);
}

void test_save_then_get_reflects_the_change() {
    ConfigStore store;
    AppConfig updated = store.Get();
    updated.mqtt_port = 8883;
    store.Save(updated);

    TEST_ASSERT_EQUAL_UINT16(8883, store.Get().mqtt_port);
}

void test_update_applies_mutator_and_persists() {
    ConfigStore store;
    store.Update([](AppConfig& c) { c.brightness_pct = 42; });
    TEST_ASSERT_EQUAL_UINT8(42, store.Get().brightness_pct);
}

void test_reset_to_defaults_discards_changes() {
    ConfigStore store;
    store.Update([](AppConfig& c) { c.brightness_pct = 5; });
    store.ResetToDefaults();
    TEST_ASSERT_EQUAL_UINT8(AppConfig{}.brightness_pct, store.Get().brightness_pct);
}

void test_reset_preserving_connection_keeps_wifi_pin_and_device_name() {
    ConfigStore store;
    store.Update([](AppConfig& c) {
        strcpy(c.wifi_ssid, "MyWifi");
        strcpy(c.wifi_password, "secret123");
        strcpy(c.web_ui_pin, "1234");
        strcpy(c.device_name, "my-display");
        c.brightness_pct = 5;
        c.mqtt_port = 8883;
    });

    store.ResetSettingsPreservingConnection();

    const AppConfig& cfg = store.Get();
    TEST_ASSERT_EQUAL_STRING("MyWifi", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("secret123", cfg.wifi_password);
    TEST_ASSERT_EQUAL_STRING("1234", cfg.web_ui_pin);
    TEST_ASSERT_EQUAL_STRING("my-display", cfg.device_name);
    TEST_ASSERT_EQUAL_UINT8(AppConfig{}.brightness_pct, cfg.brightness_pct);
    TEST_ASSERT_EQUAL_UINT16(AppConfig{}.mqtt_port, cfg.mqtt_port);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_get_returns_defaults_on_first_call);
    RUN_TEST(test_save_then_get_reflects_the_change);
    RUN_TEST(test_update_applies_mutator_and_persists);
    RUN_TEST(test_reset_to_defaults_discards_changes);
    RUN_TEST(test_reset_preserving_connection_keeps_wifi_pin_and_device_name);
    return UNITY_END();
}
