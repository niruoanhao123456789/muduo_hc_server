#pragma once
#include<mutex>
#include<condition_variable>
#include<memory>
#include<cassert>
#include<thread>
#include<atomic>
#include<functional>
#include"Buffer.hpp"

namespace LogModule
{
    using func_t = std::function<void(AsyncBuffer&)>;

    enum class AsyncStatus
    {
        ASYNC_UNSAFE,       // 缓冲区满，则扩容极限测试
        ASYNC_SAFE          // 缓冲区满，则阻塞等待
    };
    
    class AsyncLooper
    {
    public:
        AsyncLooper(func_t cb, AsyncStatus status = AsyncStatus::ASYNC_SAFE)
        :_status(status)
        ,_isrunning(true)
        ,_thread(std::thread(&AsyncLooper::ThreadEntry,this))
        ,_cb(cb)
        {
        }

        ~AsyncLooper()
        {
            Stop();
        }

        void Stop()
        {
            _isrunning = false;
            _cond_con.notify_all();
            _thread.join();
        }

        void Push(const char* data, size_t len)
        {
            assert(data);
            // 无限扩容存在不安全，固定大小则生产者缓冲区满则阻塞等待
            std::unique_lock<std::mutex> lock(_lock);
            // 生产者缓冲区满则阻塞等待
            if(_status == AsyncStatus::ASYNC_SAFE)
                _cond_pro.wait(lock,[&]() {return _pro_buffer.WriteableSize() >= len;});

            _pro_buffer.Push(data,len);

            _cond_con.notify_one();
        }

    private:
        // 线程入口函数，对于消费者缓冲区进行数据处理，处理完毕后，初始化缓冲区，交换生产者与消费者的缓冲区
        void ThreadEntry()
        {
            while(1)
            {
                // 1、判断生产者缓冲区是否有数据，有则交换，否则阻塞等待
                // 为互斥锁设置生命周期，缓冲区交换完后就解锁
                {
                    std::unique_lock<std::mutex> lock(_lock);
                    // 退出前被唤醒，确保生产者缓冲区没有数据后就退出
                    if(!_isrunning && _pro_buffer.empty())  { break; }
                    // 退出前被唤醒与有数据才继续往下运行
                    _cond_con.wait(lock,[&]() {return !_isrunning || !_pro_buffer.empty();});
                    _con_buffer.Swap(_pro_buffer);
                
                    // 2、唤醒生产者
                    if(_status == AsyncStatus::ASYNC_SAFE)
                        _cond_pro.notify_all();
                }
                
                // 3、处理数据
                _cb(_con_buffer);
                // 4、初始化缓冲区
                _con_buffer.reset();
            }
        }

    private:
        AsyncStatus _status;
        std::atomic<bool> _isrunning;
        AsyncBuffer _pro_buffer;
        AsyncBuffer _con_buffer;
        std::mutex _lock;
        std::condition_variable _cond_pro;
        std::condition_variable _cond_con;
        std::thread _thread; // 异步日志工作器对应的工作线程
        func_t _cb;
    };
}