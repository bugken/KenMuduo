#include <errno.h>
#include <sys/uio.h>

#include "Buffer.h"

/**从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区有大小，但是从fd上读数据的时候，却不知道tcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuff[65535] = {0};//栈上的内存空间 64K
    struct iovec vec[2];
    const size_t writable = writableBytes();//这是Buffer底层缓冲区剩余的可写空间大小

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuff;
    vec[1].iov_len = sizeof(extrabuff);

    const int iovcnt = (writable < sizeof(extrabuff)) ? 2 : 1;
    const size_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n < writable)
    {
        writerIndex_ += n;
    }
    else//extrabuff里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuff, n - writable);//从writerIndex_开始写n - writable个数据
    }
    return n;
}