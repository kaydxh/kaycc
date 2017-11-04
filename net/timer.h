#ifndef KAYCC_NET_TIMER_H 
#define KAYCC_NET_TIMER_H 

#include <boost/noncopyable.hpp>

#include "callbacks.h"
#include "../base/timestamp.h"
#include "../base/atomic.h"

namespace kaycc {
namespace net {

    class Timer : boost::noncopyable {
    public:
        Timer(const TimerCallback& cb, Timestamp when, double interval)
            : callback_(cb),
              expiration_(when),
              interval_(interval),
              repeat_(interval > 0),
              sequence_(s_numCreated_.incrementAndGet()) {

            }

        void run() const {
            callback_();
        }

        Timestamp expiration() const {
            return expiration_;
        }

        bool repeat() const {
            return repeat_;
        }

        int64_t sequence() const {
            return sequence_;
        }

        void restart(Timestamp now);

        static int64_t numCreated() {
            return s_numCreated_.get();
        }

    private:
        const TimerCallback callback_;
        Timestamp expiration_;
        const double interval_;
        const bool repeat_;
        const int64_t sequence_;

        static AtomicInt64 s_numCreated_;
    };

}
}


#endif