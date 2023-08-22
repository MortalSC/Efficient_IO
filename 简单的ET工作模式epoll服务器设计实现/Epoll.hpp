#pragma once

#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>

// 对 TcpServer 提供统一的接口封装！
class Epoll
{
    const static int default_size = 256;
    const static int default_timeout = 5000; // 是指默认的是 5 秒
public:
    Epoll(int timeout = default_timeout)
        : _timeout(timeout)
    {
    }
    void CreateEpoll()
    {
        // epoll 本身也就是文件描述符
        _epfd = epoll_create(default_size);
        if (_epfd < 0)
        {
            exit(5);
        }
    }

    /*
        sock：监测的文件描述符对象
        events：监测的动作类型
    */
    bool AddSockToEpoll(int sock, uint32_t events)
    {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = sock;
        int n = epoll_ctl(_epfd, EPOLL_CTL_ADD, sock, &ev);
        if (n > 0)
            return true;
        return false;
    }

    bool CtlEpoll(int sock, uint32_t events)
    {
        events |= EPOLLET; // 设定 ET 模式
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = sock;
        int n = epoll_ctl(_epfd, EPOLL_CTL_MOD, sock, &ev);
        return n == 0;
    }

    // 在 epoll 中删除！
    bool DelFromEpoll(int sock)
    {
        int n = epoll_ctl(_epfd, EPOLL_CTL_DEL, sock, nullptr);
        return n == 0;
    }

    // 用于获取下层已经就绪的文件描述符
    /*
        recv：捞取就绪资源的载体
        num：单次捞取的最大个数！
    */
    int WaitEpoll(struct epoll_event revs[], int num)
    {
        return epoll_wait(_epfd, revs, num, _timeout);
    }
    ~Epoll()
    {
    }

private:
    int _epfd;
    int _timeout;
};