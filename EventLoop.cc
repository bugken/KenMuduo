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
    wakeupChannel_(new Channel(this, wakeupFd_))
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

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("%s %d EventLoop %p start looping\n", __FUNCTION__, __LINE__, this);

    while (!quit_)
    {
        activeChannels_.clear();
        //监听两类fd，一种为client的fd，一种是wakeup的fd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel* channel : activeChannels_)
        {
            //Poller监听哪些channel发生事件，上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程mainLoop主要做accept操作，得到fd，使用channel打包fd，mainLoop对已有的fd会唤醒一个subloop去处理事件
         * mainLoop实现注册一个回调cb(需要subloop执行)，mainLoop wakeup subloop以后执行下面的方法，执行mainLoop注册的cb操作
        */
        doPendingFunctor();
    }
    looping_ = false;
    LOG_INFO("%s %d EventLoop %p stop looping\n", __FUNCTION__, __LINE__, this);
}

//退出事件循环 1.loop在自己的线程中调用quit 2.在非Loop的线程中调用Loop的quit
/**
 *          mainLoop
 * 
 * subLoop1 subLoop2 subLoop3
*/
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())//如果在其他线程中调用了quit，在一个subloop中调用mainLoop的quit
    {
        wakeup();
    }
    
}
