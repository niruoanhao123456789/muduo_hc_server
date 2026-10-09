# muduo_hc_server

一个使用 C++20 编写的高并发、多线程 Reactor 网络服务器，参考 muduo 的架构设计。
它实现了完整的 `one loop per thread + 线程池` 事件驱动核心，并附带可复用的
HTTP/1.1 服务器与 echo 服务器示例。

## 特性

- **多线程 Reactor 核心**：每个线程一个 `EventLoop`，主循环负责接收连接，I/O 线程池
  负责处理连接（muduo 风格）。
- **基于 epoll 的事件分发**：`Poller` + `Channel` 抽象文件描述符事件的注册、修改与分发。
- **非阻塞套接字**，默认开启 `TCP_NODELAY` 与 `SO_REUSEADDR`。
- **环形缓冲区 I/O**：可自动扩容的循环 `ServerBuffer`，使用分散/聚集写
  （`sendmsg`/`iovec`）并正确处理缓冲区回绕。
- **时间轮定时器**：`timerfd` + 哈希时间轮，用于空闲连接检测与延迟/周期任务。
- **连接生命周期管理**：基于 `shared_ptr` / `weak_ptr`，支持半关闭、优雅关闭与
  非活跃连接自动释放。
- **协议升级钩子**：连接可以重新绑定到新的协议（HTTP 服务器即用它挂载每连接的解析上下文）。
- **HTTP/1.1 服务器**：基于正则的路由，支持 GET/HEAD/POST/PUT/DELETE，静态文件服务与
  MIME 识别，URL 解码，查询字符串解析以及 keep-alive。
- **独立日志库**：同步与异步日志器、日志级别、格式化、按时间与大小滚动、stdout/文件输出。

## 环境要求

- Linux（使用 `epoll`、`eventfd`、`timerfd`）
- 支持 C++20 的 g++（已在 g++ 13 上测试）
- Boost 头文件（仅 `http/Util.hpp` 即 HTTP 模块需要）

## 构建与运行

### HTTP 服务器

```bash
cd http
make            # 生成 ./server
./server 8080   # 监听 8080 端口
```

在浏览器中打开 `http://127.0.0.1:8080/`，或在 `http/main.cc` 中注册路由：

```cpp
HttpServer server(port, 10);      // 端口, 空闲超时时间（秒）
server.SetThreadCount(3);         // I/O 线程池大小
server.Get("/hello", Hello);      // 正则路径 -> 处理函数
server.Post("/login", Login);
server.ListenAndStart();
```

日志文件写入 `http/logs/`（按天滚动）。

### Echo 服务器

```bash
cd echo
make                 # 生成 echoserver 与 echoclient
./echoserver 8080
./echoclient <ip> <port>
```

## 编写自己的服务器

核心库位于 `source/`，为纯头文件实现。包含总头文件后即可构建 `TcpServer`：

```cpp
#include "source/Server.hpp"
using namespace server_tcp;

TcpServer server(8080);
server.SetThreadCount(4);
server.EnableInactiveRelease(10);
server.SetConnectedCallBack([](const PtrConnection& conn){ /* ... */ });
server.SetMessageCallBack([](const PtrConnection& conn, ServerBuffer* buf){
    conn->Send(buf->ReadPosition(), buf->ReadableSize());
    buf->MoveRindex(buf->ReadableSize());
});
server.SetClosedCallBack([](const PtrConnection& conn){ /* ... */ });
server.Start();
```

## 项目结构

```
.
├── source/                 # 核心 Reactor 网络库（纯头文件）
│   ├── Server.hpp          # 总头文件
│   ├── EventLoop.hpp       # EventLoop, LoopThread, LoopThreadPool
│   ├── Poller.hpp          # epoll 封装
│   ├── Channel.hpp         # fd + 事件回调
│   ├── Socket.hpp          # 套接字系统调用封装
│   ├── Acceptor.hpp        # 监听套接字管理
│   ├── Connection.hpp      # 连接状态机 + Any 类型
│   ├── TcpServer.hpp       # 顶层 TCP 服务器
│   ├── Buffer.hpp          # 循环 ServerBuffer
│   └── TimerWheel.hpp      # 基于 timerfd 的时间轮
├── log/                    # 日志库（同步 + 异步，滚动 Sink）
├── http/                   # HTTP/1.1 服务器 + 示例 main
├── echo/                   # Echo 服务器/客户端示例
└── test/                   # 单元与集成测试
    ├── server_test/        # 核心模块测试
    └── http_test/          # HTTP 客户端 / 性能测试
```

### 核心组件

| 组件            | 职责                                                              |
| --------------- | ---------------------------------------------------------------- |
| `EventLoop`     | 事件循环、任务队列、`eventfd` 唤醒、定时任务调度                 |
| `LoopThread`    | 在独立线程中运行 `EventLoop` 并对外提供                        |
| `LoopThreadPool`| 以轮询方式把新连接分配到各 I/O 线程                             |
| `Poller`        | `epoll_create/wait/ctl` 封装，维护 fd 到 `Channel` 的映射       |
| `Channel`       | 将 fd 与读/写/错误/关闭回调及关注事件绑定                        |
| `Socket`        | 非阻塞套接字操作（`accept`/`recv`/`sendmsg`/选项）              |
| `Acceptor`      | 持有监听套接字，将新 fd 交给 `TcpServer`                        |
| `Connection`    | 每连接状态机、输入/输出缓冲区、各类回调                          |
| `TcpServer`     | 持有 acceptor、线程池与连接表，设置回调                          |
| `ServerBuffer`  | 自动扩容的循环缓冲区，读写均正确处理回绕                        |
| `TimerWheel`    | `timerfd` 驱动的哈希时间轮，用于空闲释放与定时任务              |
| `Any`           | 类型擦除的上下文容器，用于携带协议状态                          |

## 测试

```bash
cd test/server_test
make            # 构建全部核心测试可执行文件
./eventlooptest
./tcpservertest

cd ../http_test
make test       # 构建 ./test
make perf       # 构建 ./perf
```

## 许可证

基于 Apache License 2.0 授权，详见 [LICENSE](LICENSE)。
