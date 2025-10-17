#pragma once

#include "SafeSignal.hpp"

// Get a signal emitted with value true when entering sleep, and false when exiting
SafeSignal<bool>& prepare_for_sleep();
