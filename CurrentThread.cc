#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cachedTid()
    {
        if(t_cachedTid == 0)
        {
            //通过Linux系统调用获取当前线程的Tid
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}