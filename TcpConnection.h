#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "TcpConnection.h"
#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer通过Acceptor有一个新用户连接，通过Acceptor函数拿到connfd打包到TCPConnection，设置相应回调，然后
 * 调用channel，再调用Poller，再调用Channel回调
*/
class TcpConnection:noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop, const std::string& name, int sockfd, 
        const InetAddress localAddr, const InetAddress peerAdder);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const{ return localAddr_; }
    const InetAddress peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disConnected() const { return state_ == kDisconnected; }

    //发送数据
    void send(const std::string& buf);
    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t hightWaterMark)
    { 
        highWaterMarkCallback_ = cb; 
        highWaterMark_ = hightWaterMark;
    }
    //连接建立
    void connectEstablished();
    //连接销毁
    void connectionDestroyed();
private:
    enum StateE{kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE s) { state_ = s; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_;//这里绝对不是baseloop,因为TCPConnection都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    //这个和Acceptor类似 Acceptor在mainLoop里面 TcpConnection在subLoop里面
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;//有新连接的时候的回调
    MessageCallback messageCallback_;//有读写消息时候的回调
    WriteCompleteCallback writeCompleteCallback_;//消息写完以后的回调
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_;//接收数据
    Buffer outputBuffer_;//发送数据
};