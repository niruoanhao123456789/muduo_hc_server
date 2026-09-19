#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include "Util.hpp"

namespace LogModule
{
// 异步日志缓冲区的实现
#define DEFAULT_BUFFER_SIZE (10 * 1024 * 1024)
#define THRESHOLD_BUFFER_SIZE (100 * 1024 * 1024)
#define INCREMENT_BUFFER_SIZE (10 * 1024 * 1024)

    class AsyncBuffer
    {
    public:
        AsyncBuffer()
            : _buffer(DEFAULT_BUFFER_SIZE), _rindex(0), _windex(0)
        {
        }

        size_t Size() { return _buffer.size(); }
        // 向缓冲区写入数据
        void Push(const char *data, size_t len)
        {
            assert(data);
            // 缓冲区满两种策略：1、退出阻塞 2、扩容（用于极限测试）
            while (WriteableSize() < len)
            {
                BuyMemory();
                // return;
            }

            std::copy(data, data + len, &_buffer[_windex]);
            MoveWindex(len);
        }

        // 返回可读数据的起始地址
        const char *Begin() const
        {
            return &_buffer[_rindex];
        }

        // 返回可读写数据长度
        size_t ReadableSize()
        {
            // 非环形缓冲区
            return (_windex - _rindex);
        }

        size_t WriteableSize()
        {
            return (_buffer.size() - _windex);
        }

        // 重置读写位置初始化缓冲区
        void reset()
        {
            _rindex = _windex = 0;
        }

        // 对读写与任务buffer进行交换
        void Swap(AsyncBuffer &buffer)
        {
            _buffer.swap(buffer._buffer);
            std::swap(_rindex, buffer._rindex);
            std::swap(_windex, buffer._windex);
        }

        // 判断缓冲区是否为空
        bool empty()
        {
            return (_windex == _rindex);
        }

        // 对读写指针进行偏移
        void MoveRindex(size_t len)
        {
            assert(len <= ReadableSize());
            _rindex += len;
        }

    private:
        void MoveWindex(size_t len)
        {
            assert(len <= WriteableSize());
            _windex += len;
        }

        void BuyMemory()
        {
            size_t newsize = _buffer.size() < THRESHOLD_BUFFER_SIZE ? _buffer.size() * 2 : _buffer.size() + INCREMENT_BUFFER_SIZE;
            _buffer.resize(newsize);
        }

        std::vector<char> Getbuffer()
        {
            return _buffer;
        }

    private:
        std::vector<char> _buffer;
        // 缓冲区的读写指针
        size_t _rindex;
        size_t _windex;
    };
}