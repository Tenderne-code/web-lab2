# 实现Reliable UDP协议

下图是一张网络协议的五层结构图:

XXX

在上一个小节中，你实现了一个应用层协议

该节Lab要求你实现一个介于传输层和应用层之间的层

在该层中实现一个RTP协议，该协议基于UDP实现，提供了可靠性，但是协议内容相较于TCP更加简单，支持一个`sender`向一个`receiver`发送数据

为了避免去年Lab出现有同学无从下手，不知道哪些工具更好用的情况，我们设计了Lv0中的CMake环节

同样的，在下发的文件中我们也提供了CMake的一个简单的模板

## 协议简介

RTP协议包含以下的特性:

1. In order: 包按顺序送达
2. Reliable delivery: 可靠传输

## 技术规范

### Packet header

每一个RTP报文包含一个RTP头

头的定义如下

``` cpp
typedef struct RTP_Header {
    uint8_t type;
    uint16_t length;
    uint32_t seq_num;
    uint32_t checksum;
} rtp_header_t;
```

**type**: 标识了一个RTP报文的类型，0:`START`, 1:`END`, 2:`DATA`, 3:`ACK`

**length**: 标识了一个RTP报文**数据段**的长度，对于`START`,`END`,`ACK`类型的报文来说，长度为0

**seq_num**: 序列号，用于识别顺序做按序送达

**checksum**: 32-bit CRC

### States

**建立连接** `sender`首先发送一个type为`START`的，并且`seq_num`为随机值的报文，此后等待该报文的`ACK`，`ACK`到达后即建立连接

**数据传输** 在完成连接的建立之后，要发送的数据由`DATA`类型的报文进行传输。发送方的数据报文的`seq_num`从0开始每个报文递增

**终止连接** 为了保证所有数据都传输完成，发送方的`END`报文的`seq_num`应当和下一个数据报文的`seq_num`相同，在接收到该报文的`ACK`后即断开连接

### Others

**报文大小** 在用户期望传输一段数据的时候，我们并不能直接将这段数据进行传输，一般在网络中IP报文的总长度不会超过1500(否则会被自动分段)，IP头长度一般为20bytes，UDP头长度为8bytes，因此你需要保证你的RTP报文总长度不会超过1472bytes(RTP头+数据)
