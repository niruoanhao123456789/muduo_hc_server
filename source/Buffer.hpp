#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include "../log/Log.hpp"

namespace server_buffer
{
    using namespace LogModule;

    #define DEFAULT_SERVER_BUFFER_SIZE (2 * 1024)
    #define THRESHOLD_SERVER_BUFFER_SIZE (10 * 1024)
    #define INCREMENT_SERVER_BUFFER_SIZE (1024)

    class ServerBuffer
    {
    public:
        ServerBuffer()
        :_buffer(DEFAULT_SERVER_BUFFER_SIZE)
        ,_rindex(0)
        ,_windex(0)
        {}

        size_t Size()  { return _buffer.size(); }
        char *Begin()  { return &*_buffer.begin(); }
        // 返回可读数据的起始地址
        char* ReadPosition()
        {
            assert(_buffer.size());
            return &_buffer[_rindex];
        }

        // 返回可写入的起始地址
        char* WritePosition()
        {
            assert(_buffer.size());
            return &_buffer[_windex];
        }

        size_t ReadableSize()
        {
            if(_windex >= _rindex)
                return _windex - _rindex;
            else
                return (_buffer.size() - _rindex) + _windex; 
        }

        size_t WriteableSize()
        {
            // 保留一个空槽用于区分满/空，实际可写容量为 Size()-1-ReadableSize()
            return _buffer.size() - 1 - ReadableSize();
        }

        bool Empty()
        {
            return _windex == _rindex;
        }

        bool Full()
        {
            return (_windex + 1) % _buffer.size() == _rindex;
        }

        void Reset()
        {
            _rindex = _windex = 0;
        }

        // 写入数据
        void Write(const void* data,size_t len)
        {
            assert(data);
            if(!len) return;
            while(WriteableSize() < len)
            {
                BuyMemory();
            }

            const char* content = static_cast<const char *>(data);
            if(_rindex > _windex)
                std::copy(content, content + len, WritePosition());
            else
            {
                size_t suflen = _buffer.size() - _windex;
                if(suflen >= len)
                    std::copy(content, content + len, WritePosition());
                else
                {
                    // 尾部空间不足，先写尾部再回绕写到缓冲区开头
                    std::copy(content, content + suflen, WritePosition());
                    std::copy(content + suflen, content + len, _buffer.begin());
                }
            }
            MoveWindex(len);
        }

        void WriteString(const std::string& data)
        {
            Write(data.c_str(),data.size());
        }

        void WriteServerBuffer(ServerBuffer& data)
        {
            if(this == &data)
            {
                // 自搬运别名：先做非消耗快照再追加，避免扩容使源指针失效
                std::string snap = Snapshot();
                Write(snap.data(), snap.size());
                return;
            }
            size_t readable = data.ReadableSize();
            if(!readable) return;
            if(data._windex >= data._rindex)
                Write(data.ReadPosition(), readable);
            else
            {
                // 回绕源分两段写入，避免越过缓冲区尾部读取非连续内存
                Write(data.ReadPosition(), data._buffer.size() - data._rindex);
                Write(data.Begin(), data._windex);
            }
        }

        // 读取数据
        void Read(void* buf, size_t len)
        {
            assert(len <= ReadableSize());
            if(_windex >= _rindex)
                std::copy(ReadPosition(),ReadPosition()+len,(char*)buf);
            else
            {   
                size_t suflen = _buffer.size() - _rindex;
                if(suflen >= len)
                    std::copy(ReadPosition(),ReadPosition()+len,(char*)buf);
                else
                {
                    size_t prelen = len - suflen;
                    auto it = std::copy(ReadPosition(),ReadPosition()+suflen,(char*)buf);
                    std::copy(_buffer.begin(),_buffer.begin()+prelen,it);
                }
            }
            MoveRindex(len);
        }

        std::string ReadAsString(size_t len)
        {
            assert(len <= ReadableSize());
            std::string ret;
            ret.resize(len);
            Read(&ret[0],len);
            return ret;
        }

        std::string GetLine()
        {
            size_t off = FindCRLF();
            if(off == std::string::npos) return "";
            // +1是为了把换行字符也取出来
            return ReadAsString(off + 1);
        }


    private:
        // 返回 '\n' 距离可读起始位置的逻辑偏移，未找到返回 npos
        // 使用逻辑偏移而非裸指针，避免回绕时出现负偏移
        size_t FindCRLF()
        {
            size_t readablelen = ReadableSize();
            if(!readablelen) return std::string::npos;

            size_t suflen = _buffer.size() - _rindex;
            suflen = suflen > readablelen ? readablelen : suflen;
            char* pos = (char*)memchr(ReadPosition(),'\n',suflen);
            if(pos) return pos - ReadPosition();

            size_t prelen = readablelen - suflen;
            pos = (char*)memchr(Begin(),'\n',prelen);
            if(pos) return suflen + (pos - Begin());
            return std::string::npos;
        }

        // 返回当前可读数据的副本，不改变读写指针及缓冲区状态
        std::string Snapshot()
        {
            size_t datelen = ReadableSize();
            std::string s;
            s.resize(datelen);
            if(!datelen) return s;

            if(_windex >= _rindex)
                std::copy(ReadPosition(),ReadPosition()+datelen,&s[0]);
            else
            {
                size_t suflen = _buffer.size() - _rindex;
                std::copy(ReadPosition(),ReadPosition()+suflen,&s[0]);
                std::copy(_buffer.begin(),_buffer.begin()+(datelen-suflen),&s[0]+suflen);
            }
            return s;
        }

        void BuyMemory()
        {
            size_t datelen = ReadableSize();
            size_t newsize = _buffer.size() < THRESHOLD_SERVER_BUFFER_SIZE ? _buffer.size() * 2 : _buffer.size() + INCREMENT_SERVER_BUFFER_SIZE;
            // 把现有数据按逻辑顺序拷贝到新缓冲区开头
            if(_windex >= _rindex)
            {
                _buffer.resize(newsize);
                std::copy(_buffer.begin()+_rindex,_buffer.begin()+_windex,_buffer.begin());
            }
            else
            {
                std::vector<char> newbuffer(newsize);
                auto it = std::copy(_buffer.begin()+_rindex,_buffer.end(),newbuffer.begin());
                std::copy(_buffer.begin(),_buffer.begin()+_windex,it);
                _buffer.swap(newbuffer);
            }
            _rindex = 0;
            _windex = datelen;
            LOGI_STREAM <<"ServerBuffer resize: "<<newsize;
        }

        void MoveRindex(size_t len)
        {
            assert(len <= ReadableSize());
            _rindex = (_rindex + len) % _buffer.size();
        }

        void MoveWindex(size_t len)
        {
            assert(len <= WriteableSize());
            _windex = (_windex + len) % _buffer.size();
        }

    private:
        std::vector<char> _buffer;
        size_t _rindex;
        size_t _windex;
    };
}