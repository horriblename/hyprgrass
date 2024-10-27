#include "MouseDispatcherArg.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("encode -> decode yields original object") {
    Hyprgrass::MouseDispatcherArg testCases[] = {
        {Hyprgrass::EventType::BEGIN, 14.3, 15.6},
        {Hyprgrass::EventType::UPDATE, 0.0, 13},
    };

    for (auto const& tc : testCases) {
        std::string encoded = Hyprgrass::MouseDispatcherArgEncoding::encode(tc);
        auto decoded        = Hyprgrass::MouseDispatcherArgEncoding::decode(encoded);

        CHECK_EQ(tc, decoded);
    }
}
