#pragma once
#include"Util.hpp"
#include"LogMessage.hpp"
#include"Level.hpp"
#include"Formatter.hpp"
#include"Sink.hpp"
#include"Looper.hpp"
#include<atomic>
#include<mutex>
#include<vector>
#include<cstdarg>
#include<unordered_map>
#include<cassert>

/*
    日志器模块
    1、日志器基类
    2、同步与异步子类
*/
namespace LogModule
{
    class Logger
    {
    public:
        friend class LogStreamGuard;
        using ptr = std::shared_ptr<Logger>;
        Logger(const std::string& loggername, 
            std::shared_ptr<Formatter>& formatter, 
            LogLevel::Level limitlevel, 
            std::vector<std::shared_ptr<LogSink>>& sinks)
        :_loggername(loggername)
        ,_formatter(formatter)
        ,_limitlevel(limitlevel)
        ,_sinks(sinks.begin(),sinks.end())
        {
        }

        const std::string& loggerName() { return _loggername; } 
        LogLevel::Level loggerLevel() { return _limitlevel; }

        // 用于完成对应等级日志消息的的构建与格式化并输出
        void debug(const std::string& file ,size_t line, const std::string& format, ...)
        {
            // 1、判断当前日志收否达到输出等级
            if(LogLevel::Level::DEBUG < _limitlevel) { return; }

            // 2、对format和不定参进行组织，得到日志消息字符串
            char* res = nullptr;
            va_list al;
            va_start(al,format);
            int n = vasprintf(&res,format.c_str(),al);
            if(n < 0)
            {
                std::cout<<"vasprintf fail!"<<std::endl;
            }
            va_end(al);
            
            Serialize(file,line,LogLevel::Level::DEBUG ,res);
            
            free(res);
            res=nullptr;
        }

        void infor(const std::string& file ,size_t line, const std::string& format, ...)
        {
            if(LogLevel::Level::INFO < _limitlevel) { return; }

            char* res = nullptr;
            va_list al;
            va_start(al,format);
            int n = vasprintf(&res,format.c_str(),al);
            if(n < 0)
            {
                std::cout<<"vasprintf fail!"<<std::endl;
            }
            va_end(al);
            
            Serialize(file,line,LogLevel::Level::INFO,res);
            
            free(res);
            res=nullptr;
        }

        void warnning(const std::string& file ,size_t line, const std::string& format, ...)
        {
            if(LogLevel::Level::WARNING < _limitlevel) { return; }

            char* res = nullptr;
            va_list al;
            va_start(al,format);
            int n = vasprintf(&res,format.c_str(),al);
            if(n < 0)
            {
                std::cout<<"vasprintf fail!"<<std::endl;
            }
            va_end(al);
            
            Serialize(file,line,LogLevel::Level::WARNING,res);
            
            free(res);
            res=nullptr;
        }

        void error(const std::string& file ,size_t line, const std::string& format, ...)
        {
            if(LogLevel::Level::ERROR < _limitlevel) { return; }

            char* res = nullptr;
            va_list al;
            va_start(al,format);
            int n = vasprintf(&res,format.c_str(),al);
            if(n < 0)
            {
                std::cout<<"vasprintf fail!"<<std::endl;
            }
            va_end(al);
            
            Serialize(file,line,LogLevel::Level::ERROR,res);
            
            free(res);
            res=nullptr;
        }

        void fatal(const std::string& file ,size_t line, const std::string& format, ...)
        {
            if(LogLevel::Level::FATAL < _limitlevel) { return; }

            char* res = nullptr;
            va_list al;
            va_start(al,format);
            int n = vasprintf(&res,format.c_str(),al);
            if(n < 0)
            {
                std::cout<<"vasprintf fail!"<<std::endl;
            }
            va_end(al);
            
            Serialize(file,line,LogLevel::Level::FATAL,res);
            
            free(res);
            res=nullptr;
        }

    protected:
        void Serialize(const std::string& file ,size_t line ,LogLevel::Level level,std::string&& res)
        {
            // 3、根据消息字符串构建LogMessage对象
            LogMessage msg(_loggername,file,line,std::move(res),level);

            // 4、对LogMessage对象进行格式化，得到格式化字符串
            std::stringstream ss;
            _formatter->format(ss,msg);

            // 5、日志落地
            log(ss.str().c_str(),ss.str().size());
        }

        // 用于实际日志落地输出
        virtual void log(const char* data, size_t len) = 0;
        
    protected:
        std::mutex _lock;
        std::string _loggername;
        std::shared_ptr<Formatter> _formatter;
        std::atomic<LogLevel::Level> _limitlevel;
        std::vector<std::shared_ptr<LogSink>> _sinks;
    };

    // 流式日志临时对象：宏构造一个带调用点信息的对象，通过 operator<< 把内容攒进
    // 内部缓冲，语句结束时(;)对象析构，在析构中统一走 Serialize 落地(RAII)
    class LogStreamGuard
    {
    public:
        LogStreamGuard(Logger::ptr logger, const std::string& file, size_t line, LogLevel::Level level)
        :_logger(logger)
        ,_file(file)
        ,_line(line)
        ,_level(level)
        ,_enable(logger && level >= logger->loggerLevel())
        {}

        LogStreamGuard(LogStreamGuard&& other) noexcept
        :_logger(std::move(other._logger))
        ,_file(std::move(other._file))
        ,_line(other._line)
        ,_level(other._level)
        ,_enable(other._enable)
        ,_os(std::move(other._os))
        {
            other._enable = false;
        }

        LogStreamGuard(const LogStreamGuard&) = delete;
        LogStreamGuard& operator=(const LogStreamGuard&) = delete;

        template<typename T>
        LogStreamGuard& operator<<(const T& res)
        {
            if(_enable)
                _os << res;
            return *this;
        }

        LogStreamGuard& operator<<(std::ostream& (*pf)(std::ostream&))
        {
            if(_enable)
                _os << pf;
            return *this;
        }

        ~LogStreamGuard()
        {
            if(_enable && _logger)
                _logger->Serialize(_file,_line,_level,_os.str());
        }

    private:
        Logger::ptr _logger;
        std::string _file;
        size_t _line;
        LogLevel::Level _level;
        bool _enable;
        std::ostringstream _os;
    };

    class SyncLogger : public Logger
    {
    public:
        SyncLogger(const std::string& loggername,
            std::shared_ptr<Formatter>& formatter, 
            LogLevel::Level limitlevel, 
            std::vector<std::shared_ptr<LogSink>>& sinks)
        :Logger(loggername,formatter,limitlevel,sinks)
        {}

    protected:
        // 同步日志器，是将日志直接通过落地模块句柄进行日志落地
        virtual void log(const char* data, size_t len) override
        {
            assert(data);
            std::unique_lock<std::mutex> lock(_lock);
            if(_sinks.empty()) return;
            for(auto& sink:_sinks)
            {
                sink->Log(data,len);
            }
        }
    };

    class AsyncLogger : public Logger
    {
    public:
        AsyncLogger(const std::string& loggername,
            std::shared_ptr<Formatter>& formatter, 
            LogLevel::Level limitlevel, 
            std::vector<std::shared_ptr<LogSink>>& sinks,
            AsyncStatus status = AsyncStatus::ASYNC_SAFE)
        :Logger(loggername,formatter,limitlevel,sinks)
        ,_looper(std::make_shared<AsyncLooper>(std::bind(&AsyncLogger::asynclog,this,std::placeholders::_1),status))
        {}

    protected:
        // 向缓冲区写入数据
        virtual void log(const char* data, size_t len) override
        {
            assert(data);
            _looper->Push(data,len);
        }

        // 将缓冲区内的数据实际落地
        void asynclog(AsyncBuffer& buffer)
        {
            if(_sinks.empty()) return;
            for(auto& sink:_sinks)
            {
                sink->Log(buffer.Begin(),buffer.ReadableSize());
            }
        }

    private:
        std::shared_ptr<AsyncLooper> _looper;
    };

    // 使用建造者模式来建造日志器，不用用户直接构建建日志器
    // 建造者基类
    // 1、设置日志器类型
    // 2、将不同类型日志器构建，放在一个建造者里
    enum class LoggerType
    {
        LOGGER_SYNC,
        LOGGER_ASYNC
    };

    class LoggerBuilder
    {
    public:
        LoggerBuilder()
        :_looper_status(AsyncStatus::ASYNC_SAFE)
        ,_logger_type(LoggerType::LOGGER_SYNC)
        ,_limit_level(LogLevel::Level::DEBUG)
        {}

        void BuildLoggerType(LoggerType type) { _logger_type = type; }
        void BuildLoggerName(const std::string& name) { _logger_name = name; }
        void BUildLoggerLevel(LogLevel::Level level) { _limit_level = level; }\
        void BuildLoggerSetUnSafeAsync() { _looper_status = AsyncStatus::ASYNC_UNSAFE; }

        void BuildLoggerFormatter(const std::string& pattern)
        {
            _formatter = std::make_shared<Formatter>(pattern);
        }

        template<typename SinkType,typename ...Args>
        void BUildLoggerSink(Args&& ...args)
        {
            std::shared_ptr<SinkType> sp = std::make_shared<SinkType>(std::forward<Args>(args)...);
            _sinks.emplace_back(sp);
        }

        virtual Logger::ptr Build() = 0;

    protected:
        AsyncStatus _looper_status;
        LoggerType _logger_type;
        std::string _logger_name;
        std::shared_ptr<Formatter> _formatter;
        LogLevel::Level _limit_level;
        std::vector<std::shared_ptr<LogSink>> _sinks;
    };

    // 派生出局部建造者类与全局建造者类
    class LocalLoggerBuilder : public LoggerBuilder
    {
    public:
        Logger::ptr Build() override
        {
            assert(!_logger_name.empty());
            if(_formatter.get() == nullptr)
            {
                _formatter = std::make_shared<Formatter>();
            }
            if(_sinks.empty())
            {
                _sinks.emplace_back(SinkFactory::Create<StdOutSink>());
            }
            if(_logger_type == LoggerType::LOGGER_ASYNC)
            {
                return std::make_shared<AsyncLogger>(_logger_name,_formatter,_limit_level,_sinks,_looper_status);
            }

            return std::make_shared<SyncLogger>(_logger_name,_formatter,_limit_level,_sinks);
        }
    };

    /*
    日志管理器
    1、对创建的所有日志器进行管理，并将管理器设置为单例
    2、可在程序任意位置获得相同单例对象，获取其中的日志器进行日志输出
*/
    class LoggerManager
    {
    public:
        // 懒汉模式，获取单例
        static LoggerManager& getInstance() 
        {
            static LoggerManager lm;
            return lm;
        }

        bool IsExistsLogger(const std::string& loggername)
        {
            std::unique_lock<std::mutex> lock(_lock);
            auto it = _loggers.find(loggername);
            if(it == _loggers.end())
            {
                return false;
            }
            return true;
        }

        void AddLogger(Logger::ptr logger)
        {
            if(IsExistsLogger(logger->loggerName())) return;
            std::unique_lock<std::mutex> lock(_lock);
            _loggers[logger->loggerName()] = logger;
        }

        Logger::ptr GetLogger(const std::string& loggername)
        {
            std::unique_lock<std::mutex> lock(_lock);
            auto it = _loggers.find(loggername);
            if(it == _loggers.end())
            {
                return Logger::ptr();
            }
            return it->second;
        }

        Logger::ptr RootLogger()
        {
            std::unique_lock<std::mutex> lock(_lock);
            return _root_logger;
        }

    private:
        LoggerManager()
        {
            std::unique_ptr<LoggerBuilder> slb = std::make_unique<LocalLoggerBuilder>();
            slb->BuildLoggerName("root");
            slb->BuildLoggerType(LoggerType::LOGGER_SYNC);
            _root_logger = slb->Build();
            _loggers["root"] = _root_logger;
        }

        LoggerManager(const LoggerManager&) = delete;
        LoggerManager &operator=(const LoggerManager&) = delete;
    private:
        std::mutex _lock;
        Logger::ptr _root_logger;
        std::unordered_map<std::string,Logger::ptr> _loggers;
    };

    class GobalLoggerBuilder : public LoggerBuilder
    {
        Logger::ptr Build() override
        {
            assert(!_logger_name.empty());
            assert(!LoggerManager::getInstance().IsExistsLogger(_logger_name));
            if(_formatter.get() == nullptr)
            {
                _formatter = std::make_shared<Formatter>();
            }
            if(_sinks.empty())
            {
                _sinks.emplace_back(SinkFactory::Create<StdOutSink>());
            }

            Logger::ptr logger;
            if(_logger_type == LoggerType::LOGGER_ASYNC)
            {
                logger = std::make_shared<AsyncLogger>(_logger_name,_formatter,_limit_level,_sinks,_looper_status);
            }
            else
            {
                logger = std::make_shared<SyncLogger>(_logger_name,_formatter,_limit_level,_sinks);
            }
            LoggerManager::getInstance().AddLogger(logger);

            return logger;
        }
    };
}