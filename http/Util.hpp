#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <fstream>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <cctype>


namespace http_util
{
    std::unordered_map<int, std::string> statu_msg = {
        {100,  "Continue"},
        {101,  "Switching Protocol"},
        {102,  "Processing"},
        {103,  "Early Hints"},
        {200,  "OK"},
        {201,  "Created"},
        {202,  "Accepted"},
        {203,  "Non-Authoritative Information"},
        {204,  "No Content"},
        {205,  "Reset Content"},
        {206,  "Partial Content"},
        {207,  "Multi-Status"},
        {208,  "Already Reported"},
        {226,  "IM Used"},
        {300,  "Multiple Choice"},
        {301,  "Moved Permanently"},
        {302,  "Found"},
        {303,  "See Other"},
        {304,  "Not Modified"},
        {305,  "Use Proxy"},
        {306,  "unused"},
        {307,  "Temporary Redirect"},
        {308,  "Permanent Redirect"},
        {400,  "Bad Request"},
        {401,  "Unauthorized"},
        {402,  "Payment Required"},
        {403,  "Forbidden"},
        {404,  "Not Found"},
        {405,  "Method Not Allowed"},
        {406,  "Not Acceptable"},
        {407,  "Proxy Authentication Required"},
        {408,  "Request Timeout"},
        {409,  "Conflict"},
        {410,  "Gone"},
        {411,  "Length Required"},
        {412,  "Precondition Failed"},
        {413,  "Payload Too Large"},
        {414,  "URI Too Long"},
        {415,  "Unsupported Media Type"},
        {416,  "Range Not Satisfiable"},
        {417,  "Expectation Failed"},
        {418,  "I'm a teapot"},
        {421,  "Misdirected Request"},
        {422,  "Unprocessable Entity"},
        {423,  "Locked"},
        {424,  "Failed Dependency"},
        {425,  "Too Early"},
        {426,  "Upgrade Required"},
        {428,  "Precondition Required"},
        {429,  "Too Many Requests"},
        {431,  "Request Header Fields Too Large"},
        {451,  "Unavailable For Legal Reasons"},
        {501,  "Not Implemented"},
        {502,  "Bad Gateway"},
        {503,  "Service Unavailable"},
        {504,  "Gateway Timeout"},
        {505,  "HTTP Version Not Supported"},
        {506,  "Variant Also Negotiates"},
        {507,  "Insufficient Storage"},
        {508,  "Loop Detected"},
        {510,  "Not Extended"},
        {511,  "Network Authentication Required"}
    };

    std::unordered_map<std::string, std::string> mime_msg = {
        {".aac",        "audio/aac"},
        {".abw",        "application/x-abiword"},
        {".arc",        "application/x-freearc"},
        {".avi",        "video/x-msvideo"},
        {".azw",        "application/vnd.amazon.ebook"},
        {".bin",        "application/octet-stream"},
        {".bmp",        "image/bmp"},
        {".bz",         "application/x-bzip"},
        {".bz2",        "application/x-bzip2"},
        {".csh",        "application/x-csh"},
        {".css",        "text/css"},
        {".csv",        "text/csv"},
        {".doc",        "application/msword"},
        {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".eot",        "application/vnd.ms-fontobject"},
        {".epub",       "application/epub+zip"},
        {".gif",        "image/gif"},
        {".htm",        "text/html"},
        {".html",       "text/html"},
        {".ico",        "image/vnd.microsoft.icon"},
        {".ics",        "text/calendar"},
        {".jar",        "application/java-archive"},
        {".jpeg",       "image/jpeg"},
        {".jpg",        "image/jpeg"},
        {".js",         "text/javascript"},
        {".json",       "application/json"},
        {".jsonld",     "application/ld+json"},
        {".mid",        "audio/midi"},
        {".midi",       "audio/x-midi"},
        {".mjs",        "text/javascript"},
        {".mp3",        "audio/mpeg"},
        {".mpeg",       "video/mpeg"},
        {".mpkg",       "application/vnd.apple.installer+xml"},
        {".odp",        "application/vnd.oasis.opendocument.presentation"},
        {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
        {".odt",        "application/vnd.oasis.opendocument.text"},
        {".oga",        "audio/ogg"},
        {".ogv",        "video/ogg"},
        {".ogx",        "application/ogg"},
        {".otf",        "font/otf"},
        {".png",        "image/png"},
        {".pdf",        "application/pdf"},
        {".ppt",        "application/vnd.ms-powerpoint"},
        {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        {".rar",        "application/x-rar-compressed"},
        {".rtf",        "application/rtf"},
        {".sh",         "application/x-sh"},
        {".svg",        "image/svg+xml"},
        {".swf",        "application/x-shockwave-flash"},
        {".tar",        "application/x-tar"},
        {".tif",        "image/tiff"},
        {".tiff",       "image/tiff"},
        {".ttf",        "font/ttf"},
        {".txt",        "text/plain"},
        {".vsd",        "application/vnd.visio"},
        {".wav",        "audio/wav"},
        {".weba",       "audio/webm"},
        {".webm",       "video/webm"},
        {".webp",       "image/webp"},
        {".woff",       "font/woff"},
        {".woff2",      "font/woff2"},
        {".xhtml",      "application/xhtml+xml"},
        {".xls",        "application/vnd.ms-excel"},
        {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {".xml",        "application/xml"},
        {".xul",        "application/vnd.mozilla.xul+xml"},
        {".zip",        "application/zip"},
        {".3gp",        "video/3gpp"},
        {".3g2",        "video/3gpp2"},
        {".7z",         "application/x-7z-compressed"}
    };

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
            assert(buf);
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
        static void WriteFile(const std::string& filename, std::string& buf)
        {
            std::ofstream out(filename, std::ios::binary | std::ios::trunc);
            assert(out.is_open());

            out.write(buf.c_str(),buf.size());
            assert(out.good());

            out.close();
        }

        // URL 编码
        // URL编码，避免URL中资源路径与查询字符串中的特殊字符与HTTP请求中特殊字符产生歧义
        // 编码格式：将特殊字符的ascii值，转换为两个16进制字符，前缀%   C++ -> C%2B%2B
        // 不编码的特殊字符： RFC3986文档规定 . - _ ~ 字母，数字属于绝对不编码字符
        // RFC3986文档规定，编码格式 %HH 
        // W3C标准中规定，查询字符串中的空格，需要编码为+， 解码则是+转空格
        static std::string UrlEncode(std::string& url,bool convert_space_to_plus = true)
        {
            std::string ret;
            for(auto& ch:url)
            {
                if(ch == '.' || ch == '-' || ch == '_' || ch == '~' || isalnum(ch))
                {
                    ret += ch;
                }
                else if(ch == ' ' && convert_space_to_plus)
                {
                    ret += '+';
                }
                else
                {
                    // 剩下的字符都是需要编码成为 %HH 格式
                    char tmp[4] = {0};
                    size_t len = sizeof(tmp)/sizeof(tmp[0]);
                    snprintf(tmp,len,"%%%02X",ch);
                    ret += tmp;
                }
            }
            return ret;
        }

        static char HEXTOI(char ch)
        {
            if(isalnum(ch))
            {
                if(ch >= '0' && ch <= '9')
                    return ch - '0';
                else if(ch >= 'a' && ch <= 'z')
                    return ch - 'a' + 10;
                else if(ch >= 'A' && ch <= 'Z')
                    return ch - 'A' + 10;
            }

            return -1;
        }

        // Url 解码
        static std::string UrlDecode(const std::string& url, bool convert_plus_to_space = true)
        {
            // 遇到了%，则将紧随其后的2个字符，转换为数字，第一个数字左移4位，然后加上第二个数字  + -> 2b  %2b->2 << 4 + 11
            std::string ret;
            for(size_t i=0; i<url.size(); i++)
            {
                if(url[i]=='+' && convert_plus_to_space)
                {
                    ret += ' ';
                }
                else if(url[i]=='%' && (i+2)<url.size())
                {
                    char v1 = HEXTOI(url[i+1]);
                    char v2 = HEXTOI(url[i+2]);
                    char val = v1 * 16 + v2;
                    ret += val;
                    i += 2;
                }
                else
                {
                    ret += url[i];
                }
            }
            return ret;
        }

        // 响应状态的描述信息获取
        static std::string StatuDesc(int statu)
        {
            auto it  = statu_msg.find(statu);
            if(it == statu_msg.end())
                return "Unknow";
            
            return it->second;
        }

        // 根据文件后缀名获取文件mime
        static std::string ExtMime(const std::string& filename)
        {
            size_t pos = filename.find_last_of('.');
            if(pos == std::string::npos)
                return "application/octet-stream";
            
            // 根据拓展名，获取mime
            std::string postfix = filename.substr(pos);
            auto it = mime_msg.find(postfix);
            if(it == mime_msg.end())
                return "application/octet-stream";

            return it->second;
        }

        // 判断是否为目录
        static bool IsDirectoryExists(const std::string& filename)
        {
            struct stat st;
            if(stat(filename.c_str(),&st) <= 0)
                return false;
            
            return S_ISDIR(st.st_mode);
        }

        // 判断是否为普通文件
        static bool IsFileExists(const std::string& filename)
        {
            struct stat st;
            if(stat(filename.c_str(),&st) <= 0)
                return false;
            
            return S_ISREG(st.st_mode);
        }

        // htpp 请求的资源路径有效判断
        // /index.html  --- 前边的/叫做相对根目录  映射的是某个服务器上的子目录
        // 想表达的意思就是，客户端只能请求相对根目录中的资源，其他地方的资源都不予理会
        // /../login, 这个路径中的..会让路径的查找跑到相对根目录之外，这是不合理的，不安全的
        static bool ValidPath(const std::string& pathname)
        {
            // 思想：按照/进行路径分割，根据有多少子目录，计算目录深度，有多少层，深度不能小于0
            std::vector<std::string> subdir;
            SplitString(pathname,&subdir,"/");
            int level = 0;
            for(auto& dir:subdir)
            {
                if(dir.empty() || dir==".")
                {
                    continue;
                }
                else if(dir=="..")
                {
                    level--;
                    if(level<0)
                        return false;
                }
                else
                {
                    level++;
                }
            }
            return true;
        }
    };
}