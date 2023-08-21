#ifndef _EPOLL_SVR_H_
#define _EPOLL_SVR_H_

#include <iostream>
#include "log.hpp"
#include "Sock.hpp"
#include "Epoll.hpp"
#include <cassert>
#include <functional>

// 当前只设置了读取！
namespace ns_Epoll
{
    static const int default_port = 8080; // 设置默认端口
    static const int gnum = 64;
    class EpollServer
    {
        using func_t = std::function<void(std::string)>;

    public:
        EpollServer(func_t HandlerRequest, const int &port = default_port)
            : _HandlerRequest(HandlerRequest), _port(port), _revs_num(gnum)
        {
            // 0. 申请必要的空间
            _revs = new struct epoll_event[_revs_num];

            // 1. 创建套接字
            _listensock = Sock::Socket();
            Sock::Bind(_listensock, _port);
            Sock::Listen(_listensock);

            // 2. 创建 epoll 模型
            _epfd = Epoll::CreateEpoll();

            logMessage(DEBUG, "Init success, listensock : %d , epfd : %d ... ", _listensock, _epfd);

            // 3. 将 _listensock 加入到 epoll 让其管理！
            if (Epoll::CtlEpoll(_epfd, EPOLL_CTL_ADD, _listensock, EPOLLIN))
            {
                logMessage(DEBUG, "Add listensock to Epoll success, listensock ...");
            }
            else
            {
                exit(6);
            }
        }

        // 单次处理事件
        void LoopOnce(int timeout)
        {
            // 注意点：
            // 细节一：如果底层一次就绪的 sock 非常多！revs 装不下怎么办？
            // 答：不影响！一次获取不玩，下次再获取！
            // 细节二：关于epoll_wait返回值含义是有几个就绪，就返回几！
            //        特点：epoll 返回值，会将就绪的event按顺序防止在我们的数组中！好比入栈！
            int n = Epoll::WaitEpoll(_epfd, _revs, _revs_num, timeout);
            switch (n)
            {
            case 0:
                logMessage(DEBUG, "time out ...");
                break;
            case -1:
                logMessage(WARNING, "epoll wiat error : %s ", strerror(errno));
                break;
            default:
                // 此处一定成功了！
                logMessage(DEBUG, "Get a new event ...");
                HandlerEvent(n);
                break;
            }
        }

        void HandlerEvent(int n)
        {
            assert(n > 0);
            for (int i = 0; i < n; i++)
            {
                uint32_t revents = _revs[i].events; // 获取到就绪的事件动作
                int sock = _revs[i].data.fd;

                if (revents & EPOLLIN)
                {
                    // 1. 读事件就绪！
                    // _listensock（accept） or other（read）
                    if (sock == _listensock)
                        Accepter(sock);
                    else
                        Recver(sock);
                }
            }
        }

        void Accepter(int listensock)
        {
            std::string client_ip;
            uint16_t client_port;

            int sock = Sock::Accept(listensock, &client_ip, &client_port);
            if (sock < 0)
            {
                logMessage(WARNING, "Accept error ...");
                return;
            }
            // 此处不能直接accept 之后直接读取！
            // 原因在于：你不知道是否有数据就绪，没有数据读取会阻塞！
            // 将新获取的 sock 加入到 epoll 让其管理！
            if (!Epoll::CtlEpoll(_epfd, EPOLL_CTL_ADD, sock, EPOLLIN))
                return;
            logMessage(DEBUG, "Add a scok[%d] to Epoll success ...", sock);
        }

        void Recver(int sock)
        {
            // 1. 读取数据
            char buffer[1024];
            ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (n > 0)
            {
                // 问题：如何保证数据读取的完整性？
                buffer[n] = 0;
                _HandlerRequest(buffer);
            }
            else if (n == 0)
            {
                logMessage(NORMAL, "client %d quit, me too ...", sock);
                // 1. 先在epoll中去掉sock的关心，epoll 中的文件描述符必须是合法的！
                bool res = Epoll::CtlEpoll(_epfd, EPOLL_CTL_DEL, sock, 0);
                assert(res);
                (void)res;
                // 2. 再关闭！
                close(sock);
            }
            else
            {
                // 1. 先在epoll中去掉sock的关心，epoll 中的文件描述符必须是合法的！
                bool res = Epoll::CtlEpoll(_epfd, EPOLL_CTL_DEL, sock, 0);
                assert(res);
                (void)res;
                // 2. 再关闭！
                close(sock);
            }
            // 2. 处理数据（粘包问题等）
        }

        void Start()
        {
            int timeout = 1000;
            while (true)
            {
                LoopOnce(timeout);
            }
        }
        ~EpollServer()
        {
            if (_listensock >= 0)
                close(_listensock);
            if (_epfd >= 0)
                close(_epfd);
            if (_revs)
                delete _revs;
        }

    private:
        int _listensock;
        int _epfd;
        uint16_t _port;
        struct epoll_event *_revs;
        int _revs_num;
        func_t _HandlerRequest;
    };

}

#endif