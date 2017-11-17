#ifndef KAYCC_NET_BUFFER_H 
#define KAYCC_NET_BUFFER_H

#include "../base/copyable.h"
#include "endian.h"

#include <algorithm> 
#include <vector>

#include <assert.h>
#include <string.h> //memchr
#include <string>

/* 
 * 应用层缓冲区（专门用于网络数据读取发送） 
 * 假如这个buffer用于套接字读取，那么框架把套接字读取的数据写入到writerIndex指示的位置，而用户则从readerIndex的位置开始读取数据 
 * readerIndex和writerIndex之间的这段存放了有效数据 
 */ 

namespace kaycc {
namespace net {

    // 一个缓冲区由三个部分组成：预留区、读取区，写入区，  
    // 0～readerIndex之间是预留区  
    // readerIndex～writerIndex之间是读取区  
    // writerIndex～size之间是写入区  

    // prependable = readIndex
    // readable = writeIndex - readIndex
    // writable = size() - writeIndex

    /// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
    ///
    /// @code
    /// +-------------------+------------------+------------------+
    /// | prependable bytes |  readable bytes  |  writable bytes  |
    /// |                   |     (CONTENT)    |                  |
    /// +-------------------+------------------+------------------+
    /// |                   |                  |                  |
    /// 0      <=      readerIndex   <=   writerIndex    <=     size
    /// @endcode

    class Buffer : public kaycc::copyable {
    public:
        static const size_t kCheapPrepend = 8; //prependable的初始化大小

        /* 
        * 缓默认的冲区初始大小 
        * 起始的时候，预留区为8字节 
        * 读取区（或可读区域）为0字节 
        * 写入区为kInitialSize字节 
        */ 
        static const size_t kInitialSize = 1024; //writable的初始化大小

        explicit Buffer(size_t initialSize = kInitialSize)
            : buffer_(kCheapPrepend + initialSize),
              readerIndex_(kCheapPrepend),
              writerIndex_(kCheapPrepend) {
            
            assert(readableBytes() == 0);
            assert(writableBytes() == kInitialSize);
            assert(prependableBytes() == kCheapPrepend);
        }

        // 交换数据Buffer 
        void swap(Buffer& rhs) {
            buffer_.swap(rhs.buffer_);
            std::swap(readerIndex_, rhs.readerIndex_);
            std::swap(writerIndex_, rhs.writerIndex_);
        }

        // 可读区域的字节数 
        size_t readableBytes() const {
            return writerIndex_ - readerIndex_;
        }

        // 可写区域的字节数 
        size_t writableBytes() const {
            return buffer_.size() - writerIndex_;
        }

        // 预留区字节数 
        size_t prependableBytes() const {
            return readerIndex_;
        }

        // 返回可读区域的起始位置 
        const char* peek() const {
            return begin() + readerIndex_;
        }

        // 在可读区域查找\r\n 
        const char* findCRLF() const {
            // FIXME: replace with memmem()?
            const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2); //搜索不到返回last
            return crlf == beginWrite() ? NULL : crlf; //搜索不到序列2，就返回NULL，否则返回crlf
        }

         // 在可读区域指定的起始位置查找\r\n
        const char* findCRLF(const char* start) const {
            assert(peek() <= start); //star必须是可读区域
            assert(start <= beginWrite());
            // FIXME: replace with memmem()?

            const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
            return crlf == beginWrite() ? NULL : crlf;
        }

        // 在可读区域查找\n  EOL即一行的结束
        const char* findEOL() const {
            const void* eol = memchr(peek(), '\n', readableBytes()); //从buf所指内存区域的前count个字节查找字符ch。
                                                                    //当第一次遇到字符ch时停止查找。如果成功，返回指向字符ch的指针；否则返回NULL。

            return static_cast<const char*>(eol);
        }

        // 在可读区域指定的起始位置查找\n 
        const char* findEOL(const char* start) const {
            assert(peek() <= start);
            assert(start <= beginWrite());

            const void* eol = memchr(start, '\n', beginWrite() - start);
            return static_cast<const char*>(eol);
        }

        void retrieve(size_t len) { //取走len长度数据，主要在于设置readerIndex_和writerIndex_的值
            assert(len <= readableBytes());
            if (len < readableBytes()) {
                readerIndex_ += len;
            } else { //len ==  readableBytes()
                retrieveAll(); //全部取走
            }
        }

        // 把可读区域的指针移动到指定位置 
        void retrieveUnitl(const char* end) { //end前的数据全取走
            assert(peek() <= end);
            assert(end <= beginWrite());
            retrieve(end - peek());
        }

        void retrieveInt64() {
            retrieve(sizeof(int64_t));
        }

        void retrieveInt32() {
            retrieve(sizeof(int32_t));
        }

        void retrieveInt16() {
            retrieve(sizeof(int16_t));
        }

        void retrieveInt8() {
            retrieve(sizeof(int8_t));
        }

        void retrieveAll() {
            readerIndex_ = kCheapPrepend; //恢复读位置
            writerIndex_ = kCheapPrepend; //恢复写位置
        }

        std::string retrieveAllAsString() { //Buffer中所有数据以字符串形式取走
            return retrieveAsString(readableBytes());
        }

        std::string retrieveAsString(size_t len) { //Buffer中len长度数据以字符串形式取走
            assert(len <= readableBytes());
            std::string result(peek(), len);
            retrieve(len);
            return result;
        }

        void append(const char* data, size_t len) {
            ensureWritableBytes(len);
            std::copy(data, data + len, beginWrite());
            hasWritten(len);
        }

        void append(const void* data, size_t len) {
            append(static_cast<const char*>(data), len);
        }

        void ensureWritableBytes(size_t len) { //确保Buffer中可以写入len长度数据
            if (writableBytes() < len) { //可写入空间不够大
                makeSpace(len); //确保可写入空间不小于len
            }

            assert(writableBytes() >= len);
        }

        char* beginWrite() {
            return begin() + writerIndex_;
        }

        const char* beginWrite() const {
            return begin() + writerIndex_;
        }

        void hasWritten(size_t len) { //写入len长度数据后，调用本函数
            assert(len <= writableBytes());
            writerIndex_ += len;
        }

        void unwrite(size_t len) { //撤销最后写入的len长度数据
            assert(len <= readableBytes());
            writerIndex_ -= len;
        }

        ///
        /// Append int64_t using network endian
        ///
        void appendInt64(int64_t x) {
            int64_t be64 = sockets::hostToNetwork64(x);
            append(&be64, sizeof(be64));
        }

        void appendInt32(int32_t x) {
            int32_t be32 = sockets::hostToNetwork32(x);
            append(&be32, sizeof(be32));
        }

        void appendInt16(int16_t x) {
            int16_t be16 = sockets::hostToNetwork16(x);
            append(&be16, sizeof(be16));
        }

        void appendInt8(int8_t x) {
            append(&x, sizeof(x));
        }

        // 读取一个int64_t类型的数据，先将数据从网络字节顺序转换成主机字节顺序，然后再返回  
        // 同时移动读取区的指针
        int64_t readInt64() {
            int64_t result = peekInt64();
            retrieveInt64();
            return result;
        }

        int32_t readInt32() {
            int32_t result = peekInt32();
            retrieveInt32();
            return result;
        }

        int16_t readInt16() {
            int16_t result = peekInt16();
            retrieveInt16();
            return result;
        }

        int8_t readInt8() {
            int8_t result = peekInt8();
            retrieveInt8();
            return result;
        }

        // 从buffer中取出一个int64_t类型的数据，然后转换成主机字节顺序
        int64_t peekInt64() const {
            assert(readableBytes() >= sizeof(int64_t));
            int64_t be64 = 0;
            ::memcpy(&be64, peek(), sizeof(be64));
            return sockets::networkToHost64(be64);
        }

        int32_t peekInt32() const {
            assert(readableBytes() >= sizeof(int32_t));
            int32_t be32 = 0;
            ::memcpy(&be32, peek(), sizeof(be32));
            return sockets::networkToHost32(be32);
        }

        int16_t peekInt16() const {
            assert(readableBytes() >= sizeof(int16_t));
            int16_t be16 = 0;
            ::memcpy(&be16, peek(), sizeof(be16));
        }

        int8_t peekInt8() const {
            assert(readableBytes() >= sizeof(int8_t));
            int8_t x = *peek();
            return x;
        }

        // 在readerIndex_之前写入int64_t类型的数据，先把它转换成网络字节顺序 
        void prependInt64(int64_t x) {
            int64_t be64 = sockets::hostToNetwork64(x);
            prepend(&be64, sizeof(be64));
        }

        void prependInt32(int32_t x) {
            int32_t be32 = sockets::hostToNetwork32(x);
            prepend(&be32, sizeof(be32));
        }

        void prependInt16(int16_t x) {
            int16_t be16 = sockets::hostToNetwork16(x);
            prepend(&be16, sizeof(be16));
        }

        void prependInt8(int8_t x) {
            prepend(&x, sizeof(x));
        }

        // 与append相对应，append在writerIndex_所指向的缓冲区写入数据（即在可读区域后面写入数据）  
        // 而prepend则是在readerIndex_所指向的缓冲区的前面写入数据（即在可读区域前面写入数据
        void prepend(const void* data, size_t len) {
            assert(len <= prependableBytes());
            readerIndex_ -= len;
            const char* d = static_cast<const char*>(data);

            //将d地址后的len数据copy到begin() + readerIndex_中
            std::copy(d, d+len, begin() + readerIndex_); //template< class InputIt, class OutputIt >
                                                          //OutputIt copy( InputIt first, InputIt last, OutputIt d_first );
        }

        //更改Buffer的大小，使其可写入空间为reserve大小
        void shrink(size_t reserve) {
            Buffer other;
            other.ensureWritableBytes(readableBytes() + reserve); //扩展readableBytes() + reserver,
            other.append(peek(), readableBytes()); //将原先的readableBytes()写入
            swap(other); //再交换
        }

        // 初始化缓冲区容量
        size_t internalCapacity() const {
            return buffer_.capacity();
        }

        // 从套接字（文件描述符）中读取数据，然后存放在缓冲区中，savedErrno保存了错误码 
        ssize_t readFd(int fd, int* savedErrno);

    private:
        // 缓冲区的起始位置
        char* begin() {
            return &*buffer_.begin();
        }

        // 缓冲区的起始位置
        const char* begin() const {
            return &*buffer_.begin();
        }

        // 分配空间（或者调整空间）resize或移动数据，使Buffer能容下len大数据
        void makeSpace(size_t len) {
            // 空间不足的情况下需要重新分配空间 
            if (writableBytes() + prependableBytes() < len + kCheapPrepend) {//可写的空间不足， prependableBytes可能很大，所以要加入比较
                buffer_.resize(writerIndex_ + len);

                // 总的剩余空间还足够，但是需要整理以方便使用  
            } else {
                // move readable data to the front, make space inside buffer  
                // readerIndex_前面的空间太多，而writerIndex_之后的空间又不足  
                // 需要将readerIndex_与writerIndex_之间的数据往前面移动，让writerIndex_之后有足够的空间 

                assert(kCheapPrepend < readerIndex_);
                size_t readable = readableBytes();
                std::copy(begin() + readerIndex_, //InputIt first
                          begin() + writerIndex_, //InputIt last
                          begin() + kCheapPrepend); //OutputIt d_first
                readerIndex_ = kCheapPrepend; //readerIndex_归位到kCheapPrepend
                writerIndex_ = readerIndex_ + readable; //更新writerIndex_
                assert(readable == readableBytes());
            }
        }

    private:
        // 缓冲区
        std::vector<char> buffer_;

        // 读写指针
        size_t readerIndex_; //int类型，是应对重新分配内存时迭代器失效
        size_t writerIndex_;

        //回车换行符
        static const char kCRLF[]; //存储匹配串内容

    };

}
}


#endif