#pragma once
#include <regex>
#include "Util.hpp"
#include "../source/Server.hpp"

namespace http_prtocol
{
    using namespace server;
    using namespace http_util;

    const std::string linesep = "\r\n";
    const std::string headersep = ": ";
    const std::string gspace = " ";
    const std::string suffixsep = ".";
    const std::string argsep = "?";
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
            _path.clear();
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

        void HeaderInsert(const std::pair<const std::string&,const std::string&>& kv)
        {
            _headerkv.insert(kv);
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

        void ParamInsert(const std::pair<const std::string&,const std::string&>& kv)
        {
            _paramkv.insert(kv);
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

    public:
        std::string _method;             // 请求方法
        std::string _path;               // 资源路径
        std::string _http_version;       // 协议版本
        std::string _body;               // 请求正文
        std::smatch _matches;            // 资源路径正则匹配的数据
        std::unordered_map<std::string, std::string> _headerkv;     // 头部字段
        std::unordered_map<std::string, std::string> _paramkv;       // 查询字符串
    };

    class HttpResponse
    {
    public:
        HttpResponse()
        :_statu(200)
        ,_redirect_flag(false)
        {}

        HttpResponse(int statu)
        :_statu(statu)
        ,_redirect_flag(false)
        {}

        void Reset()
        {
            _statu = 200;
            _redirect_flag = false;
            _body.clear();
            _redirect_url.clear();
            _headerkv.clear();
        }

        // 插入头部字段
        void HeaderInsert(const std::string& key, const std::string& val)
        {
            _headerkv.insert({key,val});
        }

        void HeaderInsert(const std::pair<const std::string&,const std::string&>& kv)
        {
            _headerkv.insert(kv);
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

        void SetContent(const std::string& body, const std::string& type = "text/html")
        {
            _body = body;
            HeaderInsert("Content-Type",type);
        }

        void SetRedirect(const std::string& url, int statu = 302)
        {
            _statu = statu;
            _redirect_flag = true;
            _redirect_url = url;
        }

        // 判断是否是短链接
        bool Close() const
        {
            if(IsHeaderExist("Connection") && (GetHeader("Connection") == "keep-alive"))
                return false;
            else
                return true;
        }

    public:
        int _statu;  
        bool _redirect_flag;
        std::string _body;
        std::string _redirect_url;
        std::unordered_map<std::string,std::string> _headerkv;
    };

    enum class HttpRecvStatu
    {
        RECV_HTTP_ERROR,
        RECV_HTTP_LINE,
        RECV_HTTP_HEAD,
        RECV_HTTP_BODY,
        RECV_HTTP_OVER
    };

    #define MAX_LINE 8192
    class HttpContext
    {
    public:
        HttpContext()
        :_resp_statu(200)
        ,_recv_statu(HttpRecvStatu::RECV_HTTP_LINE)
        {}    

        void Reset()
        {
            _resp_statu = 200;
            _recv_statu = HttpRecvStatu::RECV_HTTP_LINE;
            _req.Reset();
        }

        int RespStatu() const
        {
            return _resp_statu;
        }

        HttpRecvStatu RecvStatu() const
        {
            return _recv_statu;
        }

        HttpRequest& Request()
        {
            return _req;
        }

        // 接收并解析HTTP请求
        void RecvHttpRequest(ServerBuffer* buf)
        {
            assert(buf);
            // 不同的状态，做不同的事情，但是这里不要break， 因为处理完请求行后，应该立即处理头部，而不是退出等新数据
            switch(_recv_statu) 
            {
                case HttpRecvStatu::RECV_HTTP_LINE: RecvHttpLine(buf);
                case HttpRecvStatu::RECV_HTTP_HEAD: RecvHttpHead(buf);
                case HttpRecvStatu::RECV_HTTP_BODY: RecvHttpBody(buf);
            }
        }

    private:
        void RecvHttpLine(ServerBuffer* buf)
        {
            assert(buf);
            if(_recv_statu != HttpRecvStatu::RECV_HTTP_LINE)
            {
                LOG_WARNNING_STREAM(GetLogger("HttpLogger")) << "Cannot recv httpline now!";
                return;
            }

            // 获取一行数据，带有末尾的换行
            std::string line = buf->GetLine();
            // 需要考虑的一些要素：缓冲区中的数据不足一行， 获取的一行数据超大
            if(!line.size())
            {
                // 缓冲区中的数据不足一行，则需要判断缓冲区的可读数据长度，如果很长了都不足一行，这是有问题的
                if(buf->ReadableSize() > MAX_LINE)
                {
                    _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                    _resp_statu = 414; // URI TOO LONG
                }
                return;
            }
            else if(line.size() > MAX_LINE)
            {
                _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return;
            }

            bool ret = ParseHttpLine(line);
            if(!ret)
                return;
            
            // 首行处理完毕，进入头部获取阶段
            _recv_statu = HttpRecvStatu::RECV_HTTP_HEAD;
        }

        bool ParseHttpLine(const std::string& line)
        {
            std::smatch matches;
            std::regex reg("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?",std::regex::icase);
            bool ret = std::regex_match(line,matches,reg);
            if(!ret)
            {
                _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                _resp_statu = 400; // BAD REQUEST
                return false;
            }
            // 0 : GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1
            // 1 : GET
            // 2 : /bitejiuyeke/login
            // 3 : user=xiaoming&pass=123123
            // 4 : HTTP/1.1
            // 请求方法的获取
            _req._method = matches[1];
            std::transform(_req._method.begin(),_req._method.end(),_req._method.begin(),::toupper);
            // 资源路径的获取，需要进行URL解码操作，但是不需要+转空格
            _req._path = http_util::Util::UrlDecode(matches[2], false);

            _req._http_version = matches[4];

            
            std::string query_string = matches[3];
            if(matches[3].matched && !query_string.empty())
            {
                std::vector<std::string> query_strings;
                // 先以 & 符号进行分割，得到各个字串
                http_util::Util::SplitString(query_string,&query_strings,"&");
                // 针对各个字串，以 = 符号进行分割，得到key 和val， 得到之后也需要进行URL解码
                for(auto& str : query_strings)
                {
                    size_t pos = str.find("=");
                    if(pos==std::string::npos)
                    {
                        _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                        _resp_statu = 400;
                        return false;
                    }
                    std::string key = Util::UrlDecode(str.substr(0,pos));
                    std::string val = Util::UrlDecode(str.substr(pos+1));
                    _req.ParamInsert(key,val);
                }
            }
            
            return true;
        }

        void RecvHttpHead(ServerBuffer* buf)
        {
            assert(buf);
            if(_recv_statu != HttpRecvStatu::RECV_HTTP_HEAD)
                return;
            
            // 一行一行取出数据，直到遇到空行为止
            // 头部的格式 key: val\r\nkey: val\r\n...

            while(1)
            {
                std::string line = buf->GetLine();
                // 考虑缓冲区中的数据不足一行， 获取的一行数据超大
                if(!line.size())
                {
                    // 缓冲区中的数据不足一行，则需要判断缓冲区的可读数据长度，如果很长了都不足一行，这是有问题的
                    if(buf->ReadableSize() > MAX_LINE)
                    {
                        _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                        _resp_statu = 414; // URI TOO LONG
                    }
                    return;
                }
                else if(line.size() > MAX_LINE)
                {
                    _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                    _resp_statu = 414; // URI TOO LONG
                    return;
                }

                if(line == "\n" || line == linesep)
                    break;
                
                bool ret = ParseHttpHead(line);
                if(!ret)
                    return;
            }

            // 头部处理完毕，进入正文获取阶段
            _recv_statu = HttpRecvStatu::RECV_HTTP_BODY;
        }

        bool ParseHttpHead(std::string& head)
        {
            // key: val\r\nkey: val\r\n....
            if(head.back() == '\n')
                head.pop_back();
            if(head.back() == '\r')
                head.pop_back();
            
            size_t pos = head.find(headersep);
            if(pos==std::string::npos)
            {
                _recv_statu = HttpRecvStatu::RECV_HTTP_ERROR;
                _resp_statu = 400;
                return false;
            }

            std::string key = head.substr(0, pos);  
            std::string val = head.substr(pos + 2);
            _req.HeaderInsert(key, val);
            return true;
        }

        void RecvHttpBody(ServerBuffer* buf)
        {
            assert(buf);
            if (_recv_statu != HttpRecvStatu::RECV_HTTP_BODY)
                return;
            
            // 获取正文长度
            size_t content_len = _req.ContentLength();
            if(!content_len)
            {
                _recv_statu = HttpRecvStatu::RECV_HTTP_OVER;
                return;
            }

            // 当前已经接收了多少正文,其实就是往  _request._body 中放了多少数据了
            // 实际还需要接收的正文长度
            size_t left_len = content_len - _req._body.size();
            
            // 接收正文放到body中，但是也要考虑当前缓冲区中的数据，是否是全部的正文
            // 缓冲区中数据，包含了当前请求的所有正文，则取出所需的数据
            if(buf->ReadableSize() >= left_len)
            {
                // 通过ReadAsString读取，内部已处理缓冲区回绕，避免越过物理末尾读越界
                _req._body += buf->ReadAsString(left_len);
                _recv_statu = HttpRecvStatu::RECV_HTTP_OVER;
            }
            else
            {
                // 缓冲区中数据，无法满足当前正文的需要，数据不足，取出数据，然后等待新数据到来
                _req._body += buf->ReadAsString(buf->ReadableSize());
            }

        }

    private:
        int _resp_statu;                // 响应状态码
        HttpRecvStatu _recv_statu;      // 当前接收及解析的阶段状态
        HttpRequest _req;               // 已经解析得到的请求信息
    };
}