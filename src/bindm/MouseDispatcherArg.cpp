
#include "MouseDispatcherArg.hpp"
#include <optional>
#include <string>

namespace Hyprgrass {
std::string MouseDispatcherArg::encode() const {
    return std::string(reinterpret_cast<const char*>(this), sizeof(*this));
}

std::optional<MouseDispatcherArg> MouseDispatcherArg::decode(std::string s) {
    if (s.length() < sizeof(MouseDispatcherArg)) {
        return std::nullopt;
    }

    auto data = reinterpret_cast<const MouseDispatcherArg*>(s.c_str());
    return *data;
}
} // namespace Hyprgrass
