#pragma  once
#include <iostream>
#include <unordered_map>
#include <vector>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>
#include "../log/Log.hpp"
#include "Channel.hpp"

namespace server_poller
{
    using namespace server_channel;
    using namespace LogModule;

    #define MAX_EPOLLEENT 1024

    class Poller
    {
    public:
        Poller()
        {
            _epfd = epoll_create(MAX_EPOLLEENT);
            if(_epfd<0)
            {
                LOGE_STREAM()<<"Epoll create failed!";
                abort();
            }
        }

        // 添加或修改监控的事件
        void UpdateEvent(Channel* channel)
        {
            bool ret = HasChannel(channel);
            if(!ret)
            {
                _chennels.insert(std::make_pair(channel->Fd(),channel));
                return Update(channel,EPOLL_CTL_ADD);
            }
            else
                return Update(channel,EPOLL_CTL_MOD);
        }

        // 移除事件监控
        void RemoveEvent(Channel* channel)
        {
            auto it = _chennels.find(channel->Fd());
            if(it != _chennels.end())
                _chennels.erase(it);
            Update(channel,EPOLL_CTL_DEL);
        }

        // 进行监控，返回活跃事件
        void Poll(std::vector<Channel*>* active)
        {
            int nfds = epoll_wait(_epfd,_evs,MAX_EPOLLEENT,-1);
            if(nfds < 0)
            {
                if(errno == EINTR)
                    return;
                LOGE_STREAM() << "Epoll wait error: "<< strerror(errno);
                abort();
            }
            for(int i=0;i<nfds;i++)
            {
                auto it = _chennels.find(_evs[i].data.fd);
                assert(it != _chennels.end());
                it->second->SetREvents(_evs[i].events); // 设置实际就绪的事件
                active->emplace_back(it->second);
            }
        }

    private:
        // 对epoll的直接操作
        void Update(Channel* channel, int op)
        {
            int fd = channel->Fd();
            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = channel->Events();
            int ret = epoll_ctl(_epfd,op,fd,&ev);
            if(ret<0)
            {
                LOGE_STREAM() << "Epollctl failed!";
                return;
            }
        }

        // 判断channel是否进行事件监控
        bool HasChannel(Channel* channel)
        {
            auto it = _chennels.find(channel->Fd());
            if(it == _chennels.end())
                return false;
            else
                return true;
        }

    private:
        int _epfd;
        struct epoll_event _evs[MAX_EPOLLEENT];
        std::unordered_map<int,Channel* > _chennels;
    };
}