#include <unity.h>

#include "openwb/openwb_mqtt_topics.h"

using openwb::ChargepointStatus;
using openwb::GlobalStatus;
using openwb::mqtt::ExcludedFromHomeConsumption;
using openwb::mqtt::HcFlag;
using openwb::mqtt::ParseChargepointField;
using openwb::mqtt::ParseGlobalTopic;
using openwb::mqtt::ParseHcFlag;
using openwb::mqtt::ParseLogPayload;

void setUp() {}
void tearDown() {}

void test_power_field_parses_as_float() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("power", "7360.0", status));
    TEST_ASSERT_EQUAL_FLOAT(7360.0f, status.power_w);
}

// openWB's simpleAPI MQTT mirror writes Python's str(bool): "True"/"False".
void test_plug_state_parses_python_bool_string() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("plug_state", "False", status));
    TEST_ASSERT_FALSE(status.plugged);
    TEST_ASSERT_TRUE(ParseChargepointField("charge_state", "True", status));
    TEST_ASSERT_TRUE(status.charging);
}

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

void test_unrecognized_suffix_returns_false() {
    ChargepointStatus status{};
    TEST_ASSERT_FALSE(ParseChargepointField("totally_unknown_field", "25.17", status));
}

// Settings-popup fields — see
// https://wiki.openwb.de/doku.php?id=openwb:vc:2.3.x:simpleapi#schreiboperationen_set-topics
void test_instant_and_pv_limit_fields_parse_independently() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("instant_charging_limit", "soc", status));
    TEST_ASSERT_TRUE(ParseChargepointField("instant_charging_limit_soc", "80", status));
    TEST_ASSERT_TRUE(ParseChargepointField("pv_charging_limit", "amount", status));
    TEST_ASSERT_TRUE(ParseChargepointField("pv_charging_limit_amount", "20", status));

    TEST_ASSERT_EQUAL_STRING("soc", status.instant_limit_type);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, status.instant_limit_soc);
    TEST_ASSERT_EQUAL_STRING("amount", status.pv_limit_type);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, status.pv_limit_amount_kwh);
}

void test_max_price_eco_parses_as_the_ct_value_openwb_reports() {
    ChargepointStatus status{};
    TEST_ASSERT_TRUE(ParseChargepointField("max_price_eco", "25.17", status));
    TEST_ASSERT_EQUAL_FLOAT(25.17f, status.max_price_eco_ct);
}

// openWB/chargepoint/<id>/set/log is the one topic with a JSON payload, not a scalar.
void test_log_payload_extracts_imported_since_plugged() {
    ChargepointStatus status{};
    ParseLogPayload(
        "{\"imported_since_plugged\": 2400, \"soc_at_start\": null, \"end\": null}", status);
    TEST_ASSERT_EQUAL_FLOAT(2400.0f, status.energy_charged_wh);
}

void test_log_payload_missing_field_defaults_to_zero() {
    ChargepointStatus status{};
    status.energy_charged_wh = 999;
    ParseLogPayload("{\"end\": null}", status);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, status.energy_charged_wh);
}

// openWB/pv/get/power is negative while generating; this project negates it back.
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

void test_parse_hc_flag_recognizes_all_three_openwb_values() {
    TEST_ASSERT_TRUE(ParseHcFlag("home_consumption") == HcFlag::kHome);
    TEST_ASSERT_TRUE(ParseHcFlag("no_home_consumption") == HcFlag::kNotHome);
    TEST_ASSERT_TRUE(ParseHcFlag("auto_home_consumption") == HcFlag::kAuto);
    TEST_ASSERT_TRUE(ParseHcFlag("garbage") == HcFlag::kAuto);
}

// A child explicitly flagged NOT home consumption is excluded regardless of the root.
void test_explicit_not_home_consumption_is_always_excluded() {
    TEST_ASSERT_TRUE(ExcludedFromHomeConsumption(HcFlag::kNotHome, HcFlag::kHome));
    TEST_ASSERT_TRUE(ExcludedFromHomeConsumption(HcFlag::kNotHome, HcFlag::kNotHome));
}

// A child explicitly flagged AS home consumption is never excluded, regardless of root —
// this is the case that was double-counted against home_consumption_w before this fix.
void test_explicit_home_consumption_is_never_excluded() {
    TEST_ASSERT_FALSE(ExcludedFromHomeConsumption(HcFlag::kHome, HcFlag::kHome));
    TEST_ASSERT_FALSE(ExcludedFromHomeConsumption(HcFlag::kHome, HcFlag::kNotHome));
}

// "auto" inherits the root's effective flag; the root's own "auto" defaults to home
// consumption (openWB hardcodes this — there's no grandparent to inherit from).
void test_auto_flag_inherits_the_roots_effective_flag() {
    TEST_ASSERT_FALSE(ExcludedFromHomeConsumption(HcFlag::kAuto, HcFlag::kHome));
    TEST_ASSERT_TRUE(ExcludedFromHomeConsumption(HcFlag::kAuto, HcFlag::kNotHome));
    TEST_ASSERT_FALSE(ExcludedFromHomeConsumption(HcFlag::kAuto, HcFlag::kAuto)); // root's own auto -> home
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_power_field_parses_as_float);
    RUN_TEST(test_plug_state_parses_python_bool_string);
    RUN_TEST(test_state_str_copies_the_full_message_verbatim);
    RUN_TEST(test_nested_soc_suffix_is_matched_exactly);
    RUN_TEST(test_config_name_maps_to_the_name_field);
    RUN_TEST(test_unrecognized_suffix_returns_false);
    RUN_TEST(test_instant_and_pv_limit_fields_parse_independently);
    RUN_TEST(test_max_price_eco_parses_as_the_ct_value_openwb_reports);
    RUN_TEST(test_log_payload_extracts_imported_since_plugged);
    RUN_TEST(test_log_payload_missing_field_defaults_to_zero);
    RUN_TEST(test_pv_power_is_negated_from_the_raw_topic_sign);
    RUN_TEST(test_grid_power_reads_the_simpleapi_mirror_topic);
    RUN_TEST(test_unrecognized_global_topic_returns_false);
    RUN_TEST(test_parse_hc_flag_recognizes_all_three_openwb_values);
    RUN_TEST(test_explicit_not_home_consumption_is_always_excluded);
    RUN_TEST(test_explicit_home_consumption_is_never_excluded);
    RUN_TEST(test_auto_flag_inherits_the_roots_effective_flag);
    return UNITY_END();
}
