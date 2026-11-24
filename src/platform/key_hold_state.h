#pragma once

#include <cstdint>

namespace platform {

struct KeyHoldTransition {
    uint32_t released_key{0};
    bool started_press{false};
};

class KeyHoldState {
public:
    KeyHoldTransition update(uint32_t key, bool pressed) {
        if (pressed) {
            if (active_key_ == key) return {};
            const auto previous = active_key_;
            active_key_ = key;
            return {previous, true};
        }
        if (active_key_ != key) return {};
        active_key_ = 0;
        return {key, false};
    }

    uint32_t reset() {
        const auto previous = active_key_;
        active_key_ = 0;
        return previous;
    }

    uint32_t active_key() const { return active_key_; }

private:
    uint32_t active_key_{0};
};

} // namespace platform
