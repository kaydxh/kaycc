#ifndef KAYCC_BASE_TIMESTAMP_H 
#define KAYCC_BASE_TIMESTAMP_H 

#include "copyable.h"
#include <boost/operators.hpp>

namespace kaycc {
    class Timestamp : public kaycc::copyable,
                      public boost::equality_comparable<Timestamp>,
                      public boost::less_than_comparable<Timestamp> {

    public:
        Timestamp()
            : microSecondsSinceEpoch_(0) {

            }

        explicit Timestamp(int64_t microSecondsSinceEpoch)
            : microSecondsSinceEpoch_(microSecondsSinceEpoch) {

            }

        void swap(Timestamp& that) {
            std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
        }

        bool valid() const {
            return microSecondsSinceEpoch_ > 0;
        }

        int64_t microSecondsSinceEpoch() const {
            return microSecondsSinceEpoch_;
        }

        time_t secondsSinceEpoch() const {
            return static_cast<int64_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
        }

        static Timestamp now();
        static Timestamp invalid() {
            return Timestamp();
        }

        static Timestamp fromUnixTime(time_t t) {
            return fromUnixTime(t, 0);
        }

        static Timestamp fromUnixTime(time_t t, int microseconds) {
            return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);

        }

        static const int kMicroSecondsPerSecond = 1000000;

    private:
        int64_t microSecondsSinceEpoch_;

    };

    inline bool operator<(Timestamp lhs, Timestamp rhs) {
        return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }

    inline bool operator==(Timestamp lhs, Timestamp rhs) {
        return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }

    inline double timeDifference(Timestamp high, Timestamp low) {
        static_cast<double>((high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch())) / Timestamp::kMicroSecondsPerSecond;
    }

    inline Timestamp addTime(Timestamp timestamp, double seconds) {
        int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
        return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
    }
}

#endif