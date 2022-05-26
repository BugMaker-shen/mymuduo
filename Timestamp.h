#pragma once

#include <iostream>
#include <string>

class Timestamp{
    public:
        Timestamp();
        explicit Timestamp(int64_t microSecondsSinceEpoch);
        static Timestamp now(); // now方法不需要通过对象调用，类名即可直接调用
        std::string toString() const;
    private:
        int64_t microSecondsSinceEpoch_;
};
