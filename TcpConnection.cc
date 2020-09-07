#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s %s %d TcpConnection is null.\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int sockfd, 
    const InetAddress localAddr, const InetAddress peerAdder)
    :loop_(CheckLoopNotNull(loop)),
    name_(name),
    state_(kConnecting), 
    reading_(true),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAdder),
    highWaterMark_(64*1024*1024)
{
    //下面给Channel设置相应的回调函数，poller给channel通知感兴趣的事件，channel会回调相应的操作函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_INFO("%s %s %d TcpConnection::ctor[%s] at fd %d\n", __FILE__, __FUNCTION__, __LINE__, name.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("%s %s %d TcpConnection::dtor[%s] at fd %d state %d\n", __FILE__, __FUNCTION__, __LINE__, name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0)
    {
        //已建立连接的用户，有可读事件发生，调用用户传入的回调操作
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n = 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("%s %s %d TcpConnection handle error\n", __FILE__, __FUNCTION__, __LINE__);
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    //loop对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("%s %s %d TcpConnection handleWrite\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
    else
    {
        LOG_ERROR("%s %s %d TcpConnection fd %d is down, no more writing\n", __FILE__, __FUNCTION__, __LINE__, channel_->fd());
    }
}

//调用过程 Poller -> Channel::closeCallback -> TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("%s %s %d fd %d state %d\n", __FILE__, __FUNCTION__, __LINE__, channel_->fd(), (int)state_);
    setState(kDisconnecting);
    channel_->disableWriting();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);//执行连接关闭的回调
    closeCallback_(connPtr);//关闭连接的回调,这里执行的TcpServer::removeConncection回调方法
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("%s %s %d TcpConnection handle Error, name %s SO_ERROR %d\n", __FILE__, __FUNCTION__, __LINE__, name_.c_str(), err);
}

void TcpConnection::send(const std::string& buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 * 发送数据，应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置的水位回调
*/
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    //之前调用过connection的shutdown，不能再发送了
    if (state_ == kDisconnected)
    {
        LOG_ERROR("%s %s %d disconnected, give up writing.\n", __FILE__, __FUNCTION__, __LINE__);
        return ;
    }
    
    //表示channel第一次开始写数据，而且缓冲区没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len = nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                //既然数据发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else//nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("%s %s %d error\n", __FILE__, __FUNCTION__, __LINE__);
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    /**
     * 说明当前write没有全部发送出去，剩余数据需要保存到缓冲区，然后给channel注册EPOLLOUT事件
     * poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel调用handleWrite回调方法
     * 也就是调用TcpConnection::handleWrite()方法把发送缓冲区数据发送完毕
    */
    if (!faultError && remaining > 0)
    {
        //目前发送缓冲区待发送的数据长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining  >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableReading();//这里一定要注册channel的写事件，否则poller不会给channel通知epollout
        }
    }
}

//连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();//向poller注册channel的epollin事件

    //新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

//连接销毁
void TcpConnection::connectionDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();//把channel的所有感兴趣的事件从poller中del掉

        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller中删除掉
}

//关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())//当前outputBuffer缓冲区数据已经全部发送完成
    {
        socket_->shutdownWrite();//关闭写端
    }
}