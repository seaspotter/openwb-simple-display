#include "app_state.h"

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
