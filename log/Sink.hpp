#pragma once
#include<iostream>
#include<string>
#include<memory>
#include<sstream>
#include<fstream> 
#include<cassert>
#include<ctime>
#include<cassert>
#include"Util.hpp"
#include"LogMessage.hpp"
#include"Formatter.hpp"

namespace LogModule
{
    // 日志落地
    class LogSink
    {
    public:
        virtual ~LogSink() {}
        virtual void Log(const char* data = nullptr,size_t len = 0) = 0;
    };

    // 标准输出
    class StdOutSink : public LogSink
    {
        // 将日志消息写入标准输出
        void Log(const char* data = nullptr,size_t len = 0) override
        {
            assert(data);
            std::cout.write(data,len);
        }
    };

    // 指定文件输出
    class FileSink : public LogSink
    {
    public:
        
        FileSink(const std::string& pathname)
        :_pathname(pathname)
        {
            // 创建指定文件目录
            util::File::CreateDirectory(util::File::GetPath(_pathname));
            // 在该目录下创建并打开日志文件
            _ofs.open(_pathname,std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }

        const std::string& File() { return _pathname; }
        void Log(const char* data = nullptr,size_t len = 0) override
        {
            _ofs.write(data,len);
            assert(_ofs.good());
        }

    private:
        std::string _pathname;
        std::ofstream _ofs;
    };

    // 滚动文件的方式输出由文件大小方式滚动
    class RollBySizeSink : public LogSink
    {
    public:
        // 构造时传入文件并打开
        /***/
        RollBySizeSink(const std::string& basename,size_t maxsize = 1024*1024)
        :_basename(basename)
        ,_maxsize(maxsize)
        ,_cursize(0)
        ,_recount(0)
        {
            std::string pathname = CreateNewFile();
            // 创建指定文件目录
            util::File::CreateDirectory(util::File::GetPath(pathname));
            // 在该目录下创建并打开日志文件
            _ofs.open(pathname,std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }

        void Log(const char* data = nullptr,size_t len = 0) override
        {
            assert(data);
            if(_cursize >= _maxsize)
            {
                _ofs.close();
                std::string pathname = CreateNewFile();
                // 创建指定文件目录
                util::File::CreateDirectory(util::File::GetPath(pathname));
                // 在该目录下创建并打开日志文件
                _ofs.open(pathname,std::ios::binary | std::ios::app);
                assert(_ofs.is_open());
                _cursize = 0;
            }
            _ofs.write(data,len);
            _cursize += len;
            assert(_ofs.good());
        }

    private:
        // 进行文件大小检查，超过指定大小则创建新文件
        std::string CreateNewFile()
        {
            time_t time = util::Date::GetCurTime();
            struct tm t;
            localtime_r(&time,&t);
            std::string filename = _basename
            + std::to_string(t.tm_year + 1900)
            + std::to_string(t.tm_mon + 1)
            + std::to_string(t.tm_mday)
            + std::to_string(t.tm_hour)
            + std::to_string(t.tm_min)
            + std::to_string(t.tm_sec)
            + "[" + std::to_string(++_recount) + "]"
            + ".log";
            return filename;
        }

    private:
        std::string _basename;
        std::ofstream _ofs;
        size_t _maxsize;
        size_t _cursize;
        size_t _recount;
    };

    // 滚动文件的方式输出由时间段方式滚动
    enum TimeGap
    {
        GAP_SECOND,
        GAP_MINITUE,
        GAP_HOUR,
        GAP_DAY
    };

    class RollByTimeSink : public LogSink
    {
    public:
        // 构造时传入文件并打开
        RollByTimeSink(const std::string& basename,TimeGap gaptype)
        :_basename(basename)
        ,_gaptype(gaptype)
        {
            switch(_gaptype)
            {
            case TimeGap::GAP_SECOND: _gapsize = 1; break;
            case TimeGap::GAP_MINITUE: _gapsize = 60; break;
            case TimeGap::GAP_HOUR: _gapsize = 3600; break;
            case TimeGap::GAP_DAY: _gapsize = 3600*24; break;
            default: std::cout<<"时间段TimeGap类型错误"<<std::endl; abort();
            }
            // 获取当前时间处在第几个时间段
            time_t curtime = util::Date::GetCurTime();
            _curgap = _gapsize == 1 ? curtime : curtime % _gapsize;
            std::string pathname = CreateNewFile();
            // 创建指定文件目录
            util::File::CreateDirectory(util::File::GetPath(pathname));
            // 在该目录下创建并打开日志文件
            _ofs.open(pathname,std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }

        void Log(const char* data = nullptr,size_t len = 0) override
        {
            assert(data);
            time_t curtime = util::Date::GetCurTime();
            size_t curgap = _gapsize == 1 ? curtime : curtime % _gapsize;
            if((curgap) != _curgap)
            {
                _ofs.close();
                std::string pathname = CreateNewFile();
                // 创建指定文件目录
                util::File::CreateDirectory(util::File::GetPath(pathname));
                // 在该目录下创建并打开日志文件
                _ofs.open(pathname,std::ios::binary | std::ios::app);
                assert(_ofs.is_open());
                _curgap = curgap;
            }
            _ofs.write(data,len);
            assert(_ofs.good());
        }

    private:
        // 进行文件大小检查，超过指定大小则创建新文件
        std::string CreateNewFile()
        {
            time_t time = util::Date::GetCurTime();
            struct tm t;
            localtime_r(&time,&t);
            std::string filename = _basename
            + std::to_string(t.tm_year + 1900)
            + std::to_string(t.tm_mon + 1)
            + std::to_string(t.tm_mday);
            if(_gaptype <= GAP_HOUR)   filename += std::to_string(t.tm_hour);
            if(_gaptype <= GAP_MINITUE) filename += std::to_string(t.tm_min);
            if(_gaptype == GAP_SECOND)  filename += std::to_string(t.tm_sec);
            filename += ".log";
            return filename;
        }

    private:
        std::string _basename;
        std::ofstream _ofs;
        size_t _curgap;
        size_t _gapsize;
        TimeGap _gaptype;
    };

    
    class SinkFactory
    {
    public:
        template<typename SinkType, typename ...Args>
        static std::shared_ptr<LogSink> Create(Args&& ...args)
        {
            return std::make_shared<SinkType>(std::forward<Args>(args)...);
        }
    };
}