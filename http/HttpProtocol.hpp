#pragma once
#include <regex>
#include "Util.hpp"

namespace http_prtocol
{
    using namespace http_util;

    const std::string gdefault_version = "HTTP/1.1";

    class HttpRequest
    {
    public:
        HttpRequest()
        :_http_version(gdefault_version)
        {
        }

        void Reset()
        {
            _method.clear();
            _url.clear();
            _http_version = gdefault_version;
            _body.clear();
            std::smatch match;
            _matches.swap(match);
            _headerkv.clear();
            _paramkv.clear();
        }

        // 插入头部字段
        void HeaderInsert(const std::string& key, const std::string& val)
        {
            _headerkv.insert({key,val});
        }

        // 判断头部字段是否存在
        bool IsHeaderExist(const std::string& key)  const
        {
            auto it = _headerkv.find(key);
            if(it == _headerkv.end())
                return false;
            else
                return true;
        }

        // 获取指定头部字段的值
        std::string GetHeader(const std::string& key) const
        {
            auto it = _headerkv.find(key);
            if(it == _headerkv.end())
                return "";
            else
                return it->second; 
        }

        // 插入查询字符串
        void ParamInsert(const std::string& key,const std::string& val)
        {
            _paramkv.insert({key,val});
        }

        // 判断指定查询字符串是否存在
        bool IsParamExist(const std::string& key) const
        {
            auto it = _paramkv.find(key);
            if(it == _paramkv.end())
                return false;
            else
                return true;
        }

        // 获取指定的查询字符串
        std::string GetParam(const std::string& key) const
        {
            auto it = _paramkv.find(key);
            if(it == _paramkv.end())
                return "";
            else
                return it->second;
        }

        // 获取正文长度
        size_t ContentLength() const
        {
            if(!IsHeaderExist("Content-Length"))
                return 0;
            
            std::string lenstr = GetHeader("Content-Length");
            return std::stoul(lenstr);
        }

        // 判断是否是短链接
        bool Close() const
        {
            if(IsHeaderExist("Connection") && (GetHeader("Connection") == "keep-alive"))
                return false;
            else
                return true;
        }

    private:
        std::string _method;             // 请求方法
        std::string _url;                // 资源路径
        std::string _http_version;       // 协议版本
        std::string _body;               // 请求正文
        std::smatch _matches;            // 资源路径正则匹配的数据
        std::unordered_map<std::string, std::string> _headerkv;     // 头部字段
        std::unordered_map<std::string, std::string> _paramkv;       // 查询字符串
    };

    class HttpResponse
    {

    };

}