#include "../../http/Util.hpp"

using namespace http_util;

void EncodeAndDecodetest()
{
    std::string s = "011,,,..,dd  d///,,";
    std::string tmp = Util::UrlEncode(s);
    std::cout << tmp << std::endl;
    std::cout << Util::UrlDecode(tmp) <<std::endl;
}

void validpathtest()
{
    struct Case
    {
        std::string path;
        bool expect;
    };

    std::vector<Case> cases = {
        {"/",                 true},
        {"/index.html",       true},
        {"/static/a.png",     true},
        {"/a/b/c",            true},
        {"/a/./b",            true},
        {"/..",               false},
        {"/../login",         false},
        {"/a/../b",           true},
        {"/a/b/../../c",      true},
        {"/a/../../c",        false},
        {"../etc/passwd",     false},
        {"a/../..",           false},
        {"/a/b/../../../x",   false},
    };

    int failed = 0;
    for(auto& c : cases)
    {
        bool ret = Util::ValidPath(c.path);
        bool ok = (ret == c.expect);
        if(!ok)
            failed++;

        std::cout << (ok ? "[PASS] " : "[FAIL] ")
                  << "path=\"" << c.path << "\" "
                  << "expect=" << (c.expect ? "true" : "false")
                  << " got=" << (ret ? "true" : "false")
                  << std::endl;
    }

    std::cout << "total=" << cases.size()
              << " failed=" << failed << std::endl;
}

int main()
{
    // EncodeAndDecodetest();
    validpathtest();

    return 0;
}