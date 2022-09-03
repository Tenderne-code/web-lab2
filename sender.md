# Assignment 1: Implement `sender`

**在开始完成该任务之前，请务必阅读完成本文档**

## 说明

`sender`需要读出数据，并将数据通过RTP协议发送给`receiver`

在`src/util.c`中我们提供了`32-bit CRC`的计算程序

你将会使用滑动窗口的技术来实现可靠传输，`window_size`将会作为一个你程序的参数输入，该参数保证了当前正在传输，并且没有被`receiver`确认的报文数量没有超过`window_size`

`sender`要考虑在如下几种网络情况下的可靠传输:

* 在任意一层发生的丢包
* ACK报文的乱序到达
* 任意数量的任意报文多次接收
* 数据包损坏

每一个报文会被一个ACK报文所确认，为了处理报文丢失或者ACK报文丢失的情况，你需要设置一个计时器。

该计时器在滑动窗口移动和有新的报文发出的时候重置，当该计时器到达500ms的时候，需要将**当前窗口中的所有报文**全部重新发送

## 实现要求

你在你的程序中应当实现以下三个函数

``` cpp
/**
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size);

/**
 * @param message 要发送的数据
 * @param size 要发送的数据大小
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessage(const char* message, ssize_t size);

/**
 * 该函数保证了所有数据包都成功到达才返回
 **/
void terminateSender();
```

> 实现这三个函数的目的是为了方便进行测试，测试程序会直接调用这三个函数，因此请注意不要使用错误的函数定义。为了能够单独执行你也可以使用这三个函数实现一个自己的`sender`的可执行程序，这并不复杂。

> 请不要在函数内部直接使用`exit`等导致**进程**退出的指令，这会导致评测程序也直接停止执行

## 编译&测试

**要求**

1. 注意在实现的过程中包含这三个函数的定义已经放到了`sender_def.h`文件中，在实现这些函数的文件中请务必`#include`该文件
2. 请使用`add_library(rtpsender .....)`将库打包成一个library，相关的代码已经写在CMakeLists.txt中
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

如需执行sender端的测试，请在编译目录中执行程序`./rtp_test_all --gtest_filter="RTPSender.*"`，测试的过程中会关闭STDOUT但是你可以从STDERR输出调试信息

|测试点名|测试点内容|分数占比|
|---|---|---|
|RTPSender.basic|基本传输不存在丢包|10分|
|RTPSender.conn_packet_loss|Connect时ACK报文丢失|5分|
|RTPSender.packet_loss|丢弃某个数据报文|5分|
|RTPSender.ack_out_of_order|收到的ACK乱序|5分|
|RTPSender.duplicate_ack|某个ACK收到两次|5分|
|RTPSender.mixed|混合出现多次任意上述情况|30分|

**声明**

最后，如有任何疑问可以联系助教
