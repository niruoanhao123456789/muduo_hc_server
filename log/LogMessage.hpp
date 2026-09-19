#ifndef __LOGMESSAGE_HPP
#define __LOGMESSAGE_HPP
#include<memory>
#include<thread>
#include"Util.hpp"
#include"Level.hpp"
#include<sstream>

namespace LogModule
{
    class LogMessage
    {
    public:
        size_t _line;               //行号
        time_t _ctime;              //时间
        std::thread::id _tid;       //线程ID
        std::string _name;          //日志器名称
        std::string _file;          //文件名
        std::string _payload;       //日志消息
        LogLevel::Level _level;     //日志等级
        
        LogMessage(const std::string& name, const std::string file, size_t line, std::string&& payload, LogLevel::Level level)
        : _line(line)
        , _ctime(util::Date::GetCurTime())
        , _tid(std::this_thread::get_id())
        , _name(name)
        , _file(file)
        , _payload(std::move(payload))
        , _level(level)
        {

        }
    };
}


#endif