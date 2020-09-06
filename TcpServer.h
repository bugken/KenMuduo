#pragma once

#include <unordered_map>
#include <atomic>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"

//对外的服务器编程使用的类
class TcpServer:noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop, const InetAddress &listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    //设置底层subloop个数
    void setThreadNum(int numThreads);

    void setThreadInitCallback(const ThreadInitCallback& cb){ threadInitCallback_ = std::move(cb); }
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = std::move(cb); }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = std::move(cb); }

    //开启服务器监听
    void start();
private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* loop_;//baseloop 用户定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;//运行在mainLoop，任务是监听新连接事件

    std::unique_ptr<EventLoopThreadPool> threadPool_; //one loop per thread

    ConnectionCallback connectionCallback_;//有新连接的时候的回调
    MessageCallback messageCallback_;//有读写消息时候的回调
    WriteCompleteCallback writeCompleteCallback_;//消息写完以后的回调

    ThreadInitCallback threadInitCallback_;//线程初始化回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap conntions_;//保存所有的连接
};