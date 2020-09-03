#include "Poller.h"

Poller::Poller(EventLoop* loop):ownerLoop_(loop)
{
}

Poller::~Poller() = default;

//判断阐述Channel是否在当前Poller中
bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}