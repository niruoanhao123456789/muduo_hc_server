#ifndef __LEVEL_HPP
#define __LEVEL_HPP
#include<iostream>
#include<string>

namespace LogModule
{
    class LogLevel
    {
    public:
        enum class Level
        {
            UNKONW = 0,
            DEBUG,
            INFO,
            WARNING,
            ERROR,
            FATAL,
            OFF
        };

        static std::string LevelToString(LogLevel::Level level)
        {
            switch(level)
            {
                case LogLevel::Level::DEBUG: return "DEBUG";
                case LogLevel::Level::INFO: return "INFO";
                case LogLevel::Level::WARNING: return "WARNING";
                case LogLevel::Level::ERROR: return "ERROR";
                case LogLevel::Level::FATAL: return "FATAL";
                case LogLevel::Level::OFF: return "OFF";
                default: return "UNKONW";
            }
        }
    };
    
} 


#endif