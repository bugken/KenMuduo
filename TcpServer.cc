#include <functional>

#include "TcpServer.h"
#include "Logger.h"

EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s %d mainLoop is null.\n", __FUNCTION__, __LINE__);
    }
    return loop;
}    

TcpServer::TcpServer(EventLoop* loop, const InetAddress &listenAddr, const std::string& nameArg, Option option = kNoReusePort)
    :loop_(CheckLoopNotNull(loop)), ipPort_(listenAddr.toIpPort()), name_(nameArg), 
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), 
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(),
    messageCallback_(),
    nextConnId_(1)
{
    //当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{

}

//开启服务器监听 loop.loop()
void TcpServer::start()
{
    if (started_++ == 0)//防止一个TCPServer对象呗start多次
    {
        threadPool_->start(threadInitCallback_);//启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

//设置底层subloop个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{

}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{

}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{

}

