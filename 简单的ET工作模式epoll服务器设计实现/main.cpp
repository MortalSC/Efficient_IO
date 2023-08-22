#include "TcpServer.hpp"
#include <memory>



static Response calculator(const Request &req)
{
    Response resp(0, 0);
    switch (req.op_)
    {
    case '+':
        resp.result_ = req.x_ + req.y_;
        break;
    case '-':
        resp.result_ = req.x_ - req.y_;
        break;
    case '*':
        resp.result_ = req.x_ * req.y_;
        break;
    case '/':
        if (0 == req.y_)
            resp.code_ = 1;
        else
            resp.result_ = req.x_ / req.y_;
        break;
    case '%':
        if (0 == req.y_)
            resp.code_ = 2;
        else
            resp.result_ = req.x_ % req.y_;
        break;
    default:
        resp.code_ = 3;
        break;
    }
    return resp;
}

void NetCal(Connection *con, std::string &request)
{
    logMessage(DEBUG, "NetCal been called, get request: %s", request.c_str());
    // 1. 反序列化，1 + 1                   2 + 3
    Request req;
    if(!req.Deserialized(request)) return;

    // 2. 业务处理
    Response resp = calculator(req);

    // 3. 序列化,构建应答
    std::string sendstr = resp.Serialize();
    sendstr = Encode(sendstr);
    // 4. 交给服务器con
    con->_outbuffer += sendstr;
    // 5. 想办法，让底层的TcpServer，让它开始发送
    // a. 需要有完整的发送逻辑
    // b. 我们触发发送的动作，一旦我们开启EPOLLOUT，epoll会自动立马触发一次发送事件就绪，如果后续保持发送的开启，epoll会一直发送
    con->_tcptr->EnableReadWrite(con, true, true);
}

int main(){

    std::unique_ptr<TcpServer> svr(new TcpServer());

    svr->Dispahter(NetCal);


    return 0;
}