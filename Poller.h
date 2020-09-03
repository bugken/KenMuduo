#pragma once

#include <vector>
#include <unordered_map>

#include "EventLoop.h"
#include "noncopyable.h"
#include "Channel.h"
#include "Timestamp.h"

//muduo中多路事件分发器的核心IO复用模块
class Poller:noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller();

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMS, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    //判断阐述Channel是否在当前Poller中
    virtual bool hasChannel(Channel* channel) const;

    //EventLoop可以通过该结果获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    //map的key是sockfd,value是sockfd所属的channel的通道类型
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_; //定义Poller所属的事件循环EventLoop
};