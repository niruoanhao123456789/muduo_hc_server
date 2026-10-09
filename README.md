# muduo_hc_server

A high-concurrency, multi-threaded Reactor network server written in C++20, modeled after
muduo's architecture. It implements a complete `one loop per thread + thread pool` event
driven core, and ships with a reusable HTTP/1.1 server and an echo server as examples.

## Features

- **Multi-threaded Reactor core**: one `EventLoop` per thread, a main loop for accepting
  connections and an I/O thread pool for handling them (muduo-style).
- **epoll-based event dispatch**: `Poller` + `Channel` abstract file descriptor event
  registration, modification and dispatching.
- **Non-blocking sockets** with `TCP_NODELAY` and `SO_REUSEADDR` enabled.
- **Ring-buffer I/O buffering**: a self-growing circular `ServerBuffer` with scatter-gather
  (`sendmsg`/`iovec`) writes that correctly handle buffer wrap-around.
- **Time wheel timer**: `timerfd` + hashed timing wheel for idle-connection detection and
  delayed / repeated tasks.
- **Connection lifecycle management** via `shared_ptr` / `weak_ptr`, supporting half-close,
  graceful shutdown and inactive-connection release.
- **Protocol upgrade hooks**: connections can be re-bound to a new protocol (used by the HTTP
  server to attach per-connection parsing context).
- **HTTP/1.1 server**: regex-based routing for GET/HEAD/POST/PUT/DELETE, static file serving
  with MIME detection, URL decoding, query string parsing and keep-alive support.
- **Standalone logging library**: synchronous and asynchronous loggers, log levels,
  formatting, rolling by time and by size, stdout/file sinks.

## Requirements

- Linux (uses `epoll`, `eventfd`, `timerfd`)
- g++ with C++20 support (tested with g++ 13)
- Boost headers (only for `http/Util.hpp`, i.e. the HTTP module)

## Build & Run

### HTTP server

```bash
cd http
make            # produces ./server
./server 8080   # listens on port 8080
```

Open `http://127.0.0.1:8080/` in a browser, or register routes in `http/main.cc`:

```cpp
HttpServer server(port, 10);      // port, idle timeout (seconds)
server.SetThreadCount(3);         // I/O thread pool size
server.Get("/hello", Hello);      // regex path -> handler
server.Post("/login", Login);
server.ListenAndStart();
```

Log files are written to `http/logs/` (rolling daily).

### Echo server

```bash
cd echo
make                 # produces echoserver and echoclient
./echoserver 8080
./echoclient <ip> <port>
```

## Writing your own server

The core library is header-only under `source/`. Include the umbrella header and build a
`TcpServer`:

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

## Project layout

```
.
├── source/                 # Core Reactor networking library (header-only)
│   ├── Server.hpp          # Umbrella header
│   ├── EventLoop.hpp       # EventLoop, LoopThread, LoopThreadPool
│   ├── Poller.hpp          # epoll wrapper
│   ├── Channel.hpp         # fd + event callbacks
│   ├── Socket.hpp          # socket syscall wrapper
│   ├── Acceptor.hpp        # listening socket management
│   ├── Connection.hpp      # connection state machine + Any type
│   ├── TcpServer.hpp       # top-level TCP server
│   ├── Buffer.hpp          # circular ServerBuffer
│   └── TimerWheel.hpp      # timerfd-based timing wheel
├── log/                    # Logging library (sync + async, rolling sinks)
├── http/                   # HTTP/1.1 server + demo main
├── echo/                   # Echo server/client example
└── test/                   # Unit and integration tests
    ├── server_test/        # Core module tests
    └── http_test/          # HTTP client / performance tests
```

### Core components

| Component       | Responsibility                                                        |
| --------------- | --------------------------------------------------------------------- |
| `EventLoop`     | Event loop, task queue, `eventfd` wake-up, timer scheduling           |
| `LoopThread`    | Runs an `EventLoop` in its own thread and exposes it                  |
| `LoopThreadPool`| Distributes new connections across I/O threads (round-robin)          |
| `Poller`        | `epoll_create/wait/ctl` wrapper, maps fd to `Channel`                 |
| `Channel`       | Binds an fd to read/write/error/close callbacks and event interests   |
| `Socket`        | Non-blocking socket operations (`accept`/`recv`/`sendmsg`/options)    |
| `Acceptor`      | Owns the listening socket and hands new fds to `TcpServer`            |
| `Connection`    | Per-connection state machine, input/output buffers, callbacks         |
| `TcpServer`     | Owns acceptor, thread pool and connection map; sets callbacks         |
| `ServerBuffer`  | Auto-growing circular buffer with wrap-around aware read/write        |
| `TimerWheel`    | `timerfd`-driven hashed timing wheel for idle-release and tasks       |
| `Any`           | Type-erased context holder used to carry protocol state               |

## Tests

```bash
cd test/server_test
make            # builds all core test binaries
./eventlooptest
./tcpservertest

cd ../http_test
make test       # builds ./test
make perf       # builds ./perf
```

## License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE).
