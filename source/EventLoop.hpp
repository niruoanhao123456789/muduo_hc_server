#pragma once
#include <sys/eventfd.h>
#include <iostream>
#include <functional>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>
#include <cassert>
#include "Poller.hpp"
#include "TimerWheel.hpp"

namespace server_eventloop
{
    using namespace server_poller;
    using namespace server_timerwheel;
    
    class EventLoop
    {
        using func_t = std::function<void()>;
    public:
        EventLoop()
        :_thread_id(std::this_thread::get_id())
        ,_event_fd(CreateEventFd())
        ,_event_channel(new Channel(this, _event_fd))
        ,_timer_wheel(this)
        {
            // 给eventfd添加可读事件回调函数，读取eventfd事件通知次数
            _event_channel->SetReadCallBack(std::bind(&EventLoop::ReadEventfd,this));
            // 启动eventfd的读事件监控
            _event_channel->EnableRead();
        }

        void Start()
        {
            while(1)
            {
                // 时间监控
                std::vector<Channel*> actives;
                _poller.Poll(&actives);

                // 事件处理
                for(auto& channel:actives)
                {
                    channel->HandleEvent();
                }

                // 执行任务
                RunAllTask();
            }
        }

        // 判断当前线程是否为EventLoop对应的线程
        bool IsInLoop()
        {
            return _thread_id == std::this_thread::get_id();
        }

        void AssertInLoop()
        {
            assert(_thread_id == std::this_thread::get_id());
        }

        // 判断将要执行的任务是否处于在当前线程中，如果是则执行，不是则入任务队列
        void RunInLoop(const func_t& cb)
        {
            if(IsInLoop())
                cb();
            else
                PushQueueInLoop(cb);
        }

        void PushQueueInLoop(const func_t& cb)
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _tasks.emplace_back(cb);
            }

            // 唤醒有可能因为没有事件就绪，而导致的epoll阻塞；
            // 其实就是给eventfd写入一个数据，这样eventfd就会触发可读事件
            WakeUpEventfd();
        }

        // 添加/修改描述符的事件监控
        void UpdateEvent(Channel* channel)  
        {
            _poller.UpdateEvent(channel);
        }

        // 移除描述符的监控
        void RemoveEvent(Channel* channel)
        {
            _poller.RemoveEvent(channel);
        }

        void TimerAdd(uint64_t id,uint32_t delay,const func_t& cb)
        {
            _timer_wheel.TimerAdd(id,delay,cb);
        }

        void TimerRefresh(uint64_t id)
        {
            _timer_wheel.TimerRefresh(id);
        }

        void TimerCancel(uint64_t id)
        {
            _timer_wheel.TimerCancel(id);
        }

        bool IsTimerExist(uint64_t id)
        {
            return _timer_wheel.IsTimerExist(id);
        }

        void RunAllTask()
        {
            std::vector<func_t> func;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _tasks.swap(func);
            }

            for(auto& f:func)
            {
                f();
            }
        }

        static int CreateEventFd() 
        {
            int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
            if (efd < 0) 
            {
                LOG_FATAL_STREAM(GetLogger("ServerLogger")) << "Create eventfd failed!" ;
                abort();
            }
            return efd;
        }

        void ReadEventfd()
        {
            uint64_t res = 0;
            int ret = read(_event_fd,&res,sizeof(res));
            if(ret<0)
            {
                if(errno == EINTR || errno == EAGAIN)
                    return;
                LOG_FATAL_STREAM(GetLogger("ServerLogger")) << "Read eventfd failed!" ;
                abort();
            }
        }

        void WakeUpEventfd()
        {
            uint64_t val = 1;
            int ret = write(_event_fd,&val,sizeof(val));
            if(ret<0)
            {
                if(errno == EINTR)
                    return;
                LOG_FATAL_STREAM(GetLogger("ServerLogger")) << "Wake up eventfd failed!" ;
                abort();
            }
        }

    private:
        std::thread::id _thread_id;
        int _event_fd; // eventfd唤醒IO事件监控有可能导致的阻塞
        std::unique_ptr<Channel> _event_channel;
        Poller _poller;
        std::vector<func_t> _tasks; // 任务池
        std::mutex _mutex; // 实现任务池操作的线程安全
        TimerWheel _timer_wheel;
    };
}

namespace server_channel
{
    // 此处 EventLoop 已完整定义，故在此处定义依赖它的成员函数
    inline void Channel::Update()
    {
        _loop->UpdateEvent(this);
    }

    inline void Channel::Remove()
    {
        _loop->RemoveEvent(this);
    }
}