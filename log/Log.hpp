#pragma once
#include "Logger.hpp"
#include <iostream>

namespace LogModule
{
    // 1、提供获取指定日志器的全局接口（避免用户自己操作单例对象）
    Logger::ptr GetLogger(const std::string& loggername)
    {
        return LoggerManager::getInstance().GetLogger(loggername);
    }

    Logger::ptr RootLogger()
    {
        return LoggerManager::getInstance().RootLogger();
    }

    // 2、使用宏函数对日志器接口进行代理
    #define debug(fmt, ...) debug(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define infor(fmt, ...) infor(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define warnning(fmt, ...) warnning(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define error(fmt, ...) error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
    #define fatal(fmt, ...) fatal(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

    // 格式化输出
    #define LOG_DEBUG(logger, fmt, ...) (logger)->debug(fmt, ##__VA_ARGS__)
    #define LOG_INFOR(logger, fmt, ...) (logger)->infor(fmt, ##__VA_ARGS__)
    #define LOG_WARNNING(logger, fmt, ...) (logger)->warnning(fmt, ##__VA_ARGS__)
    #define LOG_ERROR(logger, fmt, ...) (logger)->error(fmt, ##__VA_ARGS__)
    #define LOG_FATAL(logger, fmt, ...) (logger)->fatal(fmt, ##__VA_ARGS__)

    // 流式输出
    #define LOG_DEBUG_STREAM(logger) LogModule::LogStreamGuard((logger),__FILE__,__LINE__,LogLevel::Level::DEBUG)
    #define LOG_INFOR_STREAM(logger) LogModule::LogStreamGuard((logger),__FILE__,__LINE__,LogLevel::Level::INFO)
    #define LOG_WARNNING_STREAM(logger) LogModule::LogStreamGuard((logger),__FILE__,__LINE__,LogLevel::Level::WARNING)
    #define LOG_ERROR_STREAM(logger) LogModule::LogStreamGuard((logger),__FILE__,__LINE__,LogLevel::Level::ERROR)
    #define LOG_FATAL_STREAM(logger) LogModule::LogStreamGuard((logger),__FILE__,__LINE__,LogLevel::Level::FATAL)


    // 3、提供宏函数，直接通过默认日志器进行日志的标准输出打印
    #define LOGD(fmt, ...) LOG_DEBUG(LogModule::RootLogger(), fmt, ##__VA_ARGS__)
    #define LOGI(fmt, ...) LOG_INFOR(LogModule::RootLogger(), fmt, ##__VA_ARGS__)
    #define LOGW(fmt, ...) LOG_WARNNING(LogModule::RootLogger(), fmt, ##__VA_ARGS__)
    #define LOGE(fmt, ...) LOG_ERROR(LogModule::RootLogger(), fmt, ##__VA_ARGS__)
    #define LOGF(fmt, ...) LOG_FATAL(LogModule::RootLogger(), fmt, ##__VA_ARGS__)

    #define LOGD_STREAM() LOG_DEBUG_STREAM(LogModule::RootLogger())
    #define LOGI_STREAM() LOG_INFOR_STREAM(LogModule::RootLogger())
    #define LOGW_STREAM() LOG_WARNNING_STREAM(LogModule::RootLogger())
    #define LOGE_STREAM() LOG_ERROR_STREAM(LogModule::RootLogger())
    #define LOGF_STREAM() LOG_FATAL_STREAM(LogModule::RootLogger())
}