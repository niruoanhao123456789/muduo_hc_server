#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <sys/stat.h>
#include <sys/types.h>

namespace http_util
{
    class Util
    {
    public:
        static void SplitString(const std::string& str,std::vector<std::string>* target, const std::string& sep = " ")
        {
            assert(target);
            boost::split(*target,str,boost::is_any_of(sep),boost::algorithm::token_compress_on);
        }

        // 读取文件内容放置到一个缓冲区中
        static void ReadFile(const std::string& filename, std::string* buf)
        {
            std::ifstream in(filename,std::ios::binary);
            assert(in.is_open());

            
            // 跳转读写位置到末尾
            in.seekg(0,in.end);
            // 获取当前读写位置相对于起始位置的偏移量，从末尾偏移刚好就是文件大小
            size_t fsize = in.tellg();
            // 跳转到起始位置
            in.seekg(0,in.beg);

            buf->resize(fsize);
            in.read(&(*buf)[0],fsize);
            assert(in.good());

            in.close();
        }

        // 向文件写入数据
        static bool WriteFile()
        {

        }

        // URL 编码
        static bool UrlEncode()
        {

        }

        // Url 解码
        static bool UrlDecode()
        {

        }

        // 响应状态的描述信息获取
        static std::string StatuDesc()
        {

        }

        // 根据文件后缀名获取文件mime
        static std::string ExtMime()
        {

        }

        // 判断是否为目录
        static bool IsDirectoryExists()
        {

        }

        // 判断是否为普通文件
        static bool IsFileExists(const std::string& filename)
        {
            struct stat st;
            if(!stat(filename.c_str(),&st))
                return true;
            else
                return false;
        }

        // htpp 请求的资源路径有效判断
        static bool ValidPath()
        {

        }
    };
}