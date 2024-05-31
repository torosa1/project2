#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "packet.h"

int sockfd;
struct sockaddr_in receiver_addr;
socklen_t addrlen;
int timeout_interval;
float ack_drop_prob;
FILE *file;
int seqNum = 0;
int ack_receiver = 0;

void rdt_send(Packet *pkt) {
    char buffer[sizeof(Packet)];
    serialize_packet(pkt, buffer);
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, addrlen);
}

void rdt_rcv() {
    char buffer[sizeof(Packet)];
    Packet ack_pkt;
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, &addrlen) > 0) {
        deserialize_packet(buffer, &ack_pkt);
        if (ack_pkt.type == ACK && ack_pkt.ackNum == seqNum) {
            ack_receiver = 1;
            printf("ACK: 파일 %dKB 받음 ( seqNum %d번)\n", seqNum,seqNum+1);
            sleep(1);
        }
    }
}

void signalrm_handler(int sig){
    printf("재전송 요청\n");
    ack_receiver = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "입력방식: %s <송신포트> <수신IP> <수신포트> <타임아웃> <파일이름> <ACK손실확률>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sender_port = atoi(argv[1]);
    char *receiver_ip = argv[2];
    int receiver_port = atoi(argv[3]);
    timeout_interval = atoi(argv[4]);
    char *file_name = argv[5];
    ack_drop_prob = atof(argv[6]);

    file = fopen(file_name, "rb");
    if (file == NULL) {
        perror("파일 열기 실패");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);
    receiver_addr.sin_addr.s_addr = inet_addr(receiver_ip);
    addrlen = sizeof(receiver_addr);

    signal(SIGALRM, signalrm_handler);

    char buffer[MAX_DATA_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, MAX_DATA_SIZE, file)) > 0) {
        Packet pkt = {DATA, seqNum, 0, bytes_read};
        memcpy(pkt.data, buffer, bytes_read);

        ack_receiver = 0;
        while (!ack_receiver) {
            rdt_send(&pkt);
            alarm(timeout_interval);
            rdt_rcv();
            alarm(0);
        }
        seqNum++;
    }

    Packet finish_tran = {EOT, seqNum, 0, 0};
    rdt_send(&finish_tran);
    printf("파일 전송 완료!\n");

    fclose(file);
    close(sockfd);
    return 0;
}
