#include "HttpServer.hpp"

using namespace http_server;

void Usage(std::string procname)
{
    std::cout << "Usage: " << procname << " ServerPort" << std::endl;
}

void LoggerInit()
{
    {
        std::unique_ptr<LoggerBuilder> lbd = std::make_unique<GobalLoggerBuilder>();
        lbd->BuildLoggerName("HttpLogger");
        lbd->BUildLoggerSink<RollByTimeSink>("./logs/",TimeGap::GAP_DAY);
        lbd->Build();
    }

    {
        std::unique_ptr<LoggerBuilder> lbd = std::make_unique<GobalLoggerBuilder>();
        lbd->BuildLoggerName("ServerLogger");
        lbd->BUildLoggerSink<RollByTimeSink>("./logs/",TimeGap::GAP_DAY);
        lbd->Build();
    }
}

std::string RequestStr(const HttpRequest& req)
{
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._http_version << linesep;
    for(auto& it : req._paramkv)
    {
        ss << it.first << headersep << it.second << linesep;
    }
    for(auto& it : req._headerkv)
    {
        ss << it.first << headersep << it.second << linesep;
    }
    ss << linesep;
    ss << req._body;
    return ss.str();
}

void Hello(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req),"text/plain");
}

void Login(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req),"text/plain");
}

void PutFile(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req),"text/plain");
}

void DelFile(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req),"text/plain");
}


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        Usage(argv[0]);
        exit(1);
    }

    uint16_t port = std::stoi(argv[1]); 

    LoggerInit();

    HttpServer server(port,10);
    server.SetThreadCount(3);
    server.Get("/hello",Hello);
    server.Post("/login",Login);
    server.Put("/123.txt",PutFile);
    server.Delete("/123.txt",DelFile);

    server.ListenAndStart();

    return 0;
}