
#include "src/debug/Log.hpp"
#include <functional>
#include <wayland-server-core.h>
#include <wayland-server.h>

class Debouncer {
  public:
    using Callback = std::function<void()>;

    Debouncer(struct wl_event_loop* loop, int delay_ms, Callback callback)
        : loop_(loop), delay_ms_(delay_ms), callback_(callback), timer_source_(nullptr) {}

    ~Debouncer() {
        if (timer_source_) {
            wl_event_source_remove(timer_source_);
        }
    }

    void start() {
        if (timer_source_)
            return;

        // Add a new timer to the event loop
        timer_source_ = wl_event_loop_add_timer(loop_, &Debouncer::onTimerEnd, this);
        if (timer_source_) {
            wl_event_source_timer_update(timer_source_, delay_ms_);
        } else {
            Debug::log(ERR, "could not create wl timer source");
        }
    }

    void disarm() {
        if (timer_source_) {
            wl_event_source_remove(this->timer_source_);
            this->timer_source_ = nullptr;
        };
    }

  private:
    static int onTimerEnd(void* data) {
        auto* debouncer = static_cast<Debouncer*>(data);
        if (debouncer->callback_) {
            debouncer->callback_();
        }

        // The timer source is handled, we don't need it anymore
        debouncer->timer_source_ = nullptr;
        return 0; // Return 0 to indicate success
    }

    struct wl_event_loop* loop_;
    int delay_ms_;
    Callback callback_;
    struct wl_event_source* timer_source_;
};
