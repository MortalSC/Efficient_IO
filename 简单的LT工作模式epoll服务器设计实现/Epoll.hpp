#pragma once

#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>

class Epoll
{
    static const int gsize = 256;

public:
    static int CreateEpoll()
    {
        int epfd = epoll_create(gsize);
        if (epfd > 0)
            return epfd;
        // 创建失败直接终止！
        exit(5);
        // return -1;
    }
    /*
        int epfd：哪一个epoll模型进行处理
        int oper：处理的方式（增加、删除、修改）
        int sock：操作的对象（文件描述符）
        uint32_t events：关注的动作！
    */
    static bool CtlEpoll(int epfd, int oper, int sock, uint32_t events)
    {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = sock;
        int n = epoll_ctl(epfd, oper, sock, &ev);
        return n == 0;
    }

    static int WaitEpoll(int epfd, struct epoll_event revs[], int num, int timeout)
    {
        // 注意点：
        // 细节一：如果底层一次就绪的 sock 非常多！revs 装不下怎么办？
        // 答：不影响！一次获取不玩，下次再获取！
        // 细节二：关于epoll_wait返回值含义是有几个就绪，就返回几！
        //        特点：epoll 返回值，会将就绪的event按顺序防止在我们的数组中！好比入栈！
        return epoll_wait(epfd, revs, num, timeout);
    }
};