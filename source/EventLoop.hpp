#pragma once
#include <sys/eventfd.h>
#include <iostream>
#include <functional>
#include <thread>
#include <memory>
#include <vector>
#include "Poller.hpp"
#include "TimerWheel.hpp"

namespace server_eventloop
{
    using namespace server_poller;
    using namespace server_timerwheel;
    using func_t = std::function<void()>;

    class EventLoop
    {
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

        static int CreateEventFd() 
        {
            int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
            if (efd < 0) 
            {
                
                abort();//让程序异常退出
            }
            return efd;
        }

        void ReadEventfd()
        {
            
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