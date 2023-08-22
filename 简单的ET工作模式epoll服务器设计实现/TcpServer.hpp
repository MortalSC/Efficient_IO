#pragma once

#include <iostream>
#include <string>
#include <functional>
#include "Sock.hpp"
#include "log.hpp"
#include "Epoll.hpp"
#include <unordered_map>
#include "Protocol.hpp"
#include <vector>




class TcpServer;
class Connection;

using func_t = std::function<void(Connection *)>;                           // 回调方法：获取数据
using callBack_t = std::function<void(Connection *, std::string &request)>; // 回调业务方法！

// 为了能够正常的工作！常规的 sock 必须有自己独立的：接收缓冲区（读取完整报文）和发送缓冲区
class Connection
{
public:
    Connection(int sock = -1)
        : _sock(sock), _tcptr(nullptr)
    {
    }
    // 设置对连接的回调方法
    void SetCallBack(func_t recv_cb, func_t send_cb, func_t except_cb)
    {
        _recv_cb = recv_cb;
        _send_cb = send_cb;
        _except_cb = except_cb;
    }
    ~Connection() {}

public:
    // 负责进行 IO 的文件描述符
    int _sock;

    // 三个回调方法，表征的就是对 _sock 进行的特定读写对应的方法！
    // 读取回调
    func_t _recv_cb;
    // 写入回调
    func_t _send_cb;
    // 异常回调
    func_t _except_cb;

    // 接收缓冲区 && 发送缓冲区
    // 【问题：暂时没办法处理二进制流！即：只能处理文本数据，图片音频数据无法处理！】
    std::string _inbuffer;
    std::string _outbuffer;

    // 设置对 TcpServer 的回值指针【指向未来定义的 server】
    TcpServer *_tcptr;
};

class TcpServer
{
    const static int default_prot = 8080;
    const static int default_num = 128;

public:
    TcpServer(int port = default_prot)
        : _port(port), _revs_num(default_num)
    {
        // 1. 套接字基本操作
        _listensock = Sock::Socket();
        Sock::Bind(_listensock, _port);
        Sock::Listen(_listensock);

        // 2. 创建多路转接对象
        _poll.CreateEpoll();

        logMessage(DEBUG, "Init success, listensock : %d ... ", _listensock);

        // 3. 添加 listensock 到 服务器中【listensock 只关心读取（是否有连接就绪）】
        AddConnection(_listensock, std::bind(&TcpServer::Accepter, this, std::placeholders::_1), nullptr, nullptr);

        // 4. 构建获取就绪事件的缓冲区！
        _revs = new struct epoll_event[_revs_num];
    }

    // 专门设计一个接口把任意sock添加到TcpServer中
    void AddConnection(int sock, func_t recv_cb, func_t send_cb, func_t except_cb)
    {
        // 设置 sock 为非阻塞方式！
        Sock::SetNonBlock(sock);

        // 注意：除了 _listensock，未来还有一大批 socket，设计每一个套接字都有 connection
        //      当服务器中存在大量的 connection 时， TcpServer 必须对它们进行管理！

        // 1. 构建 connection 对象，封装 sock！
        Connection *con = new Connection(sock);
        con->SetCallBack(recv_cb, send_cb, except_cb);
        con->_tcptr = this;

        // sock[] 托管给 epoll 模型 + 对应的 connection 映射到 conections 映射表中
        // 实现解耦：
        // a. epoll 中的 sock 完成资源的监测就绪！
        // b. connection 完成上层的业务处理，读写等...

        // 2. 添加 sock 到 epoll 中
        // EPOLLET：即设定要求为ET工作模式！（即：要求单次读取完就绪的资源内容！）

        // 【前人经验】任何多路转接服务器，一般只会打开对读取事件的关心，写入事件会根据需求才打开

        _poll.AddSockToEpoll(sock, EPOLLIN | EPOLLET);
        logMessage(DEBUG, "Add listensock to Epoll success, listensock ...");

        // 3. 还要将 对应的 connection 对象指针添加到 conections 映射表中
        _connections.insert(std::make_pair(sock, con));
    }

    // 读取事件操作
    void Accepter(Connection *con)
    {
        // logMessage(DEBUG, "Accepter() 被调用 ...");
// 到此为止，一定是 listensock 就绪了！此处会阻塞吗？   => 不会！

// version 1：单次读取数据！（能保证一次读取完下层就绪的所有数据吗？）【且不关心（存储）对端的 ip / port】
#if 0
        std::string client_ip;
        uint16_t client_port;
        int sock = Sock::Accept(con->_sock, &client_ip, &client_port);
        // 该 sock 是后续进行读写的 （IO）sock
        if (sock < 0)
        {
            // 获取失败：如何处理？
            // 非阻塞下 accept 失败的原因：没有连接 / 直接读取出错
            // 如何区分两种情况？
            // 获取错误码！
        }

        // 获取成功！说明连接就绪！但是不代表数据就绪！（即：不能直接调用 read / recv）
        // 解决方式：托管给 TcpServer（epoll）
        // 过程：
        // a. 将套接字托管给epoll
        // b. 让 TcpServer 构建处对应的 connection
        // c. 将 connection 添加到 _connections 容器中
        AddConnection(sock,
                      std::bind(&TcpServer::Recver, this, std::placeholders::_1),
                      std::bind(&TcpServer::Sender, this, std::placeholders::_1),
                      std::bind(&TcpServer::Excepter, this, std::placeholders::_1));
#endif

        // version 2：循环读取数据！（能保证一次读取完下层就绪的所有数据吗？）【且不关心（存储）对端的 ip / port】
        while (true)
        {
            std::string client_ip;
            uint16_t client_port;
            int accept_errno = 0;
            // 【新增】_accept_errno：用于区分：非阻塞下 accept 失败的原因：没有连接 / 直接读取出错
            int sock = Sock::Accept(con->_sock, &client_ip, &client_port, &accept_errno);
            // 该 sock 是后续进行读写的 （IO）sock
            if (sock < 0)
            {
                // 获取失败：如何处理？
                // 非阻塞下 accept 失败的原因：没有连接 / 直接读取出错
                // 如何区分两种情况？
                // 获取错误码！

                // 情形一：没有链接
                if (accept_errno == EAGAIN || accept_errno == EWOULDBLOCK)
                    break;
                // 可能遇到中断【概率极低】
                else if (accept_errno == EINTR)
                    continue;
                else
                {
                    // 直接读取出错
                    logMessage(WARNING, "accept error => %d : %s", accept_errno, strerror(accept_errno));
                    break;
                }
            }

            // 获取成功！说明连接就绪！但是不代表数据就绪！（即：不能直接调用 read / recv）
            // 解决方式：托管给 TcpServer（epoll）
            // 过程：
            // a. 将套接字托管给epoll
            // b. 让 TcpServer 构建处对应的 connection
            // c. 将 connection 添加到 _connections 容器中
            if (sock >= 0)
            {

                AddConnection(sock,
                              std::bind(&TcpServer::Recver, this, std::placeholders::_1),
                              std::bind(&TcpServer::Sender, this, std::placeholders::_1),
                              std::bind(&TcpServer::Excepter, this, std::placeholders::_1));
                logMessage(DEBUG, "accept clinet ip / port / sock : [%s:%d:%d] ; add to epoll && TcpServer success...",
                           client_ip.c_str(), client_port, sock);
            }
        }
    }

    // IO sock 中读取方法的封装
    void Recver(Connection *con)
    {
        // data of internet => inbuffer【把 网络中 的数据读取到 接收缓冲区】
        logMessage(DEBUG, "Recver event exsits, Recver() been called ...");

        // 下层数据量未知！需要结合协议来读取全部数据！
        // version 1：直接面向字节流，先进行常规读取！
        bool err = false;
        while (true)
        {
            char buffer[1024];
            ssize_t n = recv(con->_sock, buffer, sizeof(buffer) - 1, 0);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                {
                    // 读取失败！
                    logMessage(ERROR, "recv error, %d : %s", errno, strerror(errno));
                    // 出现异常，交给异常处理函数
                    err = true;
                    con->_except_cb(con);
                    break;
                }
            }
            else if (n == 0)
            {
                // 对端关闭了连接
                logMessage(ERROR, "client[%d] quit, server close[%d]", con->_sock, con->_sock);
                // 出现异常，交给异常处理函数
                err = true;
                con->_except_cb(con);
                break;
            }
            else
            {
                // 读取成功
                buffer[n] = 0;
                // 数据载入接收缓冲区
                con->_inbuffer += buffer;
            }
        }
        logMessage(DEBUG, "con->_inbuffer [sock : %d]: %s ", con->_sock, con->_inbuffer.c_str());

        if (!err)
        {
            std::vector<std::string> message; // 可能是空！
            SpliteMessage(con->_inbuffer, &message);
            for (auto &msg : message)
            {
                // 到此一定是一个完整报文
                // 理论上，从功能角度出发（网络计算器），我们拿到了报文应该要进行序列化 / 反序列化
                // 进行计算等操作！
                // 但是！这都是业务功能！坚决必要再TcpServer提供这些业务功能方法！
                _callback(con, msg);
                // 此处在这里也可以将message封装成 task，然后push到任务队列，任务处理交给后端线程
            }
        }
    }

    // 实现控制读写事件是否关心的设定！
    /*
        readable：关心可读！
        writeable：关心可写！
    */
    void EnableReadWrite(Connection *con, bool readable, bool writeable)
    {
        uint32_t events = ((readable ? EPOLLIN : 0) | (writeable ? EPOLLOUT : 0));
        bool res = _poll.CtlEpoll(con->_sock, events);
        assert(res); // 更改成if
    }

    // IO sock 中写入方法的封装
    void Sender(Connection *con)
    {
        // data of outbuffer => internet【把 发送缓冲区 中的数据上传到 网络】
        while (true)
        {
            ssize_t n = send(con->_sock, con->_outbuffer.c_str(), con->_outbuffer.size(), 0);
            if (n > 0)
            {
                // 发送成功
                con->_outbuffer.erase(0, n); // 移除已发送的数据
                if (con->_outbuffer.empty())
                    break; // 发送缓冲区为空了！
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                {
                    // 发送错误！
                    logMessage(ERROR, "send error, %d : %s", errno, strerror(errno));
                    con->_except_cb(con);
                    break;
                }
            }
        }

        // 发完了吗？不确定！但是我能保证一定没有出错！要么发完了，要么不满足发送条件（下次发送）
        if (con->_outbuffer.empty())
        {
            // 状态：关心读，不用写
            EnableReadWrite(con, true, false);
        }
        else
        {
            // 下次再发
            EnableReadWrite(con, true, true);
        }
    }

    // IO sock 中读写等异常状况的事件处理封装
    void Excepter(Connection *con)
    {
        // 1. 判断连接是否存在
        if(IsConnectionExists(con->_sock)) return;
        // 2. 从 epoll 中移除
        bool res = _poll.DelFromEpoll(con->_sock);
        assert(res);
        // 3. 从 unordered_map 中移除【key值移除】
        _connections.erase(con->_sock); 

        // 4. 关闭 close
        close(con->_sock);

        // 5. delete con
        delete con;

        logMessage(DEBUG, "Excepter 回收完毕！所有异常情况！");
        
    }

    void LoopOnce()
    {
        // 获取下层已经就绪的资源
        int n = _poll.WaitEpoll(_revs, _revs_num);
        if (n == 0)
        {
            logMessage(DEBUG, "time out ...");
            return;
        }

        // logMessage(DEBUG, "===========================");

        // logMessage(DEBUG, "WaitEpoll success, n : %d ... ", n);

        for (int i = 0; i < n; i++)
        {
            // 获取已就绪的 sock 和 操作事件
            int sock = _revs[i].data.fd;
            uint32_t revents = _revs[i].events;

            // 将所有的异常，全部交给 read / write 来处理
            // EPOLLERR：表示对应的文件描述符发生错误;
            if(revents & EPOLLERR) revents |= (EPOLLIN | EPOLLOUT);
            // EPOLLHUP：示对应的文件描述符被挂断;
            if(revents & EPOLLHUP) revents |= (EPOLLIN | EPOLLOUT);


            // 判断是否为读取操作！
            if (revents & EPOLLIN)
            {
                // 判断 sock 的合法性 + 对应的回调函数是否存在
                if (IsConnectionExists(sock) && _connections[sock]->_recv_cb != nullptr)
                {
                    // 调用对应的 读 回调方法！
                    _connections[sock]->_recv_cb(_connections[sock]);
                }
            }

            // 判断是否为写入操作！
            if (revents & EPOLLOUT)
            {
                // 判断 sock 的合法性 + 对应的回调函数是否存在
                if (IsConnectionExists(sock) && _connections[sock]->_send_cb != nullptr)
                {
                    // 调用对应的 读 回调方法！
                    _connections[sock]->_send_cb(_connections[sock]);
                }
            }
        }
    }

    bool IsConnectionExists(int sock)
    {
        auto iter = _connections.find(sock);
        if (iter == _connections.end())
            return false;
        else
            return true;
    }

    // 根据就绪的事件，进行特定的派发任务
    void Dispahter(callBack_t callback)
    {
        _callback = callback;
        while (true)
        {
            // 获取下层已经就绪的资源
            LoopOnce();
        }
    }

    void Start()
    {
    }

    ~TcpServer()
    {
        if (_listensock >= 0)
            close(_listensock);
        if (_revs)
            delete[] _revs;
    }

private:
    int _listensock;
    int _port;

    Epoll _poll;
    // 通过 sock 映射到对应的 connection
    std::unordered_map<int, Connection *> _connections;

    // 获取下层就绪资源的载体！
    struct epoll_event *_revs;
    int _revs_num;

    callBack_t _callback;
};
