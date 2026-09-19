#ifndef __UTIL_HPP
#define __UTIL_HPP
#include<iostream>
#include<ctime>
#include<string>
#include<sys/stat.h>

/*
    通用功能类，与业务无关的功能实现
        1. 获取系统时间
        2. 获取文件大小
        3. 创建目录
        4. 获取文件所在目录
*/

namespace LogModule
{
    namespace util
    {
        class Date
        {
        public:
            static time_t GetCurTime()
            {
                return time(nullptr);
            }
        };

        class File
        {
        public:
            static bool IsExists(const std::string& pathname)
            {
                struct stat st;
                return stat(pathname.c_str(),&st) == 0;
            }

            static std::string GetPath(const std::string& pathname)
            {
                if(pathname.empty())
                    return ".";
                
                size_t pos = pathname.find_last_of("/\\");
                if(pos == std::string::npos)
                    return ".";

                return pathname.substr(0,pos+1);
            }

            static void CreateDirectory(const std::string& pathname)
            {
                if(pathname.empty() || IsExists(pathname))
                    return;
                
                size_t pos = 0, index = 0;
                while(index<pathname.size())
                {
                    pos = pathname.find_first_of("/\\",index);
                    if(pos == std::string::npos)
                    {
                        mkdir(pathname.c_str(),0755);
                        return;
                    }
                    if(pos==index)
                    {
                        index = pos+1;
                        continue;
                    }
                    std::string subdir = pathname.substr(0,pos);
                    if(subdir=="."||subdir=="..")
                    {
                        index = pos + 1;
                        continue;
                    }
                    if(IsExists(subdir))
                    {
                        index = pos + 1;
                        continue;
                    }
                    mkdir(subdir.c_str(),0755);
                    index = pos + 1;
                }
            }
        };
    }
}

#endif