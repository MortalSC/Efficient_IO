#include "epollServer.hpp"

#include <memory>

void change(std::string reqStr){
    // 只要该函数被调用，说明下层数据是有的
    // 完成业务逻辑
    std::cout << "测试 > " << reqStr << std::endl;
}


int main(){

    std::unique_ptr<ns_Epoll::EpollServer> epoll_ptrsvr(new ns_Epoll::EpollServer(change));
    epoll_ptrsvr->Start();

    return 0;
}