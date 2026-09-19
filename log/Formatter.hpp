#pragma once
#include<ctime>
#include<vector>
#include<cassert>
#include<sstream>
#include<string>
#include<memory>
#include<iostream>
#include<cstdlib>
#include"Level.hpp"
#include"LogMessage.hpp"

namespace LogModule
{
    class FormatItem
    {
    public:
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os,const LogMessage& msg) = 0;
    };

    class MessageFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            os << msg._payload;
        }
    };

    class LevelFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            os << LogLevel::LevelToString(msg._level);
        }
    };

    class NameFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            os << msg._name;
        }
    };

    class ThreadFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            os << msg._tid;
        }
    };

    class TimeFormatItem : public FormatItem
    {
    public:
        TimeFormatItem(const std::string& format = "%Y%m%d%H%M%S") :_format(format) 
        {
            if(format.empty())
                _format = "%Y%m%d%H%M%S";
        }

        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            time_t timestamp = msg._ctime;
            struct tm date_time;
            localtime_r(&timestamp,&date_time);
            char date_time_str[128]={0};
            strftime(date_time_str,127,_format.c_str(),&date_time);
            os << date_time_str;
        }
    
    private:
        std::string _format;
    };

    class CFileFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage& msg) override
        {
            os << msg._file;
        }
    };

    class CLineFormatItem : public FormatItem 
    {
    public:
        virtual void format(std::ostream &os, const LogMessage &msg) override
        {
            os << msg._line;
        }
    };

    class TabFormatItem : public FormatItem
    {
    public:
        TabFormatItem() {}
        virtual void format(std::ostream& os,const LogMessage&) override
        {
            os << "\t";
        }
    };

    class NLineFormatItem : public FormatItem
    {
    public:
        virtual void format(std::ostream& os,const LogMessage&) override
        {
            os << "\n";
        }
    };

    class OtherFormatItem : public FormatItem
    {
    public:
        OtherFormatItem(const std::string& str = "") :_str(str) {}
        virtual void format(std::ostream& os,const LogMessage&) override
        {
            os << _str;
        }
    
    private:
        std::string _str;
    };


    class Formatter
    {
    public:
        /*
            %d 日期
            %T 缩进
            %t 线程id
            %p 日志级别
            %c 日志器名称
            %f 文件名
            %l 行号
            %m 日志消息
            %n 换行
        */
        //时间[年-月-日 时:分:秒] 线程ID [日志名称] [文件名:行号] [日志级别]- 消息换行
        Formatter(const std::string& pattern = "[%d{%Y-%m-%d %H:%M:%S}][%t][%c][%f:%l][%p]- %m%n")
        :_pattern(pattern)
        {
            assert(ParsePattern());
        }

        const std::string pattern() { return _pattern; }

        // 对消息进行所要求的格式化
        std::string format(const LogMessage& msg)
        {
            std::stringstream ss;
            for(auto& it:_items)
            {
                it->format(ss,msg);
            }
            return ss.str();
        }

        std::ostream& format(std::ostream &os, const LogMessage &msg)
        {
            for(auto& it:_items)
            {
                it->format(os,msg);
            }
            return os;
        }

        private:
        bool ParsePattern()
        {
            // 1、对格式化字符串进行解析
            // ab%%de[%d{%Y-%m-%d %H:%M:%S}][%p]%T%m%n
            std::vector<std::pair<std::string,std::string>> fmt_order;
            size_t pos = 0;
            std::string key,value;
            while(pos<_pattern.size())
            {
                // 处理第一个%前的原始字符串
                if(_pattern[pos]!='%')
                {
                    value += _pattern[pos++];
                    continue;
                }
                // 处理 双%
                if(pos+1<_pattern.size()&&_pattern[pos+1] == '%')
                {
                    value += "%";
                    pos += 2;
                    continue;
                }
                // 原始字符串处理完毕
                if(!value.empty())
                {
                    fmt_order.push_back({"",value});
                    value.clear();
                }
                // 处理%d这样的格式化字符
                pos++;
                if(pos<_pattern.size()&&isalpha(static_cast<unsigned char>(_pattern[pos])))
                    key = _pattern[pos++];
                else
                {
                    std::cout<<"位置 "<< pos <<" 处 %%d 格式字符错误或无格式化字符"<< std::endl;
                    return false;
                }
                // 此时为%d后面的位置，判断是否有{}子规则并进行处理
                if(pos<_pattern.size()&&_pattern[pos]=='{')
                {
                    pos++;
                    while(pos<_pattern.size()&&_pattern[pos]!='}')
                    {
                        value += _pattern[pos++];
                    }
                    // 没有遇到 } 情况下结束循环
                    if(pos>=_pattern.size())
                    {
                        std::cout<<"{}子规则错误"<<std::endl;
                        return false;
                    }
                    pos++;
                }
                // 处理value里 %d{%Y-%m-%d %H:%M:%S} 这样的格式化字符串整体
                fmt_order.push_back({key,value});
                key.clear();
                value.clear();
            }
            // 若格式化字符串以原始文本结尾，需补上最后的原始文本
            if(!value.empty())
            {
                std::cout<<"格式化字符串结尾格式错误"<<std::endl;
                return false;
            }

            // 2、根据解析到的数据初始化格式化子项数组
            for(auto& it:fmt_order)
            {
                _items.push_back(CreateItem(it.first,it.second));
            }
    
            return true;
        }
    
        // 根据不同格式化字符串，创建不同的格式化子项对象
        std::shared_ptr<FormatItem> CreateItem(const std::string& key,const std::string& value)
        {
            if (key == "m") return std::make_shared<MessageFormatItem>();
            if (key == "p") return std::make_shared<LevelFormatItem>();
            if (key == "c") return std::make_shared<NameFormatItem>();
            if (key == "t") return std::make_shared<ThreadFormatItem>();
            if (key == "n") return std::make_shared<NLineFormatItem>();
            if (key == "d") return std::make_shared<TimeFormatItem>(value);
            if (key == "f") return std::make_shared<CFileFormatItem>();
            if (key == "l") return std::make_shared<CLineFormatItem>();
            if (key == "T") return std::make_shared<TabFormatItem>();
            if (key == "")  return std::make_shared<OtherFormatItem>(value);
            std::cout<<"未定义格式化字符: %"<<key<<std::endl;
            abort();
        }

    private:
        std::string _pattern;
        std::vector<std::shared_ptr<FormatItem>> _items;
    };
}