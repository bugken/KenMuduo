#include <sys/types.h>  
#include <sys/socket.h>
#include <errno.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0)
    {
        LOG_FATAL("%s %s %d listen socket create failed errno:%d.\n", __FILENAME__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort)
    :listenning_(false), 
    loop_(loop),
    acceptSocket_(createNonblocking()),
    acceptChannel_(loop, acceptSocket_.fd())
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reusePort);
    acceptSocket_.bindAddress(listenAddr);
    //TcpServer::start() Acceptor.listen() 有新用户的连接，要执行一个回调(connfd->channel->subloop)
    //baseloop->acceptChannel(listenfd)->handleRead
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();//listen
    acceptChannel_.enableReading();
}

//listenfd事件发生，就有新用户连接了
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd > 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);//轮训找到subloop，唤醒分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s %s %d accept error, errno:%d.\n", __FILENAME__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s %s %d socket fd reached limit.\n", __FILENAME__, __FUNCTION__, __LINE__);
        }
    }
}