# Assignment 2: Implement `receiver`

**在开始完成该任务之前，请务必阅读完成本文档**

## 说明

`receiver`只需要考虑只存在一个`sender`的情况

`receiver`需要计算CRC32的checksum，对于checksum不对的包应当直接丢弃

对于每一个被确认接收的包，你需要发送一个`ACK`报文，该报文的`seq_num`为当前期望收到的下一个包的`seq_num`，举例而言有如下两种情况(假设期望收到的下一个报文`seq_num==N`):

1. 如果当前报文`seq_num!=N`: 将该报文缓存下来，并发送一个`seq_num=N`的`ACK`
2. 如果当前报文`seq_num==N`: 将该报文缓存下来，并发送一个`seq_num=M`的`ACK`这里`M`为`seq_num`最小的还没有被缓存的报文

对于那些`seq_num > N + window_size`的报文会被直接丢弃

## 实验要求

在你的程序中，你需要实现如下三个函数

``` cpp
/**
 * @brief 开启receiver并在所有IP的port端口监听等待连接
 * 
 * @param port receiver监听的port
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 */
int initReceiver(uint16_t port, uint32_t window_size);

/**
 * @brief 接收数据
 * 
 * @param buf buffer
 * @param len buffer能够接收的数据量上限(bytes)
 * @return 0表示sender断开连接，并且已经接收所有数据 >0表示接收到数据 -1表示出现其他错误
 */
size_t recvMessage(char* buf, size_t len);

/**
 * @brief 关闭Receiver
 */
void terminateReceiver();
```

> 为了简化任务，对于接收方而言，我们可以认为一方在发送数据的时候另一方必然正在接收数据，因此不用考虑使用多线程技术在另一个线程中执行接收数据

> 实现这三个函数的目的是为了方便进行测试，测试程序会直接调用这三个函数，因此请注意不要使用错误的函数定义。为了能够单独执行你也可以使用这三个函数实现一个自己的`receiver`的可执行程序，这并不复杂

> 请不要在函数内部直接使用`exit`等导致**进程**退出的指令，这会导致评测程序也直接停止执行

## 编译&测试

**要求**

1. 注意在实现的过程中包含这三个函数的定义已经放到了`receiver_def.h`文件中，在实现这些函数的文件中请务必`#include`该文件
2. 请使用`add_library(rtpreceiver .....)`将库打包成一个library，相关的代码已经写在CMakeLists.txt中
    > 你自行实现的其他可执行程序可以通过`target_link_libraries`链接该库，并调用技术规范中的三个函数实现RTP协议的传输

**编译**

执行如下命令即可编译，具体每一句命令行的含义请参考lv.0

``` bash
mkdir build
cd build
cmake .. -G "Unix Makefile"
make -j
```

**测试**

如需执行receiver端的测试，请在编译目录中执行程序`./rtp_test_all --gtest_filter="RTPReceiver.*"`，测试的过程中会关闭STDOUT但是你可以从STDERR输出调试信息

|测试点名|测试点内容|分数占比|
|---|---|---|
|RTPReceiver.basic|基本传输不存在丢包|10分|
|RTPReceiver.conn_packet_loss|Connect时ACK报文丢失|5分|
|RTPReceiver.packet_loss|丢弃某个数据报文|5分|
|RTPReceiver.duplicate_data|某个数据报文传输两次|5分|
|RTPReceiver.checksum|某个报文checksum错误|5分|
|RTPReceiver.mixed|混合出现多次任意上述情况|30分|

**声明**

最后，如有任何疑问可以联系助教
