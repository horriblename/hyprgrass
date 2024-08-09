#pragma once
#include "../Logger.hpp"
#include <iostream>

class CoutLogger : public Logger {
    void debug(std::string s) override {
        std::cout << s << std::endl;
    }
};
