#ifndef KAYCC_BASE_SINGLETON_H
#define KAYCC_BASE_SINGLETON_H

namespace kaycc {
namespace detail {
    //SFINA原则，测试是否有成员函数no_destroy，但是不会探测到继承的成员函数
    template <typename T>
    struct has_no_destroy {
    #if __cplusplus >= 201103L
        template <typename C> static char test(decltype(&C::no_destroy));
    #else
        template <typename C> static char test(typeof(&C::no_destroy));
    #endif
        template <typename C> static int32_t test(...);
        const static bool value = sizeof(test<T>(0)) == 1; //如果C有no_destroy函数，就会匹配char test(typeof(&C::no_destroy))，
            //因此返回值的类型大小就是1（char），否则，就会匹配int32_t test(...)，返回值的类型大小的是4（int32_t)，test<T>(0),相当于将T传给C，
            //0代表传给的形参值（typeof(&C::no_destroy)类型的值 或...任意类型的值)
    };
} //end namespace detail

    template <typename T>
    class Singleton : boost::noncopyable {
    public:
        static T& instance() {
            pthread_once(&ponce_, &Singleton::init); //多个线程只会执行一次init函数  
            return *value_;
        }

    private:
        Singleton();
        ~Singleton();

        static void init() {
            value_ = new T();
            if (!detail::has_no_destroy<T>::value) { //T没有no_destroy成员函数，就注册destory，程序退出时调用destroy函数
                ::atexit(destory);
            }
        }

        static void destory() {
            typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];//类T一定要完整，否则delete时会出错，这里当T不完整时将数组的长度置为-1，会报错  
            T_must_be_complete_type dumpy; (void) dumpy;

            delete value_;
            value_ = NULL;
        }

    private:
        static pthread_once_t ponce_;
        static T*             value_;

    };

    template <typename T>
    pthread_once Singleton<T>::ponce_ = PTHREAD_ONCE_INIT; //静态变量只能在类外进行初始化

    template <typename T>
    T * Singleton<T>::value_ = NULL;

} //end namespace kaycc 

#endif