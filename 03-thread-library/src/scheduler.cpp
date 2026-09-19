#include "scheduler.h"

namespace threading {

void Scheduler::spawn(std::function<void()> entry) {
    threads_.push_back(std::make_unique<Thread>(std::move(entry), &scheduler_ctx_));
}

void Scheduler::run() {
    bool any_running = true;
    while (any_running) {
        any_running = false;
        for (auto& t : threads_) {
            if (t->state == ThreadState::Terminated) continue;
            any_running = true;
            resume(*t, &scheduler_ctx_);
        }
    }
}

} // namespace threading
