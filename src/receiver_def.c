#include "sender_def.h"
#include "rtp.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>

#define rtp_size 1472
int sock_rcv;
int win_size_rcv = 0;
struct sockaddr_in local_addr;
struct sockaddr_in recv_addr;
socklen_t addrlen = sizeof(recv_addr);
struct timeval timeout;

//打包ack報文
void pkg_ack(uint32_t seq, uint8_t ty){

    rtp_packet_t rtp_load;
    rtp_load.rtp.type = ty;
    rtp_load.rtp.length = 0;
    rtp_load.rtp.seq_num = seq;
    //printf("receiver: 發送seq爲 :%d\n", rtp_load.rtp.seq_num);
    rtp_load.rtp.checksum = 0;
    memset(rtp_load.payload, 0, sizeof(rtp_load.payload));
    rtp_load.rtp.checksum = compute_checksum(&rtp_load, 11);
    //printf("receiver: ack報文checksum 爲 :%d\n", rtp_load.rtp.checksum);
    
    int rul = sendto(sock_rcv, &rtp_load, 11, 0, (struct sockaddr*)&recv_addr, addrlen);
    if(rul == -1){
        //printf("receiver: 發送ack失敗, seq爲: %d\n", seq);
        return;
    }

    //printf("已發送seq爲%d的ack報文\n", seq);
    //printf("receiver: 關於start報文的ack已發送\n");
    return;
}

//檢查checksum的值
uint32_t check_pkg(rtp_packet_t rcv_pkg){
    
    //printf("receiver: seq: %d\n", rcv_pkg.rtp.seq_num);
    //printf("receiver: check: %d\n", rcv_pkg.rtp.checksum);
    uint32_t checksum = rcv_pkg.rtp.checksum ;
    rcv_pkg.rtp.checksum = 0;
    uint32_t check = compute_checksum(&rcv_pkg, 11);
    if(rcv_pkg.rtp.type != RTP_START){
        //printf("receiver: 類型不對, 直接退出\n");
        return -1;
    }
    if(checksum != check){
        //printf("receiver: 驗算的checksum :%d\n", check);
        //printf("receiver: 實際收到的checksum: %d\n", checksum);
        //printf("receiver: 報文損壞, checksum不對, 直接退出\n");
        return -1;
    }
    //printf("receiver: 通過驗證\n");
    return rcv_pkg.rtp.seq_num;
}
/**
 * @brief 开启receiver并在所有IP的port端口监听等待连接
 * @param port receiver监听的port
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 */
int initReceiver(uint16_t port, uint32_t window_size){
    //printf("啓動receiver\n");
    sock_rcv = socket(AF_INET, SOCK_DGRAM, 0);
    win_size_rcv = window_size;
    //printf("receiver: winsize: %d\n", win_size_rcv);
	if (-1 == sock_rcv){
		//printf("receiver: socket open err.");
		return -1;
	}
    // 设置超时
    
    timeout.tv_sec = 10;//秒
    timeout.tv_usec = 0;//微秒
    if (setsockopt(sock_rcv, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        //printf("receiver接收超時\n");
        close(sock_rcv);
        //printf("receiver: udp連接已關閉\n");
        return -1;
    }

    //printf("receiver: 綁定本地相關信息\n");
    // 2、绑定本地的相关信息，如果不绑定，则系统会随机分配一个端口号
    local_addr.sin_family = AF_INET;                                //使用IPv4地址
    local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");        //本机IP地址
    local_addr.sin_port = htons(port);                             //端口
    
    if(bind(sock_rcv, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0){
        //printf("receiver: bind連接失敗\n");
        return -1; //终止进程
    }
    //printf("receiver綁定結果爲:\n  addr:%c\n  port: %c\n", &local_addr.sin_addr.s_addr, &local_addr.sin_port);
    //printf("receiver綁定本地結束\n");
    // 3、等待接收对方发送的数据
    
    //初始化收報文緩衝區
    char rcv[11];
    uint64_t time = now_us();
    //printf("receiver等待接收來自sender的報文\n");
    rtp_packet_t rcv_pkg;
    while(1){
        //printf("time: %d", now_us()-time);
        if(now_us()-time>10000000){
            //printf("receiver接收超時\n");
            close(sock_rcv);
            //printf("receiver: udp連接已關閉\n");
            return -1;
        }
        
        int rcvfrm = recvfrom(sock_rcv, &rcv_pkg, 11, 0, (struct sockaddr *)&recv_addr,&addrlen);
        //printf("receiver: sender的start報文已收到\n");
        //printf("rcv: %d\n", rcvfrm);
        if(rcvfrm>0){
            //printf("receiver已收到來自sender的報文\n");
            break;
        }
    }
    
    //驗證收到的報文是否損壞
    uint32_t rcv_acc = check_pkg(rcv_pkg);
    if(rcv_acc<0){
        //printf("receiver: 接收rtp報文出錯,已結束連接\n");
        return -1;
    }
    //printf("rcv_seq: %d\n", rcv_pkg.rtp.seq_num);
    
    pkg_ack(rcv_acc, RTP_ACK);
    
    //printf("receiver_init結束\n");
    return 0;
}

/**
 * @brief 用于接收数据并在接收完后断开RTP连接
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int first = 0; //希望收到的最小報文序號
int recvMessage(char* filename){
    //printf("receiver: 已進入recv_message\n");

    int sum = 0; //記錄收到的總字節數
    rtp_packet_t tmp[win_size_rcv]; //存收到的報文數據
    int acks[win_size_rcv]; //記錄等待窗口狀態
    for(int i=0;i<win_size_rcv;i++){
        acks[i] = 0;
    }

    FILE *fp;
    //printf("rceiver: filename: %s\n", filename);
    if ((fp = fopen(filename, "wb") )== NULL){
        //printf("receiver: 文件打開失敗\n");
        return -1;
    }
    
    uint64_t time = now_us();
    while(1){
        // if(now_us()-time > 10000000){
        //     printf("receiver: 連接已超時, 未收到任何新報文\n");
        //     printf("receiver: 本次接收信息已完成\n");
        //     break;
        // }
        struct sockaddr_in recv_addr;
        socklen_t addrlen = sizeof(recv_addr);
        //初始化收報文緩衝區
        rtp_packet_t mes_rcv;
        if(recvfrom(sock_rcv, &mes_rcv, rtp_size, 0,(struct sockaddr*)&recv_addr, &addrlen) > 0){
            time = now_us();
            //printf("receiver: 已收到seq爲:%d的報文\n", mes_rcv.rtp.seq_num);
            if(mes_rcv.rtp.type == RTP_END){
                //printf("receiver: 終止連接\n");
                //發送ack報文
                //printf("開始關閉連接\n");
                pkg_ack(mes_rcv.rtp.seq_num, RTP_ACK);
                //printf("截至收到end報文前receiver收到的總字節數爲: %d\n", sum);
                return sum;
            }
            if(mes_rcv.rtp.type != RTP_DATA){
                //printf("receiver: 類型不對\n");
                continue;
            }
            //驗證收到的報文是否損壞
            //int len = atoi(mes_rcv.rtp.length);
            if(mes_rcv.rtp.seq_num >= first+win_size_rcv){
                //printf("receiver: 超出範圍，忽視\n");
                continue;
            }

            uint32_t check = mes_rcv.rtp.checksum;
            //printf("接受到的報文checksum爲:%d\n", check);
            mes_rcv.rtp.checksum = 0;
            uint32_t checksum = compute_checksum(&mes_rcv, 11+mes_rcv.rtp.length);
            //printf("校驗報文checksum爲:%d\n", checksum);
            if(checksum != check){
                //printf("receiver: 報文損壞, checksum不對, 丟棄報文\n");
                continue;
            }
            //正是當前等待的seq, 更新窗口
            if(mes_rcv.rtp.seq_num == first){
                acks[mes_rcv.rtp.seq_num%win_size_rcv] = 1;
                int i;
                for(i=first;i<first+win_size_rcv;i++){
                    if(!acks[i%win_size_rcv]){
                        //printf("receiver窗口下沿將更新爲:%d\n", i);
                        break;
                    }
                }
                
                //更新窗口最小值，則下一個期望的seq爲i
                //報文存入tmp數組中備用
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.checksum=mes_rcv.rtp.checksum;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length = mes_rcv.rtp.length;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.seq_num = mes_rcv.rtp.seq_num;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.type = mes_rcv.rtp.type;
                memcpy(tmp[mes_rcv.rtp.seq_num%win_size_rcv].payload, mes_rcv.payload, tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length);
        
                //將窗口前的內容全部寫入，並清理緩衝區
                for(int j=first;j<i;j++){
                    //printf("此時的j爲: %d\n", j);
                    //printf("receiver: fwrite!!\n");
                    sum +=fwrite(tmp[j%win_size_rcv].payload, sizeof(char), tmp[j%win_size_rcv].rtp.length,fp); 
                    //sum+=tmp[j%win_size_rcv].rtp.length;
                    fflush(fp);
                }
                //printf("receiver: 寫入文件sum的值更新爲: %d\n", sum);
                //更新acks
                for(int j=first;j<i;j++){
                    acks[j%win_size_rcv] = 0;
                }
                //更新窗口
                first = i;

                //發送ack報文
                pkg_ack(first, RTP_ACK);
                //printf("receiver窗口滑動\n");
                //time = now_us();
                //printf("receiver目前還未收到的最小報文seq爲: %d\n", first);
                continue;
            }
            if(mes_rcv.rtp.seq_num>first){
                acks[mes_rcv.rtp.seq_num%win_size_rcv] = 1;
                //報文存入tmp數組中備用
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.checksum=mes_rcv.rtp.checksum;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length = mes_rcv.rtp.length;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.seq_num = mes_rcv.rtp.seq_num;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.type = mes_rcv.rtp.type;
                memcpy(tmp[mes_rcv.rtp.seq_num%win_size_rcv].payload, mes_rcv.payload, tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length);
                //發送ack報文
                pkg_ack(first, RTP_ACK);
                //printf("receiver目前還未收到的最小報文seq爲: %d\n", first);
                continue;
            }
        }
        
    }
    //printf("receiver: 關閉文件\n");
    fclose(fp);
    return sum;
}
/**
 * @brief 用于接收数据失败时断开RTP连接以及关闭UDP socket
 */
void terminateReceiver(){
    //printf("receiver: terminate\n");
    close(sock_rcv);
    return;
}

/**
 * @brief 用于接收数据并在接收完后断开RTP连接(优化版本的RTP)
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int recvMessageOpt(char* filename){
    //printf("receiver: 已進入recv_message\n");
    int sum = 0; //記錄收到的總字節數
    rtp_packet_t tmp[win_size_rcv]; //存收到的報文數據
    int acks[win_size_rcv]; //記錄等待窗口狀態
    for(int i=0;i<win_size_rcv;i++){
        acks[i] = 0;
    }

    FILE *fp;
    //printf("rceiver: filename: %s\n", filename);
    if ((fp = fopen(filename, "wb") )== NULL){
        //printf("receiver: 文件打開失敗\n");
        return -1;
    }
    
    uint64_t time = now_us();
    while(1){
        // if(now_us()-time > 10000000){
        //     printf("receiver: 連接已超時, 未收到任何新報文\n");
        //     printf("receiver: 本次接收信息已完成\n");
        //     break;
        // }
        struct sockaddr_in recv_addr;
        socklen_t addrlen = sizeof(recv_addr);
        //初始化收報文緩衝區
        rtp_packet_t mes_rcv;
        if(recvfrom(sock_rcv, &mes_rcv, rtp_size, 0,(struct sockaddr*)&recv_addr, &addrlen) > 0){
            time = now_us();
            //printf("receiver: 已收到seq爲:%d的報文\n", mes_rcv.rtp.seq_num);
            if(mes_rcv.rtp.type == RTP_END){
                //printf("receiver: 終止連接\n");
                //發送ack報文
                //printf("開始關閉連接\n");
                pkg_ack(mes_rcv.rtp.seq_num, RTP_ACK);
                //printf("截至收到end報文前receiver收到的總字節數爲: %d\n", sum);
                return sum;
            }
            if(mes_rcv.rtp.type != RTP_DATA){
                //printf("receiver: 類型不對\n");
                continue;
            }
            //驗證收到的報文是否損壞
            //int len = atoi(mes_rcv.rtp.length);
            if(mes_rcv.rtp.seq_num >= first+win_size_rcv){
                //printf("receiver: 超出範圍，忽視\n");
                continue;
            }

            uint32_t check = mes_rcv.rtp.checksum;
            //printf("接受到的報文checksum爲:%d\n", check);
            mes_rcv.rtp.checksum = 0;
            uint32_t checksum = compute_checksum(&mes_rcv, 11+mes_rcv.rtp.length);
            //printf("校驗報文checksum爲:%d\n", checksum);
            if(checksum != check){
                //printf("receiver: 報文損壞, checksum不對, 丟棄報文\n");
                continue;
            }
            //正是當前等待的seq, 更新窗口
            if(mes_rcv.rtp.seq_num == first){
                acks[mes_rcv.rtp.seq_num%win_size_rcv] = 1;
                int i;
                for(i=first;i<first+win_size_rcv;i++){
                    if(!acks[i%win_size_rcv]){
                        //printf("receiver窗口下沿將更新爲:%d\n", i);
                        break;
                    }
                }
                
                //更新窗口最小值，則下一個期望的seq爲i
                //報文存入tmp數組中備用
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.checksum=mes_rcv.rtp.checksum;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length = mes_rcv.rtp.length;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.seq_num = mes_rcv.rtp.seq_num;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.type = mes_rcv.rtp.type;
                memcpy(tmp[mes_rcv.rtp.seq_num%win_size_rcv].payload, mes_rcv.payload, tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length);
        
                //將窗口前的內容全部寫入，並清理緩衝區
                for(int j=first;j<i;j++){
                    //printf("此時的j爲: %d\n", j);
                    //printf("receiver: fwrite!!\n");
                    sum +=fwrite(tmp[j%win_size_rcv].payload, sizeof(char), tmp[j%win_size_rcv].rtp.length,fp); 
                    //sum+=tmp[j%win_size_rcv].rtp.length;
                    fflush(fp);
                }
                //printf("receiver: 寫入文件sum的值更新爲: %d\n", sum);
                //更新acks
                for(int j=first;j<i;j++){
                    acks[j%win_size_rcv] = 0;
                }
                //更新窗口
                first = i;

                //發送ack報文
                pkg_ack(mes_rcv.rtp.seq_num, RTP_ACK);
                //printf("receiver窗口滑動\n");
                //time = now_us();
                //printf("receiver目前還未收到的最小報文seq爲: %d\n", first);
                continue;
            }
            if(mes_rcv.rtp.seq_num>first){
                acks[mes_rcv.rtp.seq_num%win_size_rcv] = 1;
                //報文存入tmp數組中備用
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.checksum=mes_rcv.rtp.checksum;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length = mes_rcv.rtp.length;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.seq_num = mes_rcv.rtp.seq_num;
                tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.type = mes_rcv.rtp.type;
                memcpy(tmp[mes_rcv.rtp.seq_num%win_size_rcv].payload, mes_rcv.payload, tmp[mes_rcv.rtp.seq_num%win_size_rcv].rtp.length);
                //發送ack報文
                pkg_ack(mes_rcv.rtp.seq_num, RTP_ACK);
                //printf("receiver目前還未收到的最小報文seq爲: %d\n", first);
                continue;
            }
        }
        
    }
    //printf("receiver: 關閉文件\n");
    fclose(fp);
    return sum;
}