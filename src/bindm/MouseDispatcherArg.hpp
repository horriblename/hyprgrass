#include <cstdint>
#include <optional>
#include <string>

#define HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION 0

namespace Hyprgrass {

enum class EventType : uint32_t {
    BEGIN,
    UPDATE,
    END,
};

struct MouseDispatcherArg {
    EventType event;
    double x;
    double y;

    auto operator<=>(const MouseDispatcherArg&) const = default;
};

struct MouseDispatcherArgEncoding {
    static uint32_t version() {
        return HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION;
    }

    static std::string encode(const MouseDispatcherArg&);
    static std::optional<MouseDispatcherArg> decode(std::string s);
};

} // namespace Hyprgrass
