#pragma once
#include "Buffer.hpp"
#include "Channel.hpp"
#include "Socket.hpp"
#include "EventLoop.hpp"

namespace server_connection
{
    using namespace server_buffer;
    using namespace server_channel;
    using namespace server_socket;
    using namespace server_eventloop;

    class Any
    {

    };

    enum class ConnectStatus
    {
        DISCONNECTING,
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };

    
    class Connection
    {
        using PtrConnection = std::shared_ptr<Connection>;
        using ConnectedCallback = std::function<void(const PtrConnection&)>;
        using MessageCallback   = std::function<void(const PtrConnection&, ServerBuffer*)>;
        using ClosedCallback    = std::function<void(const PtrConnection&)>;
        using AnyEventCallback  = std::function<void(const PtrConnection&)>;

    public:
        Connection(EventLoop* loop, uint64_t conn_id, int sockfd)
        :_conn_id(conn_id)
        ,_sockfd(sockfd)
        ,_enable_inactive_release(false)
        ,_loop(loop)
        ,_status(ConnectStatus::CONNECTING)
        ,_socket(_sockfd)
        ,_channel(loop,_sockfd)
        {

        }

        int SocketFd() { return _sockfd;  }
        int ConnId()   { return _conn_id; }

        bool Connected() { return _status == ConnectStatus::CONNECTED; }

        void SetContext(const Any& context) { _context = context; }

        Any* GetContext() { return &_context; }

        void SetConnectedCallback(const ConnectedCallback& cb) { _connected_cb = cb; }
        void SetMessageCallback(const MessageCallback& cb)     { _message_cb = cb; }
        void SetClosedCallback(const ClosedCallback& cb)       { _closed_cb = cb; }
        void SetAnyEventCallback(const AnyEventCallback& cb)   { _event_cb = cb; }
        void SetServerClosedCallBack(const ClosedCallback& cb) { _server_closed_cb = cb; }
        
        // 连接建立就绪后，进行channel回调设置，启动读监控，调用_connected_callback
        void Established()
        {

        }

        // 发送数据，将数据放到发送缓冲区，启动写事件监控
        void Send(const char *data, size_t len)
        {

        }

        // 提供给组件使用者的关闭接口--并不实际关闭，需要判断有没有数据待处理
        void Shutdown()
        {

        }

        void Release()
        {

        }

        // 启动非活跃销毁，并定义多长时间无通信就是非活跃，添加定时任务
        void EnableInactiveRelease(int sec)
        {

        }

        // 取消非活跃销毁
        void CancelInactiveRelease()
        {

        }

        // 切换协议 -- 重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
        // 防备新的事件触发后，处理的时候，切换任务还没有被执行 -- 会导致数据使用原协议处理了。
        void Upgrade(const Any &context, const ConnectedCallback &conn, const MessageCallback &msg, 
                     const ClosedCallback &closed, const AnyEventCallback &event)
        {

        }

        ~Connection()
        {

        }

    private:
    private:
        uint64_t _conn_id;                  // 链接的唯一ID，用于链接的管理与查找，同时作为定时器ID使用
        int _sockfd;                        // 链接关联的文件描述符
        bool _enable_inactive_release;      // 连接是否启动非活跃销毁的判断标志，默认为false
        EventLoop* _loop;                   // 链接所关联的EventLoop
        ConnectStatus _status;               // 链接状态
        Socket _socket;                     // 套接字操作管理
        Channel _channel;                   // 链接的事件管理
        ServerBuffer _in_buf;               // 输入缓冲区 -- 存放从socket中读取到的数据
        ServerBuffer _out_buf;              // 输出缓冲区 -- 存放要发送给对端的数据
        Any _context;                       // 请求的接收处理上下文
        
        ConnectedCallback _connected_cb;
        MessageCallback   _message_cb;
        ClosedCallback    _closed_cb;
        AnyEventCallback  _event_cb;

        // 组件内的连接关闭回调--组件内设置的，因为服务器组件内会把所有的连接管理起来，
        // 一旦某个连接要关闭就应该从管理的地方移除掉自己的信息
        ClosedCallback _server_closed_cb;
    };
}