#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

//防止一个线程创建多个EventLoop __thread意味着thread_local，变量只在线程内
__thread EventLoop* t_loopInThisThread = nullptr;

//默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeup fd，用来notify唤醒sub reactor来处理新的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("%s %d eventfd error:%d", __FUNCTION__, __LINE__, errno);
    }
    
    return evtfd;
}

EventLoop::EventLoop():looping_(false),
    quit_(false), callPendingFunctor_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(nullptr)
{
    LOG_INFO("%s %d EventLoop created %p in thread %d\n", __FUNCTION__, __LINE__, this, threadId_);
    if (t_loopInThisThread)
    {
         LOG_FATAL("%s %d Another EventLoop %p in thread %d\n", __FUNCTION__, __LINE__, t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    
    //设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个EventLoop都将监听wakeupChannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n =read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one))
  {
    LOG_ERROR("%s %d reads %ld bytes instead of 8", __FUNCTION__, __LINE__, n);
  }
}

