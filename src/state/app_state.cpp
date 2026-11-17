#include "app_state.h"

#include <cstring>

#ifdef UNIT_TEST
#define LOCK(x) ((void)(x))
#define UNLOCK(x) ((void)(x))
#else
#define LOCK(x) portENTER_CRITICAL(x)
#define UNLOCK(x) portEXIT_CRITICAL(x)
#endif

namespace state {

void AppState::SetChargepointStatus(uint8_t index, const openwb::ChargepointStatus& status) {
    if (index >= kMaxChargepoints) return;
    LOCK(&lock_);
    chargepoints_[index] = status;
    UNLOCK(&lock_);
}

openwb::ChargepointStatus AppState::GetChargepointStatus(uint8_t index) const {
    openwb::ChargepointStatus copy{};
    if (index >= kMaxChargepoints) return copy;
    LOCK(&lock_);
    copy = chargepoints_[index];
    UNLOCK(&lock_);
    return copy;
}

void AppState::SetGlobalStatus(const openwb::GlobalStatus& status) {
    LOCK(&lock_);
    global_ = status;
    UNLOCK(&lock_);
}

openwb::GlobalStatus AppState::GetGlobalStatus() const {
    LOCK(&lock_);
    openwb::GlobalStatus copy = global_;
    UNLOCK(&lock_);
    return copy;
}

void AppState::NoteTouch(uint32_t now_ms) {
    LOCK(&lock_);
    last_touch_ms_ = now_ms;
    UNLOCK(&lock_);
}

uint32_t AppState::LastTouchMillis() const {
    LOCK(&lock_);
    uint32_t copy = last_touch_ms_;
    UNLOCK(&lock_);
    return copy;
}

void AppState::SetMqttConnected(bool connected) {
    LOCK(&lock_);
    mqtt_connected_ = connected;
    UNLOCK(&lock_);
}

bool AppState::IsMqttConnected() const {
    LOCK(&lock_);
    bool copy = mqtt_connected_;
    UNLOCK(&lock_);
    return copy;
}

void AppState::SetProvisioningActive(bool active) {
    LOCK(&lock_);
    provisioning_active_ = active;
    UNLOCK(&lock_);
}

bool AppState::IsProvisioningActive() const {
    LOCK(&lock_);
    bool copy = provisioning_active_;
    UNLOCK(&lock_);
    return copy;
}

void AppState::SetNetworkReady(bool ready) {
    LOCK(&lock_);
    network_ready_ = ready;
    UNLOCK(&lock_);
}

bool AppState::IsNetworkReady() const {
    LOCK(&lock_);
    bool copy = network_ready_;
    UNLOCK(&lock_);
    return copy;
}

void AppState::SetIpAddress(const char* ip) {
    LOCK(&lock_);
    strncpy(ip_address_, ip, sizeof(ip_address_) - 1);
    ip_address_[sizeof(ip_address_) - 1] = '\0';
    UNLOCK(&lock_);
}

void AppState::GetIpAddress(char* out, size_t out_size) const {
    LOCK(&lock_);
    strncpy(out, ip_address_, out_size - 1);
    out[out_size - 1] = '\0';
    UNLOCK(&lock_);
}

void AppState::SetPendingBrightnessSave(uint8_t percent, uint32_t now_ms) {
    LOCK(&lock_);
    pending_brightness_percent_ = percent;
    pending_brightness_set_ms_ = now_ms;
    brightness_save_pending_ = true;
    UNLOCK(&lock_);
}

bool AppState::TryTakePendingBrightnessSave(uint32_t now_ms, uint32_t debounce_ms, uint8_t* out_percent) {
    LOCK(&lock_);
    bool ready = brightness_save_pending_ && (now_ms - pending_brightness_set_ms_ >= debounce_ms);
    if (ready) {
        *out_percent = pending_brightness_percent_;
        brightness_save_pending_ = false;
    }
    UNLOCK(&lock_);
    return ready;
}

bool AppState::AnyChargepointCharging() const {
    LOCK(&lock_);
    bool any = false;
    for (const auto& cp : chargepoints_) {
        if (cp.valid && cp.charging) {
            any = true;
            break;
        }
    }
    UNLOCK(&lock_);
    return any;
}

AppState& State() {
    static AppState instance;
    return instance;
}

} // namespace state
