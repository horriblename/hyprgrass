#include "gestures/Logger.hpp"
#include <hyprland/src/debug/log/Logger.hpp>

class HyprLogger : public Logger {
  public:
    void debug(std::string s) {
        LOG(Log::INFO, "[hyprgrass] {}", s);
    }
};
