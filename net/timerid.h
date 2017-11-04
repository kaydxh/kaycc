#ifndef KAYCC_NET_TIMERID_H 
#define KAYCC_NET_TIMERID_H 

namespace kaycc {
namespace net {
    class Timer;

    class TimerId : public kaycc::copyable {
    public:
        TimerId()
            : timer_(NULL),
              sequence_(0) {

              }

        TimerId(Timer* timer, int64_t seq)
            : timer_(timer),
              sequence_(seq) {

              }

        friend class TimeQueue;

    private:
        Timer* timer_;
        int64_t sequence_;

    };

} //end net
}

#endif