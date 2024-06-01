
#include "MouseDispatcherArg.hpp"
#include <optional>
#include <string>

namespace Hyprgrass {
struct MouseDispatcherArgMessage {
    uint32_t version;
    MouseDispatcherArg payload;
};

std::string MouseDispatcherArgEncoding::encode(const MouseDispatcherArg& arg) {
    MouseDispatcherArgMessage payload = {
        .version = HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION,
        .payload = arg,
    };

    return std::string(reinterpret_cast<const char*>(&payload), sizeof(payload));
}

std::optional<MouseDispatcherArg> MouseDispatcherArgEncoding::decode(std::string s) {
    if (s.length() < sizeof(MouseDispatcherArgMessage)) {
        return std::nullopt;
    }

    auto msg = reinterpret_cast<const MouseDispatcherArgMessage*>(s.c_str());

    if (msg->version != HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION) {
        return std::nullopt;
    }

    return msg->payload;
}
} // namespace Hyprgrass
