#ifndef KAYCC_BASE_TIMESTAMP_H 
#define KAYCC_BASE_TIMESTAMP_H 

#include "copyable.h"
#include <boost/operators.hpp>

namespace kaycc {
    class Timestamp : public kaycc::copyable,
                      public boost::equality_comparable<Timestamp>,
                      public boost::less_than_comparable<Timestamp> { //表示可以作大小比较（!=,>,<=,>=; 在实现时，只需实现<
                                                                      //和==即可，其他可以自动生成;比较运算符的重载是以友元的形式实现的。

    public:
        Timestamp()
            : microSecondsSinceEpoch_(0) {

            }

        explicit Timestamp(int64_t microSecondsSinceEpoch)
            : microSecondsSinceEpoch_(microSecondsSinceEpoch) {

            }

        std::string toString() const;
        ////两个时间戳进行交换
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
        int64_t microSecondsSinceEpoch_; //epoch到现在的微秒数

    };

    inline bool operator<(Timestamp lhs, Timestamp rhs) {
        return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
    }

    inline bool operator==(Timestamp lhs, Timestamp rhs) {
        return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
    }

    inline double timeDifference(Timestamp high, Timestamp low) {
        int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch(); //微秒
        return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond; //转为秒
    }

    inline Timestamp addTime(Timestamp timestamp, double seconds) {
        int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond); //秒转为微秒
        return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
    }
}

#endif