#include <optional>
#include <variant>
#include <vector>
#include <wlr/types/wlr_touch.h>

typedef std::variant<wlr_touch_down_event, wlr_touch_up_event,
                     wlr_touch_motion_event>
    touchEvent;

struct TestCase {
    std::vector<touchEvent> inputStream;
};

static TestCase TEST() {
    return {std::vector<touchEvent>{
        wlr_touch_down_event{nullptr, 100, 0, 450, 290},
        wlr_touch_down_event{nullptr, 100, 1, 500, 300},
        wlr_touch_down_event{nullptr, 100, 2, 550, 290},
        wlr_touch_motion_event{nullptr, 110, 0, 443, 290},
        wlr_touch_motion_event{nullptr, 110, 1, 493, 300},
        wlr_touch_motion_event{nullptr, 110, 2, 543, 290},
        wlr_touch_motion_event{nullptr, 120, 0, 436, 290},
        wlr_touch_motion_event{nullptr, 120, 1, 486, 300},
        wlr_touch_motion_event{nullptr, 120, 2, 536, 290},
        wlr_touch_motion_event{nullptr, 130, 0, 429, 290},
        wlr_touch_motion_event{nullptr, 130, 1, 479, 300},
        wlr_touch_motion_event{nullptr, 130, 2, 529, 290},
        wlr_touch_motion_event{nullptr, 140, 0, 422, 290},
        wlr_touch_motion_event{nullptr, 140, 1, 472, 300},
        wlr_touch_motion_event{nullptr, 140, 2, 522, 290},
        wlr_touch_motion_event{nullptr, 150, 0, 415, 290},
        wlr_touch_motion_event{nullptr, 150, 1, 465, 300},
        wlr_touch_motion_event{nullptr, 150, 2, 515, 290},
        wlr_touch_motion_event{nullptr, 160, 0, 408, 290},
        wlr_touch_motion_event{nullptr, 160, 1, 458, 300},
        wlr_touch_motion_event{nullptr, 160, 2, 508, 290},
        wlr_touch_motion_event{nullptr, 170, 0, 401, 290},
        wlr_touch_motion_event{nullptr, 170, 1, 451, 300},
        wlr_touch_motion_event{nullptr, 170, 2, 501, 290},
        wlr_touch_motion_event{nullptr, 180, 0, 394, 290},
        wlr_touch_motion_event{nullptr, 180, 1, 444, 300},
        wlr_touch_motion_event{nullptr, 180, 2, 494, 290},
        wlr_touch_motion_event{nullptr, 190, 0, 387, 290},
        wlr_touch_motion_event{nullptr, 190, 1, 437, 300},
        wlr_touch_motion_event{nullptr, 190, 2, 487, 290},
        wlr_touch_motion_event{nullptr, 200, 0, 380, 290},
        wlr_touch_motion_event{nullptr, 200, 1, 430, 300},
        wlr_touch_motion_event{nullptr, 200, 2, 480, 290},
        wlr_touch_motion_event{nullptr, 210, 0, 373, 290},
        wlr_touch_motion_event{nullptr, 210, 1, 423, 300},
        wlr_touch_motion_event{nullptr, 210, 2, 473, 290},
        wlr_touch_motion_event{nullptr, 220, 0, 366, 290},
        wlr_touch_motion_event{nullptr, 220, 1, 416, 300},
        wlr_touch_motion_event{nullptr, 220, 2, 466, 290},
        wlr_touch_motion_event{nullptr, 230, 0, 359, 290},
        wlr_touch_motion_event{nullptr, 230, 1, 409, 300},
        wlr_touch_motion_event{nullptr, 230, 2, 459, 290},
        wlr_touch_motion_event{nullptr, 240, 0, 352, 290},
        wlr_touch_motion_event{nullptr, 240, 1, 402, 300},
        wlr_touch_motion_event{nullptr, 240, 2, 452, 290},
        wlr_touch_motion_event{nullptr, 250, 0, 345, 290},
        wlr_touch_motion_event{nullptr, 250, 1, 395, 300},
        wlr_touch_motion_event{nullptr, 250, 2, 445, 290},
        wlr_touch_motion_event{nullptr, 260, 0, 338, 290},
        wlr_touch_motion_event{nullptr, 260, 1, 388, 300},
        wlr_touch_motion_event{nullptr, 260, 2, 438, 290},
        wlr_touch_motion_event{nullptr, 270, 0, 331, 290},
        wlr_touch_motion_event{nullptr, 270, 1, 381, 300},
        wlr_touch_motion_event{nullptr, 270, 2, 431, 290},
        wlr_touch_motion_event{nullptr, 280, 0, 324, 290},
        wlr_touch_motion_event{nullptr, 280, 1, 374, 300},
        wlr_touch_motion_event{nullptr, 280, 2, 424, 290},
        wlr_touch_motion_event{nullptr, 290, 0, 317, 290},
        wlr_touch_motion_event{nullptr, 290, 1, 367, 300},
        wlr_touch_motion_event{nullptr, 290, 2, 417, 290},
        wlr_touch_up_event{nullptr, 300, 0},
        wlr_touch_up_event{nullptr, 300, 1},
        wlr_touch_up_event{nullptr, 300, 2},

    }};
}
