#ifndef KAYCC_BASE_TYPES_H 
#define KAYCC_BASE_TYPES_H 

namespace kaycc {

    //隐式转换， 如，派生类转基类
    template <typename To, typename From> 
    inline To implicit_cast(From const &f) {
        return f;
    }

    //用来替代dynamic_cast的，没有运行时检查，直接用static_cast来做转型，从而提高性能。但是基类转到派生类，static_cast不会报错，但是是危险的，会crash
    template <typename To, typename From>
    inline To down_cast(From* f) {
        if (false) {
            implicit_cast<From*, To>(0); //巧妙的使用了implicit_cast，让编译器帮助做了类型检查，而 if (false) 
                                        //条件保证了最终肯定会被编译器优化掉，所以对性能没有任何影响。
        }

        return static_cast<To>(f); 

    }

}

#endif