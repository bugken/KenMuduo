#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"
#include "Channel.h"

/**
 * epoll的使用
 * epoll_create
 * epoll_ctl add/modify/delete
 * epoll_wait
*/
class EPollPoller:public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMS, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
    int epollFd(){ return epollfd_; }

private:
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannel) const;
    //更新channel通道
    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};