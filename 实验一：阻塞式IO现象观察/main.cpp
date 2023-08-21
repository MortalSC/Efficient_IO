#include <iostream>
#include <unistd.h>

int main(){

    char buffer[1024];          // 用作读取的数据缓存
    int cnt = 0;                // 用作显示读取次数！
    while(true){
        // 指定在标准输入中读取
        ssize_t s = read(0, buffer, sizeof(buffer)-1);
        if(s>0){
            // 读取成功输入信息
            std::cout << "第" << ++cnt << "次读取：" << buffer << std::endl;
        }
    }

    return 0;
}