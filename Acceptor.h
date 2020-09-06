#pragma once 
#include <functional>
#include <memory>

#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include "Socket.h"
#include "noncopyable.h"
#include "Channel.h"

class Acceptor:noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort = true);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb){ newConnectionCallback_ = std::move(cb);}
    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
    int idleFd_;
};