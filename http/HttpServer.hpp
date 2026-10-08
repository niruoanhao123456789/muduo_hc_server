#pragma once
#include "HttpProtocol.hpp"

namespace http_server
{
    using namespace http_prtocol;

    #define DEFALT_TIMEOUT 10

    const std::string webroot = "wwwroot";
    const std::string homepage = "index.html";

    class HttpServer
    {
        using Handler = std::function<void(const HttpRequest&, HttpResponse*)>;
        using Handlers = std::vector<std::pair<std::regex,Handler>>;
    public:
        HttpServer(int port = 8080,int timeout = DEFALT_TIMEOUT)
        :_server(port)
        {
            _server.EnableInactiveRelease(timeout);
            _server.SetConnectedCallBack(std::bind(&HttpServer::OnConnected,this,std::placeholders::_1));
            _server.SetMessageCallBack(std::bind(&HttpServer::OnMessage,this,std::placeholders::_1,std::placeholders::_2));
            SetBaseDir("./" + webroot + "/");
        }

        void SetBaseDir(const std::string& path)
        {
            assert(Util::IsDirectoryExists(path));
            _basedir = path;
        }

        // 设置/添加，请求（请求的正则表达）与处理函数的映射关系
        void Get(const std::string& pattern, const Handler& handler)
        {
            _get_route.emplace_back(std::make_pair(std::regex(pattern), handler));
        }

        void Post(const std::string& pattern, const Handler& handler) 
        {
            _post_route.push_back(std::make_pair(std::regex(pattern), handler));
        }

        void Put(const std::string& pattern, const Handler& handler) 
        {
            _put_route.push_back(std::make_pair(std::regex(pattern), handler));
        }

        void Delete(const std::string& pattern, const Handler& handler) 
        {
            _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
        }

        void SetThreadCount(int count) 
        {
            _server.SetThreadCount(count);
        }

        void ListenAndStart()
        {
            _server.Start();
        }

    private:
        void ErrorHandler(const HttpRequest& req, HttpResponse* resp)
        {
            assert(resp);
            // 组织一个错误展示页面
            std::string body;
            body += "<html>";
            body += "<head>";
            body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
            body += "</head>";
            body += "<h1>";
            body += std::to_string(resp->_statu);
            body += " ";
            body += Util::StatuDesc(resp->_statu);
            body += "</h1>";
            body += "</body>";
            body += "</html>";

            // 将页面数据，当作响应正文，放入resp中
            resp->SetContent(body, "text/html");
        }

        // 将HttpResponse中的要素按照http协议格式进行组织，发送
        void WriteResponse(const PtrConnection& conn, const HttpRequest& req, HttpResponse& resp)
        {
            // 先完善头部字段
            if(req.Close())
                resp.HeaderInsert("Connection","close");
            else
                resp.HeaderInsert("Connection","keep-alive");

            if(!resp._body.empty() && !resp.IsHeaderExist("Content-Length"))
                resp.HeaderInsert("Content-Length",std::to_string(resp._body.size()));
            
            if(!resp._body.empty() && !resp.IsHeaderExist("Content-Type"))
                resp.HeaderInsert("Content-Type", "application/octet-stream");
            
            if(resp._redirect_flag)
                resp.HeaderInsert("Location",resp._redirect_url);
            
            // 将resp中的要素，按照http协议的格式进行组织
            std::stringstream resp_str;
            resp_str << req._http_version << " " << std::to_string(resp._statu) << " " << Util::StatuDesc(resp._statu) << linesep;
            for(auto& head : resp._headerkv)
            {
                resp_str << head.first << ": " << head.second << linesep;
            }      
            resp_str << linesep;
            resp_str << resp._body;
            // 发送数据
            conn->Send(resp_str.str().c_str(),resp_str.str().size());
        }

        bool IsFileHandler(const HttpRequest& req)
        {
            // 需先设置了静态资源根目录
            if(_basedir.empty())
                return false;
            
            // 请求方法需是GET或HEAD请求方法
            if(req._method != "GET" && req._method != "HEAD")
                return false;

            // 请求的资源路径必须是一个合法路径
            if(!Util::ValidPath(req._path))
                return false;

            // 请求的资源必须存在,且是一个普通文件
            // 有一种请求比较特殊 -- 目录：/, /image/， 这种情况给后边默认追加一个 index.html
            // index.html    /image/a.png
            // 不要忘了前缀的相对根目录,也就是将请求路径转换为实际存在的路径  /image/a.png  ->   ./wwwroot/image/a.png
            std::string req_path = _basedir + req._path;
            if(req._path.back() == '/')
                req_path += "index.html";

            if(!Util::IsFileExists(req_path))
                return false;
            
            return true;
        }

        // 静态资源的请求处理 --- 将静态资源文件的数据读取出来，放到rsp的_body中, 并设置mime
        void FileHandler(const HttpRequest& req, HttpResponse* resp)
        {
            assert(resp);
            std::string req_path = _basedir + req._path;
            if(req._path.back() == '/')
                req_path += "index.html";
            
            Util::ReadFile(req_path,&resp->_body);
            std::string mime = Util::ExtMime(req_path);
            resp->HeaderInsert("Content-Type",mime);
        }

        // 功能性请求的分类处理
        void Dispatcher(HttpRequest& req,HttpResponse* resp,Handlers& handlers)
        {
            assert(resp);
            // 在对应请求方法的路由表中，查找是否含有对应资源请求的处理函数，有则调用，没有则发挥404
            // 思想：路由表存储的时键值对 -- 正则表达式 & 处理函数
            // 使用正则表达式，对请求的资源路径进行正则匹配，匹配成功就使用对应函数进行处理
            //  /numbers/(\d+)       /numbers/12345
            for(auto& handler : handlers)
            {
                const std::regex& reg = handler.first;
                const Handler& func = handler.second;
                bool ret = std::regex_match(req._path,req._matches,reg);
                if(!ret)
                    continue;
                func(req,resp); // 传入请求信息，和空的rsp，执行处理函数
                return;
            }
            resp->_statu = 404;
        }

        /*
        对请求进行分辨，分清是静态请求还是功能性请求
        静态资源请求，则进行静态资源的处理
        功能性请求，则通过请求路由表来确定对应处理函数
        如果是其他，则返回405
        */
        void Route(HttpRequest& req, HttpResponse* resp)
        {
            assert(resp);
            if(IsFileHandler(req))
                FileHandler(req,resp);
            else if(req._method == "GET" || req._method == "HEAD")
                Dispatcher(req,resp,_get_route);
            else if(req._method == "POST")
                Dispatcher(req,resp,_post_route);
            else if(req._method == "PUT")
                Dispatcher(req,resp,_put_route);
            else if(req._method == "DELETE")
                Dispatcher(req,resp,_delete_route);
            else
                resp->_statu = 405; // Method Not Allowed

        }

        // 设置上下文
        void OnConnected(const PtrConnection& conn)
        {
            conn->SetContext(HttpContext());
            LOG_INFOR_STREAM(GetLogger("HttpLogger")) << "New Connection: " << conn.get();
        }

        // 缓冲区数据解析+处理
        void OnMessage(const PtrConnection& conn, ServerBuffer* buf)
        {
            assert(buf);
            while(buf->ReadableSize()>0)
            {
                // 获取上下文
                HttpContext* context = conn->GetContext()->get<HttpContext>();

                // 通过上下文对缓冲区数据进行解析，得到HttpRequest对象
                //  1. 如果缓冲区的数据解析出错，就直接回复出错响应
                //  2. 如果解析正常，且请求已经获取完毕，才开始去进行处理
                context->RecvHttpRequest(buf);
                HttpRequest& req = context->Request();
                HttpResponse resp(context->RespStatu());
                if(context->RespStatu() >= 400)
                {
                    // 进行错误响应，关闭链接

                    // 填充一个错误显示页面数据到rsp中
                    ErrorHandler(req,&resp);
                    // 组织响应发送给客户端
                    WriteResponse(conn,req,resp); 
                    context->Reset();
                    // 出错了就把缓冲区数据清空
                    buf->Reset();
                    conn->Shutdown();
                }

                // 当前请求还没有接收完整,则退出，等新数据到来再重新继续处理
                if(context->RecvStatu() != HttpRecvStatu::RECV_HTTP_OVER)
                    return;

                // 请求路由 + 业务处理
                Route(req,&resp);

                // 对HttpResponse进行组织发送
                WriteResponse(conn,req,resp);

                // 重置上下文
                context->Reset();
                
                // 根据长短链接判断是否关闭链接或者继续处理
                // 短链接则直接关闭
                if(resp.Close())
                    conn->Shutdown();
            }
        }

    private:
        Handlers _get_route;
        Handlers _post_route;
        Handlers _put_route;
        Handlers _delete_route;
        std::string _basedir;
        TcpServer _server;
    };
}
