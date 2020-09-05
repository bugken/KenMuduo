#include <sys/epoll.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

//EventLoop: Channel Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(0), tied_(false)
{
}

Channel::~Channel()
{
}

//防止Channel被手动remove掉,Channel还在执行回调操作
//Channel的tie方法什么时候调用过?
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * 当改变Channel所表示的fd的events事件后，update负责在poller里改变fd相应事件的epoll_ctl
 * EventLoop包含ChannelList Poller
*/
void Channel::update()
{
    //通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

//在channel所属的EventLoop中，把当前的Channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

//fd得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel发生的具体事件，由channel负责具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("%s %d channel handleEvent revents:%d", __FUNCTION__, __LINE__, revents_);
    if ((revents_ & EPOLLHUP) && (revents_ & EPOLLIN))
    {
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_) errorCallback_();
    }
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (readCallback_) readCallback_(receiveTime);
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_) writeCallback_();
    }
}

