# Assignment 3: Implement `opt_sender` and `opt_receiver`

**在开始完成该任务之前，请务必阅读完成本文档**

## 说明

考虑丢包，如果在一个窗口之中只存在一个丢包，在之前的实现中必然发生如下情况

1. Sender等待到整个报文丢失
2. 丢的报文为没有发送的窗口的第一个报文
3. 将整个窗口内的所有报文都重新传输

这种情况浪费了大量带宽，仅仅为了一个丢弃的报文，因此我们可以考虑进行优化

根据如下的规则对代码进行优化

* receiver的ACK将对每一个包发送一个对应seq_num的ACK（而不是之前的每一个ACK表示之前的内容都已经收到）
* 同样的仍然需要维护一个N表示当前还没有ACK的最小的seq_num，对于任意大于`N+window`的报文应当直接丢弃
* sender需要维护当前window中没有被ACK的报文，仅重传那些没有被ACK的报文

同样以之前的情况作为例子会发生如下情况:

1. Sender等待到超时，发现窗口第一个报文丢失
2. 检查当前窗口中所有报文，将所有没有收到ACK的报文重新传输

## 实现要求

你在你的程序中应当实现以下函数，**注意该函数需要兼容之前写的`initSender,initReceiver`以及`terminateSender,terminateReceiver`函数**

``` cpp
/**
 * @param message 要发送的数据
 * @param size 要发送的数据大小
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessageOpt(const char* message, ssize_t size);

/**
 * @brief 接收数据
 * 
 * @param buf buffer
 * @param len buffer能够接收的数据量上限(bytes)
 * @return 0表示sender断开连接，并且已经接收所有数据 >0表示接收到数据 -1表示出现其他错误
 */
size_t recvMessageOpt(char* buf, size_t len);
```

> 实现这两个函数的目的是为了方便进行测试，测试程序会直接调用这两个函数，因此请注意不要使用错误的函数定义。为了能够单独执行你也可以使用这三个函数实现一个自己的`opt_sender,opt_receiver`的可执行程序，这并不复杂。

> 请不要在函数内部直接使用`exit`等导致**进程**退出的指令，这会导致评测程序也直接停止执行

## 编译&测试

**要求**

1. 注意在实现的过程中包含这三个函数的定义已经放到了`sender_def.h,receiver_def.h`文件中，在实现这些函数的文件中请务必`#include`该文件
2. 请使用`add_library(rtpsender .....), add_library(rtpreceiver .....)`将库打包成一个library，相关的代码已经写在CMakeLists.txt中
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

如需执行测试，请在编译目录中执行程序`./rtp_test_all --gtest_filter="RTP*Opt.*"`，测试的过程中会关闭STDOUT但是你可以从STDERR输出调试信息

|测试点名|测试点内容|分数占比|
|---|---|---|
|RTPSenderOpt.basic|正常无丢包环境|5分|
|RTPSenderOpt.mixed|重复出现包含所有在sender中出现的错误|10分|
|RTPReceiverOpt.basic|正常无丢包环境|5分|
|RTPReceiverOpt.mixed|重复出现包含所有在receiver中出现的错误|10分|

**声明**

最后，如有任何疑问可以联系助教
