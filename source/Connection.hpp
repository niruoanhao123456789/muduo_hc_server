#pragma once
#include <memory>
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
    public:
        Any()
        :_content(nullptr)
        {}

        template<class T>
        Any(const T& val)
        :_content(new placeholder<T>(val))
        {}

        Any(const Any& other)
        :_content(other._content ? other._content->clone() : nullptr)
        {}

        Any& operator=(const Any& other)
        {
            if(this != &other)
            {
                Any tmp(other);
                tmp.swap(*this);
            }
            return *this;
        }

        Any& swap(Any& other)
        {
            std::swap(_content,other._content);
            return *this;
        }

        // 返回子类对象保存的数据的指针
        template<class T>
        T* get()
        {
            // 想要获取的数据类型，必须和保存的数据类型一致
            assert(typeid(T) == _content->type());
            return &((placeholder<T>*)_content)->_val;
        }

        template<class T>
        Any& operator=(const T& val)
        {
            Any(val).swap(*this);
            return *this;
        }

        ~Any()
        {
            delete _content;
        }

    private:
        class holder
        {
        public:
            virtual ~holder() {}
            virtual const std::type_info& type() = 0;
            virtual holder* clone() = 0;
        };

        template<class T>
        class placeholder: public holder
        {
        public:
            placeholder(const T& val)
            :_val(val)
            {}

            // 获取子类对象保存的数据
            virtual const std::type_info& type()
            {
                return typeid(T);
            }

            virtual holder* clone()
            {
                return new placeholder(_val);
            }

        public:
            T _val;
        };

    private:
        holder* _content;
    };

    enum class ConnectStatus
    {
        DISCONNECTING,
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };

    class Connection;
    using PtrConnection = std::shared_ptr<Connection>;

    class Connection : public std::enable_shared_from_this<Connection>
    {
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
            _channel.SetCloseCallBack(std::bind(&Connection::HandleClose,this));
            _channel.SetErrorCallBack(std::bind(&Connection::HandleError,this));
            _channel.SetEventCallBack(std::bind(&Connection::HandleEvent,this));
            _channel.SetReadCallBack (std::bind(&Connection::HandleRead ,this));
            _channel.SetWriteCallBack(std::bind(&Connection::HandleWrite,this));
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
            _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop,this));
        }

        // 发送数据，将数据放到发送缓冲区，启动写事件监控
        void Send(const char* data, size_t len)
        {
            assert(data);
            ServerBuffer buf;
            buf.Write(data,len);
            _loop->RunInLoop(std::bind(&Connection::SendInLoop,this,std::move(buf)));
        }

        // 提供给组件使用者的关闭接口--并不实际关闭，需要判断有没有数据待处理
        void Shutdown()
        {
            _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop,this));
        }

        void Release()
        {
            _loop->PushQueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
        }

        // 启动非活跃销毁，并定义多长时间无通信就是非活跃，添加定时任务
        void EnableInactiveRelease(int sec)
        {
            _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop,this,sec));
        }

        // 取消非活跃销毁
        void CancelInactiveRelease()
        {
            _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop,this));
        }

        // 切换协议 -- 重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
        // 防备新的事件触发后，处理的时候，切换任务还没有被执行 -- 会导致数据使用原协议处理了。
        void Upgrade(const Any &context, const ConnectedCallback &conn_cb, const MessageCallback &msg_cb, 
                     const ClosedCallback &closed_cb, const AnyEventCallback &event_cb)
        {
            _loop->AssertInLoop();
            _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop,this,context,conn_cb,msg_cb,closed_cb,event_cb));
        }

        ~Connection()
        {
            LOG_INFOR_STREAM(GetLogger("ServerLogger")) << "Release connection: " << this;
        }

    private:
        // 五个channel的事件回调函数
        void HandleRead()
        {
            // 接受socket数据，放到缓冲区里
            char buffer[65536] = {0};
            ssize_t ret = _socket.NonBlockRecv(buffer,sizeof buffer);
            if(ret < 0)
            {
                // 出错了不能直接关闭链接
                ShutdownInLoop();
                return;
            }
            else if(ret > 0)
            {
                // 这里的等于0表示的是没有读取到数据，而并不是连接断开了，连接断开返回的是-1
                // 将数据放入输入缓冲区,写入之后顺便将写偏移向后移动
                _in_buf.Write(buffer,ret);
            }
            
            // 调用_message_cb进行业务处理
            if(_in_buf.ReadableSize() > 0)
            {
                // 从当前对象自身获取自身的shared_ptr管理对象
                if(_message_cb)
                        _message_cb(shared_from_this(),&_in_buf);
                else
                    LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_message_cb is nullptr!";
            }
        }

        void HandleWrite()
        {
            // 环形缓冲区回绕时，可读数据分为 [ReadPosition, 物理末尾) 与 [Begin, 头部) 两段
            // 用iovec一次sendmsg把两段聚集发出：只一次系统调用、无需拷贝，也不会越过物理末尾读越界
            // 未回绕时只有一段（iovcnt=1）；部分写时按实际发送字节推进读索引，剩余部分下次继续
            struct iovec iov[2];
            int iovcnt = 0;

            size_t tail_len = _out_buf.ReadableSizeContiguous();
            if(tail_len)
            {
                iov[iovcnt].iov_base = _out_buf.ReadPosition();
                iov[iovcnt].iov_len  = tail_len;
                ++iovcnt;
            }
            size_t head_len = _out_buf.ReadableSize() - tail_len;
            if(head_len)
            {
                iov[iovcnt].iov_base = _out_buf.Begin();
                iov[iovcnt].iov_len  = head_len;
                ++iovcnt;
            }

            ssize_t n = _socket.NonBlockSendV(iov,iovcnt);
            if(n < 0)
            {
                // 如果是发送错误就该关闭连接             
                while(_in_buf.ReadableSize() > 0)
                {
                    size_t prevent = _in_buf.ReadableSize();
                    if(_message_cb)
                        _message_cb(shared_from_this(),&_in_buf);
                    else
                        LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_message_cb is nullptr!";

                    if(prevent == _in_buf.ReadableSize())
                    {
                        LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "Reabable datas of _in_buf are still left";
                        break;
                    }
                }
                // 实际关闭释放操作
                Release();
                return;
            }
            _out_buf.MoveRindex(n);

            if(!_out_buf.ReadableSize())
            {
                // 无待发送数据，关闭事件监控
                _channel.DisableWrite();
                // 如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
                if(_status == ConnectStatus::DISCONNECTING)
                {
                    Release();
                    return;
                }
            }
        }

        // 描述符触发挂断事件
        void HandleClose()
        {      
            // 一旦连接挂断了，套接字就什么都干不了了，因此有数据待处理就处理一下，完毕关闭连接
            while(_in_buf.ReadableSize() > 0)
            {
                size_t prevent = _in_buf.ReadableSize();
                if(_message_cb)
                    _message_cb(shared_from_this(),&_in_buf);
                else
                    LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_message_cb is nullptr!";

                if(prevent == _in_buf.ReadableSize())
                {
                    LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "Reabable datas of _in_buf are still left";
                    break;
                }
            }

            Release();
        }

        // 描述符触发出错事件
        void HandleError()
        {
            HandleClose();
        }

        //描述符触发任意事件:
        // 1. 刷新连接的活跃度--延迟定时销毁任务； 
        // 2. 调用组件使用者的任意事件回调
        void HandleEvent()
        {
            if(_enable_inactive_release)
                _loop->TimerRefresh(_conn_id);

            // _event_cb 是可选回调，未设置时不应当每个事件都告警刷屏
            if(_event_cb)
                _event_cb(shared_from_this());
        }

        // 连接获取之后，所处的状态下要进行各种设置（启动读监控,调用回调函数）
        void EstablishedInLoop()
        {
            // 1. 修改连接状态； 
            // 2. 启动读事件监控； 
            // 3. 调用回调函数
            // 当前的状态必须一定是上层的半连接状态
            assert(_status == ConnectStatus::CONNECTING);

            _status = ConnectStatus::CONNECTED;
            // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
            _channel.EnableRead();
            if(_connected_cb)
                _connected_cb(shared_from_this());
            else
                LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_connected_cb is nullptr!";
        }

        // 这个接口才是实际的释放接口
        void ReleaseInLoop()
        {
            // 修改链接状态， 将其设置为DISCONNECTED
            _status = ConnectStatus::DISCONNECTED;

            // 移除连接的事件监控
            _channel.Remove();

            // 关闭描述符
            _socket.Close();

            // 如果当前定时器队列中还有定时销毁任务，则取消任务
            if(_loop->IsTimerExist(_conn_id))
                CancelInactiveReleaseInLoop();

            // 调用关闭回调函数，避免先移除服务器管理的连接信息导致Connection被释放，再去处理会出错，因此先调用用户的回调函数
            if(_closed_cb)
                _closed_cb(shared_from_this());
            else
                LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_closed_cb is nullptr!";

            // 移除服务器内部管理的连接信息
            if(_server_closed_cb)
                _server_closed_cb(shared_from_this());
            else
                LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_server_closed_cb is nullptr!";
        }

        // 这个接口并不是实际的发送接口，而只是把数据放到了发送缓冲区，启动了可写事件监控
        void SendInLoop(ServerBuffer &buf)
        {
            if(_status == ConnectStatus::DISCONNECTED)
                return;

            _out_buf.WriteServerBuffer(buf);

            if(!_channel.Writeable())
                _channel.EnableWrite();
        }

        // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
        void ShutdownInLoop()
        {
            // 设置连接为半关闭状态
            _status = ConnectStatus::DISCONNECTING;
            
            while(_in_buf.ReadableSize() > 0)
            {
                size_t prevent = _in_buf.ReadableSize();
                if(_message_cb)
                    _message_cb(shared_from_this(),&_in_buf);
                else
                    LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "_message_cb is nullptr!";

                if(prevent == _in_buf.ReadableSize())
                {
                    LOG_WARNNING_STREAM(GetLogger("ServerLogger")) << "Reabable datas of _in_buf are still left!";
                    break;
                }
            }

            // 要么就是写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
            if(_out_buf.ReadableSize() > 0)
            {
                if(!_channel.Writeable())
                    _channel.EnableWrite();
            }
            else if(!_out_buf.ReadableSize())
                Release();
        }

        // 启动非活跃连接超时释放规则
        void EnableInactiveReleaseInLoop(int sec)
        {
            // 将判断标志 _enable_inactive_release 置为true
            _enable_inactive_release = true;
        
            // 如果当前定时销毁任务已经存在，那就刷新延迟一下
            if(_loop->IsTimerExist(_conn_id))
            {
                _loop->TimerRefresh(_conn_id);
            }
            else
            {
                // 如果不存在定时销毁任务，则新增
                _loop->TimerAdd(_conn_id,sec,std::bind(&Connection::Release,this));
            }
        }

        void CancelInactiveReleaseInLoop()
        {
            _enable_inactive_release = false;
            if(_loop->IsTimerExist(_conn_id))
                _loop->TimerCancel(_conn_id);
        }

        void UpgradeInLoop(const Any& context,const ConnectedCallback& conn_cb,
                           const MessageCallback& msg_cb,const ClosedCallback& closed_cb,
                           const AnyEventCallback& event_cb)
        {
            _context = context;
            _connected_cb = conn_cb;
            _message_cb = msg_cb;
            _closed_cb = closed_cb;
            _event_cb = event_cb;
        }

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