#include "buffer.h"
#include "socketsops.h"
#include "../base/types.h"

#include <errno.h>
#include <sys/uio.h>

using namespace kaycc;
using namespace kaycc::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;


/*
    struct iovec {
        char   *iov_base;  // Base address. 
        size_t iov_len;    // Length. 
    };
*/

// 从套接字（文件描述符）中读取数据，然后存放在缓冲区中，savedErrno保存了错误码 
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.

    // 如果可写区域的剩余空间大于65536，那么把数据直接写到写入区即可，否则还需要一个额外的扩展空间  
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    const ssize_t n = sockets::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;

    } else if (implicit_cast<size_t>(n) <= writable) { ////Buffer中可以存储所有读到的数据
        writerIndex_ += n;
    } else { //n > writable 读的数据太多，部分存储到了extrabuf
        writerIndex_ = buffer_.size();

        // 将额外空间的数据写入到缓冲区中  
        append(extrabuf, n - writable);
    }

    return n;
}
