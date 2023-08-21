#ifndef _SELECT_SVR_H_
#define _SELECT_SVR_H_

#include <iostream>
#include <sys/select.h>
#include "Sock.hpp"
#include "log.hpp"
#include <sys/time.h>
#include <string>
#include <vector>

#define BITS 8
#define NUM (sizeof(fd_set) * BITS)
#define FD_NONE -1

// 此处的设计工作仅完成读取的工作！写入和异常在 epoll 完成！
class SelectServer
{
public:
    SelectServer(const uint16_t &port = 8080)
        : _port(port)
    {
        // 1. 创建套接字
        _listensock = Sock::Socket();

        // 2. 绑定
        Sock::Bind(_listensock, _port);

        // 3. 监听
        Sock::Listen(_listensock);

        // 4. 文件描述符（库）的初始值：规定 _rfdsArr[0] = _listensock
        for (int i = 0; i < NUM; i++)
            _rfdsArr[i] = FD_NONE;

        _rfdsArr[0] = _listensock;
        logMessage(DEBUG, "Create base select success.");
    }

    void Start()
    {
        /*
                注意：服务一开始启动的时候，在此我们看见只有一个 套接字 _listensock，
                根据学习的多进程、多线程可以知道，在未来服务运行中是会有多个 sock 的！

                那么此处的 _listensock 该如何看待？
                首先要知道：accept 就是从下层获取连接（已建立了三次握手的连接）！
                这个获取的过程就可以看成一个 input 动作！
                此处的 _listensock 不就是关联了这个 input 事件动作嘛！

                但是，要知道，accept 获取新连接时，如果没有新连接！就会发生阻塞！
                （因此，在不能直接像以往一样上来就调用 accept 获取连接）

                新的操作思路，将本身获取连接的过程也放在 select 中！
                实现的效果：当下层连接就绪了，我们再获取上来！
            */
        // fd_set rfds; // 只作读的实验操作！

        // // 设置文件描述符集的第一步：清空内容！
        // FD_ZERO(&rfds); // 对位图结构进行清空！
        while (true)
        {

            // int sock = Sock::Accept(_listensock, ...);        // 以往的直接写法

            // 指定 select 的时间参数
            struct timeval timeout = {5, 0};

            // 测试打印观察文件描述符添加到”管理库“中
            DebugPrint();

            fd_set rfds; // 只作读的实验操作！
            int maxfd = _listensock;

            // 设置文件描述符集的第一步：清空内容！
            FD_ZERO(&rfds); // 对位图结构进行清空！

            // // 设置文件描述符集的第二步：将指定文件描述符加入到集合中！
            // FD_SET(_listensock, &rfds);

            // 找到合法的文件描述符
            for (int i = 0; i < NUM; i++)
            {
                if (_rfdsArr[i] == FD_NONE)
                    continue; // 非法
                // 到此一定是合法的！
                // 加入到 rfds 中
                FD_SET(_rfdsArr[i], &rfds);
                if (maxfd < _rfdsArr[i])
                    maxfd = _rfdsArr[i]; // 找到文件描述符的最大值！
            }

            // select 操作
            // 此处的 nfds 肯定有问题！随着文件描述符的增多，nfds也在增加，因此 _listensock + 1 肯定是错误的！
            // nfds 肯定是一个变化的值！
            // 同时后面的四个参数都是输入输出型参数！每次使用可能会有不同！因此，则四个参数也是要动态设定的！

            // int n = select(_listensock + 1, &rfds, nullptr, nullptr, nullptr);
            int n = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);

            // int n = select(_listensock + 1, &rfds, nullptr, nullptr, &timeout);

            /*
                注意：若时间设置为：nullptr 表示阻塞式等待！
            */
            switch (n)
            {
            case 0: // 没有就绪资源（后续可以补充其他功能操作）
                logMessage(DEBUG, "time out ...");
                break;
            case -1:
                logMessage(WARNING, "select error ：%d : %s", errno, strerror(errno));
                break;
            default:
                // 到此说明获取到了可用连接！
                logMessage(DEBUG, "Get a new link event ...");
                // 到此的代码现象：一直打印：Get a new link event ...
                // 因为：未使用 _listensock。 rfds 一直有数据！
                // 即：连接已经就绪，你没有取走连接，select 一直提醒！
                // =========================================================================
                // 事件到来处理事件！
                // 获取到就绪资源 select 就会有对输出参数的修改！
                // 此处拿到 rfds，就是看那个资源就绪了，可以进行处理了！
                HandlerEvent(rfds);
                break;
            }
        }
    }

    ~SelectServer()
    {
        if (_listensock >= 0)
            close(_listensock);
    }

private:
    // 目前该版本的最大问题：
    // 【问题】我们的文件描述符管理数组中，既有获取连接的sock，又有 IO 的sock！【解决方式：分离功能】
    // 而且，sock 只会越来越多！
#if 0
    void HandlerEvent(const fd_set &rfds)
    { // fd_set：是一个集合！
        std::string client_ip;
        uint16_t client_port = 0;
        // 文件描述符集操作的第三步：查看指定获取的资源是否就绪！
        // 此处我们只是一个 获取下层的连接，所以直接判断！
        if (FD_ISSET(_listensock, &rfds))
        {
            // 获取连接对象的 ip 和 port
            // 此处是否会引发阻塞？
            // 不会！
            int sock = Sock::Accept(_listensock, &client_ip, &client_port);
            if (sock < 0)
            {
                logMessage(WARNING, "Accept error ...");
                return;
            }
            logMessage(DEBUG, "Get a new link success : [%s : %d], sock : %d ", client_ip.c_str(), client_port, sock);

            // 提问：到此是否能直接拿获取的连接进行操作？
            // 能不能 read / recv ？
            // 不能！
            // 原因：目前并不知道，对方什么时候发送数据过来！
            // 当接收缓冲区为空的时候！read / recv 依旧可能会造成阻塞！
            // 刚刚获取到的 _listensock 只是说明有连接来了！三次握手成功了！并不代表数据过来了！
            // 解决方式：找一个能知道是否有数据来了（就绪）的人”代管“！
            // 说白了，就是依旧使用 select 托管！
            // 数据到来，就是 select 都事件就绪，select 就会通知我！我再执行操作，从而没有阻塞！
            // 到此的问题：如何把 sock 交给 select ？
            // 设置第三方的数组管理sock即可，如：int _rfdsArr[NUM]; // 用于记录文件描述符
            int pos = 1;
            for (; pos < NUM; pos++)
            {
                // 第一步找到一个合法位置（没有被其他文件描述符占用的位置）
                if (_rfdsArr[pos] == FD_NONE)
                    break;
            }
            if (pos == NUM)
            {
                // 数组满了！
                logMessage(WARNING, "%s : %d", "select server already full, close : ", sock);
                close(sock);
            }
            else
            {
                _rfdsArr[pos] = sock;
            }

            
        }
    }
#endif

    // 思路概括：
    // 指定一个 sock 专门处理：获取连接！
    // 其他 sock 处理读事件！
    void HandlerEvent(const fd_set &rfds)
    {
        // 遍历我们的 sock 管理库
        for (int i = 0; i < NUM; i++)
        {
            // 1. 不操作非法的文件描述符
            if (_rfdsArr[i] == FD_NONE)
                continue;
            // 2. 判断合法的 sock 中的就绪情况！
            if (FD_ISSET(_rfdsArr[i], &rfds))
            {
                // 到此处！资源一定就绪了！！！

                // 区分就绪情况：数据读取就绪 or 连接就绪
                if (_rfdsArr[i] == _listensock)
                    Accepter();
                else
                    Recver(i);
            }
        }
    }

    void Accepter()
    {
        std::string client_ip;
        uint16_t client_port = 0;
        // 获取连接对象的 ip 和 port
        // 此处是否会引发阻塞？
        // 不会！
        int sock = Sock::Accept(_listensock, &client_ip, &client_port);
        if (sock < 0)
        {
            logMessage(WARNING, "Accept error ...");
            return;
        }
        logMessage(DEBUG, "Get a new link success : [%s : %d], sock : %d ", client_ip.c_str(), client_port, sock);

        // 提问：到此是否能直接拿获取的连接进行操作？
        // 能不能 read / recv ？
        // 不能！
        // 原因：目前并不知道，对方什么时候发送数据过来！
        // 当接收缓冲区为空的时候！read / recv 依旧可能会造成阻塞！
        // 刚刚获取到的 _listensock 只是说明有连接来了！三次握手成功了！并不代表数据过来了！
        // 解决方式：找一个能知道是否有数据来了（就绪）的人”代管“！
        // 说白了，就是依旧使用 select 托管！
        // 数据到来，就是 select 都事件就绪，select 就会通知我！我再执行操作，从而没有阻塞！
        // 到此的问题：如何把 sock 交给 select ？
        // 设置第三方的数组管理sock即可，如：int _rfdsArr[NUM]; // 用于记录文件描述符
        int pos = 1;
        for (; pos < NUM; pos++)
        {
            // 第一步找到一个合法位置（没有被其他文件描述符占用的位置）
            if (_rfdsArr[pos] == FD_NONE)
                break;
        }
        if (pos == NUM)
        {
            // 数组满了！
            logMessage(WARNING, "%s : %d", "select server already full, close : ", sock);
            close(sock);
        }
        else
        {
            _rfdsArr[pos] = sock;
        }
    }

    void Recver(int pos)
    {
        // 读事件就绪：INPUT事件到来、recv，read
        logMessage(DEBUG, "message in, get IO event: %d", _rfdsArr[pos]);
        // 暂时先不做封装, 此时select已经帮我们进行了事件检测，fd上的数据一定是就绪的，即 本次 不会被阻塞

        // 这样读取有bug吗？有的，你怎么保证以读到了一个完整包文呢？
        
        char buffer[1024];
        int n = recv(_rfdsArr[pos], buffer, sizeof(buffer) - 1, 0);
        if (n > 0)
        {
            buffer[n] = 0;
            logMessage(DEBUG, "client[%d]# %s", _rfdsArr[pos], buffer);
        }
        else if (n == 0)
        {
            logMessage(DEBUG, "client[%d] quit, me too...", _rfdsArr[pos]);
            // 1. 我们也要关闭不需要的fd
            close(_rfdsArr[pos]);
            // 2. 不要让select帮我关心当前的fd了
            _rfdsArr[pos] = FD_NONE;
        }
        else
        {
            logMessage(WARNING, "%d sock recv error, %d : %s", _rfdsArr[pos], errno, strerror(errno));
            // 1. 我们也要关闭不需要的fd
            close(_rfdsArr[pos]);
            // 2. 不要让select帮我关心当前的fd了
            _rfdsArr[pos] = FD_NONE;
        }
    }

    void DebugPrint()
    {
        std::cout << "_fd_array[]: ";
        for (int i = 0; i < NUM; i++)
        {
            if (_rfdsArr[i] == FD_NONE)
                continue;
            std::cout << _rfdsArr[i] << " ";
        }
        std::cout << std::endl;
    }

private:
    // 作为服务器要有端口号
    uint16_t _port;
    int _listensock;

    int _rfdsArr[NUM]; // 用于记录文件描述符：解决：如何把 sock 交给 select ？
};

#endif