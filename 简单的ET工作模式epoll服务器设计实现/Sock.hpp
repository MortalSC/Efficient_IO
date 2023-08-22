#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include "log.hpp"
#include <fcntl.h>

/* 提供套接字接口（封装） */

class Sock
{
private:
    const static int gbacklog = 20;

public:
    // 构造函数
    Sock() {}

    // 创建套接字
    static int Socket()
    {
        int listensock = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM：面向字节流
        if (listensock < 0)
        {
            logMessage(FATAL, "create socket error, %d:%s", errno, strerror(errno));
            exit(2);
        }
        logMessage(NORMAL, "create socket success, listensock: %d", listensock);
        return listensock;
    }
    // 创建绑定：使指定文件描述符与指定 ip / port 进行绑定
    static void Bind(int sock, uint16_t port, std::string ip = "0.0.0.0")
    {
        // 创建：sockaddr结构对象
        struct sockaddr_in local;
        memset(&local, 0, sizeof local);                 // 内容初始化为空
        local.sin_family = AF_INET;                      // 指定：16位地址类型！
        local.sin_port = htons(port);                    // 指定：16位端口号
        inet_pton(AF_INET, ip.c_str(), &local.sin_addr); // 指定：32位ip地址
        if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0)
        {
            logMessage(FATAL, "bind error, %d:%s", errno, strerror(errno));
            exit(3);
        }
    }

    // 监听请求！
    static void Listen(int sock)
    {
        //
        if (listen(sock, gbacklog) < 0)
        {
            logMessage(FATAL, "listen error, %d:%s", errno, strerror(errno));
            exit(4);
        }

        logMessage(NORMAL, "init server success");
    }
    // 一般经验
    // const std::string &: 输入型参数
    // std::string *: 输出型参数
    // std::string &: 输入输出型参数
    // 【新增】_accept_errno：用于区分：非阻塞下 accept 失败的原因：没有连接 / 直接读取出错
    static int Accept(int listensock, std::string *ip, uint16_t *port, int *_accept_errno)
    {
        // ET 模式下，非阻塞式处理
        // 获取失败：如何处理？
        // 非阻塞下 accept 失败的原因：没有连接 / 直接读取出错
        // 如何区分两种情况？
        // 获取错误码！
        struct sockaddr_in src;
        socklen_t len = sizeof(src);
        *_accept_errno = 0;
        int servicesock = accept(listensock, (struct sockaddr *)&src, &len);
        if (servicesock < 0)
        {
            // logMessage(ERROR, "accept error, %d:%s", errno, strerror(errno));
            *_accept_errno = errno;
            return -1;
        }
        if (port)
            *port = ntohs(src.sin_port);
        if (ip)
            *ip = inet_ntoa(src.sin_addr);
        return servicesock;
    }

    // 连接
    static bool Connect(int sock, const std::string &server_ip, const uint16_t &server_port)
    {
        struct sockaddr_in server;
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_port = htons(server_port);
        server.sin_addr.s_addr = inet_addr(server_ip.c_str());

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == 0)
            return true;
        else
            return false;
    }

    // 设置 sock 为非阻塞方式
    static bool SetNonBlock(int sock)
    {
        int fl = fcntl(sock, F_GETFL);
        if (fl < 0)
            return false;
        fcntl(sock, F_SETFL, fl | O_NONBLOCK);
        return true;
    }

    ~Sock() {}
};
