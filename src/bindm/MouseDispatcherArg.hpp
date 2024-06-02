#include <cstdint>
#include <expected>
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

enum class ArgDecodeErr {
    STRING_TOO_SHORT,
    VERSION_MISMATCH,
};
inline int gArgDecodeErrInfo;

struct MouseDispatcherArgEncoding {
    static uint32_t version() {
        return HYPRGRASS_CUSTOM_BINDM_LAYOUT_VERSION;
    }

    static std::string encode(const MouseDispatcherArg&);
    static std::expected<MouseDispatcherArg, ArgDecodeErr> decode(std::string s);
};

} // namespace Hyprgrass
