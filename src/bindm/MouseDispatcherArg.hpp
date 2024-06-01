#include <cstdint>
#include <optional>
#include <string>
namespace Hyprgrass {
enum class EventType : uint32_t {
    BEGIN,
    UPDATE,
    END,
};

struct MouseDispatcherArg {
    uint32_t version;
    EventType event;
    double x;
    double y;

    std::string encode() const;
    static std::optional<MouseDispatcherArg> decode(std::string s);

    auto operator<=>(const MouseDispatcherArg&) const = default;
};

} // namespace Hyprgrass
