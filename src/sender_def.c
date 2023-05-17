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

//定義全局變量
char recv_ip[1024] = {0};
uint16_t recv_port;
uint32_t win_size = 0;
int sock;
struct sockaddr_in local;
//socklen_t len_sen = sizeof(local);
socklen_t local_len;
rtp_packet_t rtp_load;
struct timeval stimeout;
#define rtp_size 1472

void pkg_rtp(int sock, struct sockaddr_in local, uint8_t ty, uint32_t seq){

    rtp_load.rtp.type = ty;
    rtp_load.rtp.length = 0;
    rtp_load.rtp.seq_num = seq;
    //printf("發送seq爲 :%d\n", rtp_load.rtp.seq_num);
    rtp_load.rtp.checksum = 0;
    memset(rtp_load.payload, 0, sizeof(rtp_load.payload));
    rtp_load.rtp.checksum = compute_checksum(&rtp_load, 11);
    //printf("報文checksum 爲 :%d\n", rtp_load.rtp.checksum);
    socklen_t local_len;
    local_len = sizeof(local);
    //char send_tmp[rtp_size];

    ssize_t rul  = 0;
    rul = sendto(sock, &rtp_load, 11, 0, (struct sockaddr*)&local, local_len);
    if(rul == -1){
        //printf("發送pkg失敗, seq爲: %d\n", seq);
    }
    return;
}

/**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
int 
initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size){
    
    //printf("啓動sender\n");
    //全局變量傳參
    recv_port = receiver_port;
    //printf("準備獲取ip\n");
    memcpy(recv_ip, receiver_ip, sizeof(receiver_ip));
    //printf("獲取ip成功\n");
    win_size = window_size;
    //printf("window_size is %d\n", win_size);

    //udp連接
    sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock == -1){
        //printf("socket 錯誤\n");
        return -1;
    }

    //初始化服務器信息
    local.sin_family = AF_INET;
    local.sin_port = htons(receiver_port);
    local.sin_addr.s_addr = inet_addr(receiver_ip);

    pkg_rtp(sock, local, RTP_START, rand());
    uint64_t time1 = now_us();
    //printf("sender發送start成功\n");

    char buf[11] = {0};
    stimeout.tv_sec = 0;//秒
    stimeout.tv_usec = 100000;//微秒
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &stimeout, sizeof(stimeout)) == -1) {
        //printf("sender接收超時\n");
        close(sock);
        //printf("接收ack超時\n");
        //printf("直接發送報文\n");
        //printf("sender_init結束\n");
        return 0;
    }
    while(1){
        //printf("開始打印當前時間：%d\n", now_us()-time1);
        rtp_packet_t sender_rcv;
        if(recvfrom(sock, &sender_rcv, 11, 0, NULL, NULL)>0){
            //printf("接收ack成功\n");
            //驗證ack是否正確
            if(sender_rcv.rtp.type == RTP_ACK && sender_rcv.rtp.seq_num == rtp_load.rtp.seq_num){
                //printf("ack正確, 連接已建立\n");
                return 0;
            }
            //收到的ACK損壞，直接發送END報文
            //printf("連接失敗, ACK錯誤\n");
            terminateSender();
            //printf("sender_init結束\n");
            return -1;
        }
    }
    return 0;

}

//獲取當前窗口ack狀態
// int check_ack(int *acks, int size, int base){
//     for(int i=base;i<base+size;i++){
//         if(!acks[i%win_size]){
//             printf("當前窗口中第%d個ack未收到\n", i-base);
//             return i-base;
//         }
//     }
//     return size;
// }

//存儲已發送還未確認的窗口的報文
void save_send(rtp_packet_t *dst, rtp_packet_t *rsc){
    dst->rtp.type = rsc->rtp.type;
    dst->rtp.length = rsc->rtp.length;
    dst->rtp.seq_num = rsc->rtp.seq_num;
    dst->rtp.checksum = rsc->rtp.checksum;
    memcpy(dst->payload, rsc->payload, rsc->rtp.length);
    //printf("已存儲seq爲%d的報文\n", rsc->rtp.seq_num);
    return;
}

/**
 * @brief 用于发送数据
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int base = 0; //作爲窗口標誌位，標誌當前窗口的最前端
int nxt_seq = 0; // 記錄下一個將要發出的seq

int sendMessage(const char* message){
    int sendsum = 0;
    //printf("進入sender_send\n");
    //int acks[win_size];  //記錄收到的ack個數
    rtp_packet_t sen_save[win_size];//緩存要發出的數據包

    // for(int i=0;i<win_size;i++){
    //     acks[i] = 0;
    // }
    
    //printf("message: %s\n", message);
    FILE *fp;
    if ((fp = fopen(message, "rb")) == NULL){
        //printf("文件打開失敗\n");
        return -1;
    }
    uint64_t start_time = now_us(); //窗口計時器
    stimeout.tv_sec = 0;//秒
    stimeout.tv_usec = 100000;//微秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &stimeout, sizeof(stimeout));

    //開始wait狀態機
    while(1){
        //超時重傳
        if(now_us()-start_time > 100000){
            //重置計時器
            
            //重置當前窗口
            //nxt_seq = base;
            //重發
            //printf("重置base: %d, 重置seq: %d\n", base, nxt_seq);
            for(int i=base;i<nxt_seq;i++){
                //printf("已重發seq爲%d的包\n", i);
                sendto(sock, &sen_save[i%win_size], sen_save[i%win_size].rtp.length+11, 0, (struct sockaddr*)&local, sizeof(local));
            }
            //printf("窗口緩存已重發\n");
            start_time = now_us();
            //continue;
        }
        //窗口未滿
        if(nxt_seq < base+win_size){
            //rtp報文頭部填充
            //rtp_packet_t rtp_load;
            char tmp[PAYLOAD_SIZE] = {0};
            rtp_load.rtp.type = RTP_DATA;
            rtp_load.rtp.length = fread(tmp, sizeof(char), PAYLOAD_SIZE, fp);
            //printf("sender: 本次發送的長度爲: %d\n", rtp_load.rtp.length);
            
            // if(rtp_load.rtp.length == 0 ){
            //     printf("該文件已發送完畢,sender發送了%d個字節\n", sendsum);
            //     printf("base: %d, nxt_seq: %d\n", base, nxt_seq);
            //     break;
            // }
            rtp_load.rtp.seq_num = nxt_seq;
            rtp_load.rtp.checksum = 0;
            memset(rtp_load.payload, 0, sizeof(rtp_load.payload));
            memcpy(rtp_load.payload, tmp, rtp_load.rtp.length);
            rtp_load.rtp.checksum = compute_checksum(&rtp_load, rtp_load.rtp.length+11);
            //printf("seq is : %d, checksum is :%d\n", nxt_seq, rtp_load.rtp.checksum);
            //存儲已發送的報文
            save_send(&sen_save[nxt_seq%win_size], &rtp_load);

            //發送udp報文
            if(rtp_load.rtp.length){
                int suc = sendto(sock, &rtp_load, rtp_load.rtp.length+11, 0, (struct sockaddr*)&local, sizeof(local));
                if(suc == -1){
                    //printf("發送data: %d失敗\n", rtp_load.rtp.seq_num);
                    continue;
                }
                
                // if(nxt_seq == base && nxt_seq!=0){  //說明剛剛開始或者剛剛重置過窗口
                //     printf("重新發送data: %d成功\n", rtp_load.rtp.seq_num);
                //     start_time = now_us();
                //     printf("窗口計時器已重置\n");
                // }
                if(suc > 0 ){
                    nxt_seq++;
                    sendsum+=rtp_load.rtp.length;
                    //printf("已發送的總字節爲： %d\n", sendsum);
                    continue;
                }
            }
            
        }
        //初始化收報文緩衝區
        rtp_packet_t senmes_rcv;
        uint32_t checksum = 0;
        local_len = sizeof(local);
        //非阻塞收報文
        if(recvfrom(sock, &senmes_rcv, 11, 0, (struct sockaddr *)&local, &local_len)){
            //驗證收到的報文是否損壞
            if(senmes_rcv.rtp.type != RTP_ACK || senmes_rcv.rtp.seq_num < base){
                //printf("報文損壞或者不在範圍內\n");
                //printf("接受到的報文seq爲: %d, 當前base爲: %d\n", senmes_rcv.rtp.seq_num, base);
                continue;
            }
            //報文未損壞且在窗口範圍內
            //printf("接收到了seq爲%d的ack\n", senmes_rcv.rtp.seq_num);
            //acks[(senmes_rcv.rtp.seq_num-1) %win_size] = 1;
            //int change = check_ack(acks, win_size, base);
            //窗口滑動
            if(senmes_rcv.rtp.seq_num > base){
                base = senmes_rcv.rtp.seq_num;
                //printf("change: base: %d, nxt_seq: %d\n", base, nxt_seq);
                if(base >= nxt_seq && rtp_load.rtp.length == 0) {
                    //printf("該文件已發送完畢,sender發送了%d個字節\n", sendsum);
                    
                    break;
                }    
                   //傳輸完畢
                //更新ack的值
                // for(int i=base-change;i<base;i++){
                //     acks[i%win_size] = 0;
                // }
                //printf("窗口滑動，計時器重啓\n");
                start_time = now_us();
            }
            continue;
        }
    }
    // printf("sender發送end成功\n");
    // pkg_rtp(sock, local, RTP_END, nxt_seq);
    // uint64_t time1 = now_us();
    // //等待end報文的ack
    // rtp_packet_t end_rcv;
    // // int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
    // // printf("ret: %d\n", ret);
    
    stimeout.tv_sec = 0;//秒
    stimeout.tv_usec = 100000;//微秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &stimeout, sizeof(stimeout));
    uint64_t time_end = now_us();

    while(now_us() - time_end < 100000){
        //printf("sender發送end成功\n");
        pkg_rtp(sock, local, RTP_END, nxt_seq);
        //等待end報文的ack
        rtp_packet_t end_rcv;
        // int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
        // printf("ret: %d\n", ret);
        int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
        //printf("ret: %d\n, nxt_seq: %d\n", ret, nxt_seq);
        if(ret>0){
            if(end_rcv.rtp.seq_num!=nxt_seq){
                //printf("endrevseq: %d, nxt_seq: %d\n", end_rcv.rtp.seq_num, nxt_seq);
                continue;
            }
            //printf("接收ack成功\n");
            //驗證ack是否正確
            if(end_rcv.rtp.type == RTP_ACK && end_rcv.rtp.seq_num == nxt_seq){
                //printf("ack正確, 可正常關閉\n");
                //開始關閉udp連接
                close(sock);
                return 0;
            }
            //收到的ACK損壞，直接發送END報文
            //printf("驗證失敗, ACK錯誤\n");
            //printf("還沒結束報文，不能關閉\n");
            return -1;
        }
    }
    close(sock);
        //printf("sender接收end的ack超時,直接結束\n");
        fclose(fp);
        return 0;
}

/**
 * @brief 用于断开RTP连接以及关闭UDP socket
 **/
void terminateSender(){

    //關閉udp連接
    close(sock);
    //printf("sender: terminate\n");
    //printf("sender的udp連接已關閉\n");
    return 0;
}

/**
 * @brief 用于发送数据(优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessageOpt(const char* message){
    int sendsum = 0;
    //printf("進入sender_send\n");
    int acks[win_size];  //記錄收到的ack個數
    rtp_packet_t sen_save[win_size];//緩存要發出的數據包

    for(int i=0;i<win_size;i++){
        acks[i] = 0;
    }
    
    //printf("message: %s\n", message);
    FILE *fp;
    if ((fp = fopen(message, "rb")) == NULL){
        //printf("文件打開失敗\n");
        return -1;
    }
    uint64_t start_time = now_us(); //窗口計時器
    stimeout.tv_sec = 0;//秒
    stimeout.tv_usec = 100000;//微秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &stimeout, sizeof(stimeout));

    //開始wait狀態機
    while(1){
        //超時重傳
        if(now_us()-start_time > 100000){
            //重置計時器
            
            //重置當前窗口
            //nxt_seq = base;
            //重發
            //printf("重置base: %d, 重置seq: %d\n", base, nxt_seq);
            for(int i=base;i<nxt_seq;i++){
                if(!acks[i%win_size]){
                    //printf("已重發seq爲%d的包\n", i);
                    sendto(sock, &sen_save[i%win_size], sen_save[i%win_size].rtp.length+11, 0, (struct sockaddr*)&local, sizeof(local));
                }
            }
            //printf("窗口緩存已重發\n");
            start_time = now_us();
            //continue;
        }
        //窗口未滿
        if(nxt_seq < base+win_size){
            //rtp報文頭部填充
            //rtp_packet_t rtp_load;
            char tmp[PAYLOAD_SIZE] = {0};
            rtp_load.rtp.type = RTP_DATA;
            rtp_load.rtp.length = fread(tmp, sizeof(char), PAYLOAD_SIZE, fp);
            //printf("sender: 本次發送的長度爲: %d\n", rtp_load.rtp.length);
            
            // if(rtp_load.rtp.length == 0 ){
            //     printf("該文件已發送完畢,sender發送了%d個字節\n", sendsum);
            //     printf("base: %d, nxt_seq: %d\n", base, nxt_seq);
            //     break;
            // }
            rtp_load.rtp.seq_num = nxt_seq;
            rtp_load.rtp.checksum = 0;
            memset(rtp_load.payload, 0, sizeof(rtp_load.payload));
            memcpy(rtp_load.payload, tmp, rtp_load.rtp.length);
            rtp_load.rtp.checksum = compute_checksum(&rtp_load, rtp_load.rtp.length+11);
            //printf("seq is : %d, checksum is :%d\n", nxt_seq, rtp_load.rtp.checksum);
            //存儲已發送的報文
            save_send(&sen_save[nxt_seq%win_size], &rtp_load);

            //發送udp報文
            if(rtp_load.rtp.length){
                int suc = sendto(sock, &rtp_load, rtp_load.rtp.length+11, 0, (struct sockaddr*)&local, sizeof(local));
                if(suc == -1){
                    //printf("發送data: %d失敗\n", rtp_load.rtp.seq_num);
                    continue;
                }
                
                // if(nxt_seq == base && nxt_seq!=0){  //說明剛剛開始或者剛剛重置過窗口
                //     printf("重新發送data: %d成功\n", rtp_load.rtp.seq_num);
                //     start_time = now_us();
                //     printf("窗口計時器已重置\n");
                // }
                if(suc > 0 ){
                    nxt_seq++;
                    sendsum+=rtp_load.rtp.length;
                    //printf("已發送的總字節爲： %d\n", sendsum);
                    continue;
                }
            }
            
        }
        //初始化收報文緩衝區
        rtp_packet_t senmes_rcv;
        uint32_t checksum = 0;
        local_len = sizeof(local);
        //非阻塞收報文
        if(recvfrom(sock, &senmes_rcv, 11, 0, (struct sockaddr *)&local, &local_len)){
            //驗證收到的報文是否損壞
            if(senmes_rcv.rtp.type != RTP_ACK || senmes_rcv.rtp.seq_num < base){
                //printf("報文損壞或者不在範圍內\n");
                //printf("接受到的報文seq爲: %d, 當前base爲: %d\n", senmes_rcv.rtp.seq_num, base);
                continue;
            }
            //報文未損壞且在窗口範圍內
            //printf("接收到了seq爲%d的ack\n", senmes_rcv.rtp.seq_num);
            acks[senmes_rcv.rtp.seq_num % win_size] = 1;
            //int change = check_ack(acks, win_size, base);
            //窗口滑動
            int hua = 0;
            while(1){
                if(acks[base%win_size]==0){
                    if(hua){
                        //printf("窗口滑動，計時器重啓\n");
                        start_time = now_us();
                    }
                    break;
                }
                hua ++;
                acks[base%win_size]= 0;
                base++;
            }
            // if(senmes_rcv.rtp.seq_num >= base){
            //     //更新ack的值
            //     for(int i=base;i<=senmes_rcv.rtp.seq_num;i++){
            //         acks[i%win_size] = 0;
            //     }
            //     base = senmes_rcv.rtp.seq_num+1;
            //     //printf("change: base: %d, nxt_seq: %d\n", base, nxt_seq);
            if(base >= nxt_seq && rtp_load.rtp.length == 0) {
                //printf("該文件已發送完畢,sender發送了%d個字節\n", sendsum);
                
                break;
            }    
                   //傳輸完畢
        }
            continue;
    }
    
    // printf("sender發送end成功\n");
    // pkg_rtp(sock, local, RTP_END, nxt_seq);
    // uint64_t time1 = now_us();
    // //等待end報文的ack
    // rtp_packet_t end_rcv;
    // // int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
    // // printf("ret: %d\n", ret);
    
    stimeout.tv_sec = 0;//秒
    stimeout.tv_usec = 100000;//微秒
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &stimeout, sizeof(stimeout));
    uint64_t time_end = now_us();

    while(now_us() - time_end < 100000){
        //printf("sender發送end成功\n");
        pkg_rtp(sock, local, RTP_END, nxt_seq);
        //等待end報文的ack
        rtp_packet_t end_rcv;
        // int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
        // printf("ret: %d\n", ret);
        int ret = recvfrom(sock, &end_rcv, 11, 0, (struct sockaddr *)&local, &local_len);
        //printf("ret: %d\n, nxt_seq: %d\n", ret, nxt_seq);
        if(ret>0){
            if(end_rcv.rtp.seq_num!=nxt_seq){
                //printf("endrevseq: %d, nxt_seq: %d\n", end_rcv.rtp.seq_num, nxt_seq);
                continue;
            }
            //printf("接收ack成功\n");
            //驗證ack是否正確
            if(end_rcv.rtp.type == RTP_ACK && end_rcv.rtp.seq_num == nxt_seq){
                //printf("ack正確, 可正常關閉\n");
                //開始關閉udp連接
                close(sock);
                return 0;
            }
            //收到的ACK損壞，直接發送END報文
            //printf("驗證失敗, ACK錯誤\n");
            //printf("還沒結束報文，不能關閉\n");
            return -1;

        }
    }
    close(sock);
    //printf("sender接收end的ack超時,直接結束\n");
    fclose(fp);
    return 0;
}