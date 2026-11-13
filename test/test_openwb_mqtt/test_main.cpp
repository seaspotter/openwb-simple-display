#include <unity.h>

#include "openwb/openwb_mqtt_topics.h"

using openwb::ChargepointStatus;
using openwb::GlobalStatus;
using openwb::mqtt::ParseChargepointField;
using openwb::mqtt::ParseGlobalTopic;
using openwb::mqtt::ParseLogPayload;

void setUp() {}
void tearDown() {}

void test_power_field_parses_as_float() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("power", "7360.0", status));
    TEST_ASSERT_EQUAL_FLOAT(7360.0f, status.power_w);
}

// Confirmed live against the user's broker: openWB's simpleAPI MQTT mirror writes
// Python's str(bool) ("True"/"False"), not JSON/lowercase.
void test_plug_state_parses_python_bool_string() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("plug_state", "False", status));
    TEST_ASSERT_FALSE(status.plugged);
    TEST_ASSERT_TRUE(ParseChargepointField("charge_state", "True", status));
    TEST_ASSERT_TRUE(status.charging);
}

// Confirmed verbatim from the live broker for an unplugged chargepoint.
void test_state_str_copies_the_full_message_verbatim() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField(
        "state_str", "Keine Ladung, da kein Auto angesteckt ist.", status));
    TEST_ASSERT_EQUAL_STRING("Keine Ladung, da kein Auto angesteckt ist.", status.state_str);
}

void test_nested_soc_suffix_is_matched_exactly() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("soc/soc", "56", status));
    TEST_ASSERT_EQUAL_FLOAT(56.0f, status.soc_percent);
}

void test_config_name_maps_to_the_name_field() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("config/name", "MQTT-Ladepunkt 3", status));
    TEST_ASSERT_EQUAL_STRING("MQTT-Ladepunkt 3", status.name);
}

// The wildcard subscribe (openWB/simpleAPI/chargepoint/<id>/#) is forward-compatible
// with fields this project doesn't parse — an unrecognized suffix must be reported as
// "not handled", not silently mis-parsed into the wrong field.
void test_unrecognized_suffix_returns_false() {
    ChargepointStatus status{};
    TEST_ASSERT_FALSE(ParseChargepointField("max_price_eco", "25.17", status));
}

// openWB/chargepoint/<id>/set/log is the one topic with a JSON payload instead of a
// plain scalar (confirmed live) — only imported_since_plugged ("Geladen") is extracted.
void test_log_payload_extracts_imported_since_plugged() {
    ChargepointStatus status{};
    ParseLogPayload(
        "{\"imported_since_plugged\": 2400, \"soc_at_start\": null, \"end\": null}", status);
    TEST_ASSERT_EQUAL_FLOAT(2400.0f, status.energy_charged_wh);
}

void test_log_payload_missing_field_defaults_to_zero() {
    ChargepointStatus status{};
    status.energy_charged_wh = 999; // pre-existing value must be overwritten, not kept
    ParseLogPayload("{\"end\": null}", status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, status.energy_charged_wh);
}

// Confirmed live: openWB/pv/get/power is negative while generating, same convention as
// the retired HTTP client's "pv-all" field — negated back to this project's "positive
// generation magnitude" convention.
void test_pv_power_is_negated_from_the_raw_topic_sign() {
    GlobalStatus global{};
    TEST_ASSERT_TRUE(ParseGlobalTopic("openWB/pv/get/power", "-2885.0", global));
    TEST_ASSERT_EQUAL_FLOAT(2885.0f, global.pv_power_w);
}

void test_grid_power_reads_the_simpleapi_mirror_topic() {
    GlobalStatus global{};
    TEST_ASSERT_TRUE(ParseGlobalTopic("openWB/simpleAPI/counter/get/power", "-17.0", global));
    TEST_ASSERT_EQUAL_FLOAT(-17.0f, global.grid_power_w);
}

void test_unrecognized_global_topic_returns_false() {
    GlobalStatus global{};
    TEST_ASSERT_FALSE(ParseGlobalTopic("openWB/some/other/topic", "1", global));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_power_field_parses_as_float);
    RUN_TEST(test_plug_state_parses_python_bool_string);
    RUN_TEST(test_state_str_copies_the_full_message_verbatim);
    RUN_TEST(test_nested_soc_suffix_is_matched_exactly);
    RUN_TEST(test_config_name_maps_to_the_name_field);
    RUN_TEST(test_unrecognized_suffix_returns_false);
    RUN_TEST(test_log_payload_extracts_imported_since_plugged);
    RUN_TEST(test_log_payload_missing_field_defaults_to_zero);
    RUN_TEST(test_pv_power_is_negated_from_the_raw_topic_sign);
    RUN_TEST(test_grid_power_reads_the_simpleapi_mirror_topic);
    RUN_TEST(test_unrecognized_global_topic_returns_false);
    return UNITY_END();
}
