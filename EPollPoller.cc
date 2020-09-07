#include <unistd.h>
#include <string.h>

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

//未添加到poller中
const int kNew = -1; //channel的成员index_初始化为-1
//channel已经添加到poller中
const int kAdded = 1;
//channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("%s %s %d epoll_create error: %d\n", __FILENAME__, __FUNCTION__, __LINE__, errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMS, ChannelList *activeChannels)
{
    //实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("%s %s %d fd total count:%d\n", __FILENAME__, __FUNCTION__, __LINE__, (int)channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMS);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {  
        LOG_INFO("%s %s %d %d events happened\n", __FILENAME__, __FUNCTION__, __LINE__, numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_INFO("%s %s %d timeout.\n", __FILENAME__, __FUNCTION__, __LINE__);
    }
    else
    {
        if (savedErrno != EINTR)
        {
            errno = savedErrno;
            LOG_ERROR("%s %s %d EPollPoller::poll error", __FILENAME__, __FUNCTION__, __LINE__);
        }
    }

    return now;
}

//被调用过程:channel update remove -> EventLoop updateChannel removeChannel -> Poller updateChannel removeChannel
/**
 *          EventLoop -> poller.poll
 * ChannelList        Poller
 *                   ChannelMap<fd, channel*> epollfd
*/
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("%s %s %d fd=%d events=%d index=%d\n", __FILENAME__, __FUNCTION__, __LINE__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);

        update(EPOLL_CTL_ADD, channel);
    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

//被调用过程:channel update remove -> EventLoop updateChannel removeChannel -> Poller updateChannel removeChannel
//从poller中移除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("%s %s %d fd=%d \n", __FILENAME__, __FUNCTION__, __LINE__, channel->fd());

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannel) const
{
    for (int i = 0; i < numEvents; numEvents++)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannel->push_back(channel);//EventLoop就拿到了它的Poller给他返回的所有发生事情的channel列表了
    }
}

//更新channel通道 epoll_ctl add/mod/delete
void EPollPoller::update(int operation, Channel *channel)
{
    int fd = channel->fd();
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("%s %s %d epoll_ctr del error:%d\n", __FILENAME__, __FUNCTION__, __LINE__, errno);
        }
        else
        {
            LOG_FATAL("%s %s %d epoll_ctr add/mod error:%d\n", __FILENAME__, __FUNCTION__, __LINE__, errno);
        }
    }
}
