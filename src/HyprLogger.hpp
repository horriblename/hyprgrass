#include "gestures/Logger.hpp"
#include "src/debug/Log.hpp"

class HyprLogger : public Logger {
  public:
    void debug(std::string s) {
        Debug::log(INFO, s);
    }
};
