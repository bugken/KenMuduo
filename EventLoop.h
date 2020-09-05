#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

//事件循环类，其中包括两个主要大模块 Channel Poller(epoll的抽象)
class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor db);

    //唤醒loop所在的线程
    void wakeup();

    //EventLoop的方法->Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    void hasChannel(Channel* channel);

    //判断EventLoop对象是否在自己线程里面
    bool isInLoopThread()const { return threadId_ == CurrentThread::tid(); }
private:
    void handleRead();//wake up
    void doPendingFunctor();//执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;//原子操作，通过CAS实现
    std::atomic_bool quit_;//退出loop循环
    const pid_t threadId_;//当前loop所在线程的id
    Timestamp pollReturnTime_;//记录poll返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;//当mainLoop获取一个新用户的channel，通过轮训算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;//封装wakeupfd_的channel

    ChannelList activeChannels_;

    std::atomic_bool callPendingFunctor_;//当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;//存储loop需要执行的所有的回调操作
    std::mutex mutex_;//互斥锁用来保护上面vector容器的线程安全操作
};