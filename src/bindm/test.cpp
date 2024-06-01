#include "MouseDispatcherArg.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("encode -> decode yields original object") {
    Hyprgrass::MouseDispatcherArg testCases[] = {
        {0, Hyprgrass::EventType::BEGIN, 14.3, 15.6},
        {1, Hyprgrass::EventType::UPDATE, 0.0, 13},
    };

    for (auto const& tc : testCases) {
        std::string encoded = tc.encode();
        auto decoded        = Hyprgrass::MouseDispatcherArg::decode(encoded);

        if (decoded.has_value()) {
            CHECK_EQ(tc, decoded.value());
        }
    }
}
