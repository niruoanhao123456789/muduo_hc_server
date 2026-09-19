#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <log/Log.hpp>

namespace server_buffer
{
    using namespace LogModule;

    #define DEFAULT_BUFFER_SIZE (2 * 1024)
    #define THRESHOLD_BUFFER_SIZE (10 * 1024)
    #define INCREMENT_BUFFER_SIZE (1024)

    class ServerBuffer
    {
    public:
        ServerBuffer()
        :_buffer(DEFAULT_BUFFER_SIZE)
        ,_rindex(0)
        ,_windex(0)
        {}

        size_t Size()  { return _buffer.size(); }
        const char *Begin() const { return &*_buffer.begin(); }
        // 返回可读数据的起始地址
        const char* ReadPosition() const
        {
            return &_buffer[_rindex];
        }

        // 返回可写入的起始地址
        const char* WritePosition() const
        {
            return &_buffer[_windex];
        }

        size_t IdelSize()
        {
            return PrefixIdleSize() + SuffixIdleSize() + MiddleIdleSize();
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
            if(_windex >= _rindex)
                return _buffer.size() - _windex;
            else
                return _rindex - _windex;
        }

        bool Empty()
        {
            return _windex == _rindex;
        }

        void reset()
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
                    size_t prelen = len - suflen;
                    auto it = std::copy(content,content + suflen,WritePosition());
                    std::copy(content + suflen, content + len, it);
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
            Write(data.ReadPosition(),data.ReadableSize());
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
            char* pos = FindCRLF();
            if(!pos) return "";
            // +1是为了把换行字符也取出来
            return ReadAsString(pos - ReadPosition() + 1);
        }


    private:
        char* FindCRLF()
        {
            size_t readablelen = ReadableSize();
            if(!readablelen) return nullptr;

            size_t suflen = _buffer.size() - _rindex;
            suflen = suflen > readablelen ? readablelen : suflen;
            char* pos = (char*)memchr(ReadPosition(),'\n',suflen);
            if(pos) return pos;

            size_t prelen = readablelen - suflen;
            pos = (char*)memchr(Begin(),'\n',prelen);
            return pos;
        }

        void BuyMemory()
        {
            size_t datelen = ReadableSize();
            size_t newsize = _buffer.size() < THRESHOLD_BUFFER_SIZE ? _buffer.size() * 2 : _buffer.size() + INCREMENT_BUFFER_SIZE;
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

        //获取缓冲区起始空闲空间大小--读偏移之前的空闲空间
        size_t PrefixIdleSize() 
        {
            if(_windex >= _rindex)
            {
                return _rindex;
            }
            return 0;
        }

        size_t SuffixIdleSize()
        {
            if(_windex >= _rindex)
            {
                return _buffer.size() - _windex;
            }
            return 0;
        }

        size_t MiddleIdleSize()
        {
            if(_rindex > _windex)
            {
                return _rindex - _windex;
            }
            return 0;
        }

    private:
        std::vector<char> _buffer;
        size_t _rindex;
        size_t _windex;
    };
}