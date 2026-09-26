#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <unistd.h>
#include <sys/timerfd.h>

#include "Channel.hpp"
#include "../log/Log.hpp"

namespace server_eventloop { class EventLoop; }

namespace server_timerwheel
{
    using namespace LogModule;
    using namespace server_channel;
    using tastfunc_t = std::function<void()>;
    using releasefunc_t = std::function<void()>;
    using EventLoop = server_eventloop::EventLoop;
    
    class TimerTask
    {
    public:
        TimerTask(uint64_t id, uint32_t delay, const tastfunc_t& cb)
        :_id(id)
        ,_timeout(delay)
        ,_task_cb(cb)
        ,_canceled(false)
        {}

        void Cancel()
        {
            _canceled = true;
        }

        void SetRelease(const releasefunc_t& cb)
        {
            _release = cb;
        }

        uint32_t DelayTime()
        {
            return _timeout;
        }

        ~TimerTask()
        {
            if(!_canceled)
                _task_cb();
            _release();
        }

    private:
        uint64_t _id;           // 定时器任务对象ID
        uint32_t _timeout;      // 定时任务的超时时间     
        bool _canceled;         // false-表示没有被取消， true-表示被取消
        tastfunc_t _task_cb;    // 定时器对象要执行的定时任务
        releasefunc_t _release; // 用于删除TimerWheel中保存的定时器对象信息
    };

    class TimerWheel
    {
    public:
        TimerWheel(EventLoop* loop)
        :_timerfd(CreateTimerfd())
        ,_curtick(0)
        ,_capacity(60)
        ,_wheel(_capacity)
        ,_loop(loop)
        ,_timer_channel(new Channel(_loop,_timerfd))
        {
            _timer_channel->SetReadCallBack(std::bind(&TimerWheel::OnTime,this));
            _timer_channel->EnableRead(); // 启动事件监控
        }

        // 定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题
        // 如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行
        void TimerAdd(uint64_t id, uint32_t delay, const tastfunc_t& cb)
        {
            TimerAddInLoop(id,delay,cb);
        }

        // 刷新/延迟定时任务
        void TimerRefresh(uint64_t id)
        {
            TimerRefreshInLoop(id);
        }

        void TimerCancel(uint64_t id)
        {
            TimerCannelInLoop(id);
        }

        // 这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行
        bool IsTimerExist(uint64_t id) 
        {
            auto it = _timers.find(id);
            if (it == _timers.end()) 
            {
                return false;
            }
            return true;
        }

    private:
        void RemoveTimer(uint64_t id)
        {
            auto it = _timers.find(id);
            if(it != _timers.end())
                _timers.erase(it);
        }

        static int CreateTimerfd()
        {
            int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
            if(timerfd < 0)
            {
                LOG_FATAL_STREAM(GetLogger("ServerLogger")) << "Create timerfd failed!";
                abort();
            }

            struct itimerspec itime;
            itime.it_value.tv_sec = 1;
            itime.it_value.tv_nsec = 0;//第一次超时时间为1s后
            itime.it_interval.tv_sec = 1; 
            itime.it_interval.tv_nsec = 0; //第一次超时后，每次超时的间隔时
            timerfd_settime(timerfd, 0, &itime, nullptr);

            return timerfd;
        }

        int ReadTimefd()
        {
            uint64_t times;
            int ret = read(_timerfd,&times,8);
            if(ret<0)
            {
                LOG_FATAL_STREAM(GetLogger("ServerLogger")) << "Read timefd failed!";
                abort();
            }

            return times;
        }

        // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
        void RunTimerTask()
        {
            _curtick = (_curtick + 1) % _capacity;
            _wheel[_curtick].clear();
        }

        void OnTime()
        {
            int times = ReadTimefd();
            for(int i=0;i<times;i++)
            {
                RunTimerTask();
            }
        }

        void TimerAddInLoop(uint64_t id,uint32_t delay, const tastfunc_t& cb)
        {
            PtrTask pt(new TimerTask(id,delay,cb));
            pt->SetRelease(std::bind(&TimerWheel::RemoveTimer,this,id));
            int pos = (_curtick + delay) % _capacity;
            _wheel[pos].emplace_back(pt);
            _timers[id] = WeakTask(pt);
        }

        void TimerRefreshInLoop(uint64_t id)
        {
            // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
            auto it = _timers.find(id);
            if(it == _timers.end())
                return; // 没找着定时任务，没法刷新，没法延迟
            
            // lock获取weak_ptr管理的对象对应的shared_ptr
            PtrTask pt = it->second.lock(); 
            int delay = pt->DelayTime();
            int pos = (_curtick + delay) % _capacity;
            _wheel[pos].emplace_back(pt);
        }

        void TimerCannelInLoop(uint64_t id)
        {
            auto it = _timers.find(id);
            if(it == _timers.end())
                return; // 没找着定时任务，没法刷新，没法延迟

            PtrTask pt = it->second.lock();
            if(pt)
                pt->Cancel();
        }

    private:
        using WeakTask = std::weak_ptr<TimerTask>;
        using PtrTask  = std::shared_ptr<TimerTask>;
        int _timerfd;       // 定时器描述符--可读事件回调就是读取计数器，执行定时任务
        size_t _curtick;    // 当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
        size_t _capacity;   // 表盘最大数量---其实就是最大延迟时间
        std::vector<std::vector<PtrTask>> _wheel;
        std::unordered_map<uint64_t,WeakTask> _timers;
        EventLoop* _loop;
        std::unique_ptr<Channel> _timer_channel;
    };
}