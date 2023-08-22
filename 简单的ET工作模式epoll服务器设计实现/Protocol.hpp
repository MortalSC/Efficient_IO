#pragma once

#include <iostream>
#include <cstring>
#include <string>
#include <vector>

// 自定义协议：网络计算机
// 1. 报文之间，采用特殊字符来进行解决 => 粘包问题
// 2. 获取一个独立完整的报文 —— 序列化和反序列化（自定义）
// 示例：100+19X100+20

// 支持解决粘报问题，处理独立报文
#define SEP "X"
#define SEP_LEN strlen(SEP)

// 自己手写序列反序列化
#define SPACE " "
#define SPACE_LEN strlen(SPACE)

// 我们要把传入进来的缓冲区进行切分
// 1. buffer被切走的，也同时要从buffer中移除
// 2. 可能会存在多个报文，多个报文依次放入out
// buffer: 输入输出型参数
// out: 输出型参数
void SpliteMessage(std::string &buffer, std::vector<std::string> *out)
{
    // 100+
    // 100+19X1
    // 100+19X100+19
    while(true)
    {
        auto pos = buffer.find(SEP);
        if(std::string::npos == pos) break;
        std::string message = buffer.substr(0, pos);
        buffer.erase(0, pos + SEP_LEN);
        out->push_back(message);
        // std::cout << "debug: " << message << " : " << buffer << std::endl;
        // sleep(1);
    }
}
    //TODO
    // 
    std::string Encode(std::string &s)
    {
        return s + SEP;
    }
    
    class Request
    {
    public:
        std::string Serialize()
        {
            std::string str;
            str = std::to_string(x_);
            str += SPACE;
            str += op_; // TODO
            str += SPACE;
            str += std::to_string(y_);
            return str;
        }
        bool Deserialized(const std::string &str) // 1 + 1
        {
            std::size_t left = str.find(SPACE);
            if (left == std::string::npos)
                return false;
            std::size_t right = str.rfind(SPACE);
            if (right == std::string::npos)
                return false;
            x_ = atoi(str.substr(0, left).c_str());
            y_ = atoi(str.substr(right + SPACE_LEN).c_str());
            if (left + SPACE_LEN > str.size())
                return false;
            else
                op_ = str[left + SPACE_LEN];
            return true;
        }

    public:
        Request()
        {
        }
        Request(int x, int y, char op) : x_(x), y_(y), op_(op)
        {
        }
        ~Request() {}

    public:
        int x_;   // 是什么？
        int y_;   // 是什么？
        char op_; // '+' '-' '*' '/' '%'
    };

    class Response
    {
    public:
        // "code_ result_"
        std::string Serialize()
        {

            std::string s;
            s = std::to_string(code_);
            s += SPACE;
            s += std::to_string(result_);

            return s;
        }
        // "111 100"
        bool Deserialized(const std::string &s)
        {
            std::size_t pos = s.find(SPACE);
            if (pos == std::string::npos)
                return false;
            code_ = atoi(s.substr(0, pos).c_str());
            result_ = atoi(s.substr(pos + SPACE_LEN).c_str());
            return true;
        }
    public:
        Response()
        {
        }
        Response(int result, int code) : result_(result), code_(code)
        {
        }
        ~Response() {}

    public:
        // 约定！
        // result_? code_? code_ 0? 1?2?3?
        int result_; // 计算结果
        int code_;   // 计算结果的状态码
    };








// #include <iostream>
// #include <cstring>
// #include <string>
// #include <vector>

// #define SEP "X" // 设置报文的分隔符为："X"
// #define SEP_LEN strlen(SEP)

// // buffer：被解析的报文（输入缓冲区内容）【输入输出型参数】
// // out：解析出来的单个有效可处理的计算报文【输出型参数】
// // 注：
// // 1. buffer 中的数据被取出，输入缓冲区中的数据要减少！
// // 2. 可能存在多个可处理的数据内容！多个可处理的数据内容依次放入 out
// void SplitMessage(std::string &buffer, std::vector<std::string> *out)
// {
//     // 可能的情形：
//     // 100+19
//     // 100+19X100+20
//     // 100+
//     // 100+19X100
//     // ...
//     while (true)
//     {
//         auto pos = buffer.find(SEP);
//         // 获取到的缓冲区中没有完整的单个数据报分隔符
//         if(pos == std::string::npos) break;

//         // 获取了数据报分隔符前的数据
//         std::string message = buffer.substr(0, pos);
        
//         // 从缓冲区中移除被提取的数据
//         buffer.erase(0,pos+SEP_LEN);          // 移除读取的数据
//         // buffer.erase();      // 移除分隔符

//         out->push_back(message);

//     }
// }