#pragma once
#include <string>

class Logger {
  public:
    virtual ~Logger() {}
    virtual void debug(std::string s) = 0;
};
