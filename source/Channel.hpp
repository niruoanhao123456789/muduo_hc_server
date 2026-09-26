#pragma  once
#include <iostream>
#include <functional>
#include <cstdint>
#include <sys/epoll.h>

namespace server_eventloop { class EventLoop; }

namespace server_channel
{
    class Channel
    {
        using EventLoop = server_eventloop::EventLoop;
        using EventCallBack = std::function<void()>;
    public:
        Channel(EventLoop* loop, int fd)
        :_fd(fd)
        ,_loop(loop)
        ,_events(0)
        ,_tri_events(0)
        {}

        int Fd() 
        { 
            return _fd; 
        }

        // 获取想要监控的事件
        uint32_t Events()
        {
            return _events;
        }

        // 当前是否监控了可读
        bool Readable()
        {
            return (_events & EPOLLIN);
        }

        // 当前是否监控了可写
        bool Writeable()
        {
            return (_events & EPOLLOUT);
        }

        // 启动读事件监控
        void EnableRead()
        {
            _events |= EPOLLIN;
            Update();
        }

        // 启动写事件监控
        void EnableWrite()
        {
            _events |= EPOLLOUT;
            Update();
        }

        // 关闭读事件监控
        void DisableRead()
        {
            _events &= ~EPOLLIN;
            Update();
        }

        // 关闭写事件监控
        void DisableWrite()
        {
            _events &= ~EPOLLOUT;
            Update();
        }

        // 关闭所有事件监控
        void DisableAll()
        {
            _events = 0;
            Update();
        }

        // 移除监控
        void Remove();

        void Update();

        void SetREvents(uint32_t events) 
        { 
            _tri_events = events;
        }
        
        void SetReadCallBack(const EventCallBack& cb)
        {
            _read_callback = cb;
        }

        void SetWriteCallBack(const EventCallBack& cb)
        {
            _write_callback = cb;
        }

        void SetErrorCallBack(const EventCallBack& cb)
        {
            _error_callback = cb;
        }

        void SetCloseCallBack(const EventCallBack& cb)
        {
            _close_callback = cb;
        }

        void SetEventCallBack(const EventCallBack& cb)
        {
            _event_callback = cb;
        }

        void HandleEvent()
        {
            if((_tri_events & EPOLLIN) || (_tri_events & EPOLLRDHUP) || (_tri_events & EPOLLPRI))
            {
                // 无论哪个事件，都要调用的回调函数
                if(_read_callback)
                    _read_callback();
            }

            // 对于存在释放链接的操作，每次只执行一个
            if(_tri_events & EPOLLOUT)
                if(_write_callback)
                    _write_callback();
            else if(_tri_events & EPOLLERR)
                if(_error_callback)
                    _error_callback();
            else if(_tri_events & EPOLLHUP)
                if(_close_callback)
                    _close_callback();

            if(_event_callback)
                _event_callback();
        }

    private:
        int _fd;
        EventLoop* _loop;
        uint32_t  _events;
        uint32_t  _tri_events;
        EventCallBack _read_callback;
        EventCallBack _write_callback;
        EventCallBack _error_callback;
        EventCallBack _close_callback;
        EventCallBack _event_callback;
    };
}