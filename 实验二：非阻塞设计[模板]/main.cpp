#include <iostream>
#include <cstring> // 用于错误信息的转义输出
#include <unistd.h>
#include <fcntl.h>

// 非阻塞控制函数！
// 参数：对指定的目标文件描述符进行设置
void NoSetBlock(int fd)
{
    int fl = fcntl(fd, F_GETFL); // 获取文件状态标志信息
    if (fl < 0)
    {
        return;
    }
    fcntl(fd, F_SETFL, fl | O_NONBLOCK); // 让原始的文件状态标志或上新的，就等于新增一个状态
    // 该函数功能等价于
    // read(0, O_RDONLY | O_NONBLOCK);
}

int main()
{
    // 设定非阻塞方式
    NoSetBlock(0);

    char buffer[1024];
    while (true)
    {
        sleep(1);

        // 指定从标准输入中读取！
        ssize_t s = read(0, buffer, sizeof(buffer) - 1);
        if (s > 0)
        {
            buffer[s - 1] = 0;
            // 成功读取到内容！
            std::cout << "输入的内容为：" << buffer << std::endl;
        }
        else
        {
            // 未成功读取到内容！
            // 给出提示
            // 读取不成功时，必然会有信息码 errno【成功：0；不成功：多种情形！】
            // std::cout << "read nullptr? "
            //           << "errno：" << errno << " strerror：" << strerror(errno) << std::endl;

            // 由初步实验结果可以推断出：
            // 非阻塞未读取到数据就是：11 号信息码（errno）
            // 而 11 号：实际对应的就是：EWOULDBLOCK！
            // EAGAIN：即再次尝试
            // 如果失败的errno值是11，就代表其实没错，只不过是底层数据没就绪
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::cout << "当前0号fd数据没有就绪, 请下一次再来试试吧" << std::endl;
                continue;
            }
            else if (errno == EINTR)      
            {
                std::cout << "当前IO可能被信号中断，在试一试吧" << std::endl;
                continue;
            }
            else
            {
                // 进行差错处理
            }
        }
    }

    return 0;
}