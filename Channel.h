#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;
/*
*Channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPLLOUT事件
*还绑定了poller返回的具体事件
*/
class Channel:noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);
    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb){ writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb){ closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb){ errorCallback_ = std::move(cb); }

private:
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; //事件循环
    const int fd_; //fd Poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //poller返回的具体发生的事情
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体的事件回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};