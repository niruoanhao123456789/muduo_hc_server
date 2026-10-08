#include "../../source/Server.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sys/time.h>

using namespace server;

// 请求默认使用长连接；配合 reqs/conn 可在一条TCP连接上连续压测请求处理吞吐
static const char* REQ = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
static const size_t REQ_LEN = strlen(REQ);

// 压测统计信息（多线程共享）
struct Stats
{
    std::atomic<uint64_t> total{0};     // 实际发出的请求数
    std::atomic<uint64_t> success{0};   // 返回200的请求数
    std::atomic<uint64_t> failed{0};    // 失败/超时请求数
    std::atomic<uint64_t> bytes{0};     // 收到的响应字节数
};

// 给客户端套接字设置收包超时，避免服务器异常时压测线程被无限阻塞
static void SetRecvTimeout(int fd, int sec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// 从响应头中解析 Content-Length，未找到返回 SIZE_MAX
static size_t FindContentLength(const std::string& headers)
{
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    size_t pos = lower.find("content-length:");
    if(pos == std::string::npos)
        return SIZE_MAX;
    pos += strlen("content-length:");

    while(pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t'))
        pos++;

    size_t begin = pos;
    while(pos < headers.size() && std::isdigit(static_cast<unsigned char>(headers[pos])))
        pos++;

    if(pos == begin)
        return SIZE_MAX;
    return std::stoull(headers.substr(begin, pos - begin));
}

// 解析状态码，形如 "HTTP/1.1 200 OK"
static int ParseStatus(const std::string& buf)
{
    size_t sp = buf.find(' ');
    if(sp == std::string::npos || sp + 4 > buf.size())
        return -1;
    return std::stoi(buf.substr(sp + 1, 3));
}

// 接收一条完整的HTTP响应（头部 + Content-Length长度的正文），返回状态码，出错/超时返回-1
// 按Content-Length精确收取，保证长连接上请求/响应不会错位
static int RecvResponse(Socket& client, uint64_t& bytesRecv)
{
    std::string buf;
    buf.reserve(4096);
    char tmp[4096];
    size_t header_end = std::string::npos;

    while((header_end = buf.find("\r\n\r\n")) == std::string::npos)
    {
        ssize_t n = client.Recv(tmp, sizeof(tmp));
        if(n <= 0)
            return -1;
        buf.append(tmp, n);
        bytesRecv += n;
        if(buf.size() > (1u << 20))  // 防御：响应头异常过大
            return -1;
    }

    size_t len = FindContentLength(buf.substr(0, header_end));
    if(len != SIZE_MAX)
    {
        size_t need = header_end + 4 + len;
        while(buf.size() < need)
        {
            ssize_t n = client.Recv(tmp, sizeof(tmp));
            if(n <= 0)
                return -1;
            buf.append(tmp, n);
            bytesRecv += n;
        }
    }

    return ParseStatus(buf);
}

static bool SendAll(Socket& client, const char* data, size_t len)
{
    size_t off = 0;
    while(off < len)
    {
        ssize_t n = client.Send(data + off, len - off);
        if(n <= 0)
            return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

// 发送一次请求并接收响应，更新统计；成功返回true
static bool DoRequest(Socket& client, Stats& st, std::vector<uint64_t>& latencies)
{
    auto t0 = std::chrono::steady_clock::now();

    if(!SendAll(client, REQ, REQ_LEN))
    {
        st.total++;
        st.failed++;
        return false;
    }

    uint64_t bytesRecv = 0;
    int status = RecvResponse(client, bytesRecv);
    auto t1 = std::chrono::steady_clock::now();

    st.total++;
    st.bytes += bytesRecv;

    if(status == 200)
    {
        st.success++;
        latencies.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
        return true;
    }

    st.failed++;
    return false;
}

// 单个压测线程
// short_mode=true  : 每个请求新建一条连接（测连接建立+请求）
// short_mode=false : 每条连接发送 reqs 条长连接请求
static void Worker(Stats& st,
                   const std::string& ip, uint16_t port,
                   int conns, int reqs, bool short_mode,
                   std::vector<uint64_t>& latencies)
{
    latencies.reserve(static_cast<size_t>(conns) * reqs);

    if(short_mode)
    {
        int total = conns * reqs;
        for(int i = 0; i < total; ++i)
        {
            Socket client;
            if(!client.CreateClient(port, ip))
            {
                st.total++;
                st.failed++;
                continue;
            }
            SetRecvTimeout(client.Fd(), 3);
            DoRequest(client, st, latencies);
            client.Close();
        }
        return;
    }

    for(int c = 0; c < conns; ++c)
    {
        Socket client;
        if(!client.CreateClient(port, ip))
        {
            st.total += reqs;
            st.failed += reqs;
            continue;
        }
        SetRecvTimeout(client.Fd(), 3);

        for(int r = 0; r < reqs; ++r)
        {
            if(!DoRequest(client, st, latencies))
                break; // 连接异常/超时，换下一条连接
        }

        client.Close();
    }
}

static uint64_t Percentile(const std::vector<uint64_t>& sorted, double p)
{
    if(sorted.empty())
        return 0;
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    return sorted[idx];
}

static void Usage(const char* proc)
{
    std::cout << "Usage: " << proc
              << " [ip] [port] [threads] [conns_per_thread] [reqs_per_conn] [mode]\n"
              << "  mode: keepalive(default) | short\n"
              << "  default: 127.0.0.1 8080 4 4 1000 keepalive" << std::endl;
}

int main(int argc, char* argv[])
{
    std::string ip = "127.0.0.1";
    uint16_t port = 8080;
    int threads = 4;
    int conns = 4;
    int reqs = 1000;
    std::string mode = "keepalive";

    if(argc >= 2) ip = argv[1];
    if(argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));
    if(argc >= 4) threads = std::stoi(argv[3]);
    if(argc >= 5) conns = std::stoi(argv[4]);
    if(argc >= 6) reqs = std::stoi(argv[5]);
    if(argc >= 7) mode = argv[6];

    if(threads <= 0 || conns <= 0 || reqs <= 0 ||
       (mode != "keepalive" && mode != "short"))
    {
        Usage(argv[0]);
        return 1;
    }

    bool short_mode = (mode == "short");
    uint64_t expect = static_cast<uint64_t>(threads) * conns * reqs;
    std::cout << "Target http://" << ip << ":" << port
              << "  threads=" << threads
              << " conns/thread=" << conns
              << " reqs/conn=" << reqs
              << " mode=" << mode
              << "  (expect " << expect << " requests)" << std::endl;

    Stats st;
    std::vector<std::vector<uint64_t>> localLat(threads);
    std::vector<std::thread> pool;
    pool.reserve(threads);

    auto start = std::chrono::steady_clock::now();
    for(int i = 0; i < threads; ++i)
        pool.emplace_back(Worker, std::ref(st), std::cref(ip), port,
                          conns, reqs, short_mode, std::ref(localLat[i]));
    for(auto& t : pool)
        t.join();
    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();

    std::vector<uint64_t> lat;
    size_t totalLat = 0;
    for(auto& v : localLat)
        totalLat += v.size();
    lat.reserve(totalLat);
    for(auto& v : localLat)
        lat.insert(lat.end(), v.begin(), v.end());
    std::sort(lat.begin(), lat.end());

    uint64_t total = st.total.load();
    uint64_t success = st.success.load();
    uint64_t failed = st.failed.load();
    uint64_t bytes = st.bytes.load();

    uint64_t sum = 0;
    for(uint64_t x : lat)
        sum += x;

    double qps = seconds > 0 ? success / seconds : 0;
    double mbps = seconds > 0 ? (bytes / 1024.0 / 1024.0) / seconds : 0;
    double avg = lat.empty() ? 0 : static_cast<double>(sum) / lat.size();

    std::cout << "---------------- HTTP Perf Result ----------------" << std::endl;
    std::cout << "elapsed      : " << seconds << " s" << std::endl;
    std::cout << "total        : " << total << std::endl;
    std::cout << "success(200) : " << success << std::endl;
    std::cout << "failed       : " << failed << std::endl;
    std::cout << "QPS          : " << qps << " req/s (success/elapsed)" << std::endl;
    std::cout << "throughput   : " << mbps << " MB/s" << std::endl;
    std::cout << "latency(us)  : avg=" << avg
              << " min=" << (lat.empty() ? 0 : lat.front())
              << " p50=" << Percentile(lat, 0.50)
              << " p90=" << Percentile(lat, 0.90)
              << " p99=" << Percentile(lat, 0.99)
              << " max=" << (lat.empty() ? 0 : lat.back()) << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    return failed == 0 ? 0 : 1;
}
