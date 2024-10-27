
#include "MouseDispatcherArg.hpp"
#include <expected>
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

MouseDispatcherArg MouseDispatcherArgEncoding::decode(std::string s) {
    if (s.length() < sizeof(MouseDispatcherArgMessage)) {
        gArgDecodeErrInfo = s.length();
        throw ArgDecodeErr::STRING_TOO_SHORT;
    }

    auto msg = reinterpret_cast<const MouseDispatcherArgMessage*>(s.c_str());

    if (msg->version != HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION) {
        gArgDecodeErrInfo = HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION;
        throw ArgDecodeErr::VERSION_MISMATCH;
    }

    return msg->payload;
}
} // namespace Hyprgrass
