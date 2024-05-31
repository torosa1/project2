Term Project #2

프로젝트 코드 특징

송신자에서 수신자로 파일을 전송할 수 있는 코드입니다. 파일은 고정된 길이의 데이터 청크로 분할되어야 합니다. 각 청크의 데이터는 UDP 소켓을 통해 전송됩니다.

단순화를 위해 프로토콜을 단방향으로 만들었습니다. 즉, 문자 파일은 송신자에서 수신자로, ACK는 수신자에서 송신자로만 흐르게 됩니다.

RDT 프로토콜로 패킷 손실, 중복 패킷 기능이 있습니다.

송신자와 수신자가 사용자 정의 확률에 따라 수신된 패킷을 폐기할 수 있습니다.

네트워크 상황에 따라 패킷 폐기 확률을 정할 수 있습니다.

손실된 패킷을 재전송하는 타이머에 알람 시스템 호출을 사용할 수 있습니다.

Packet 클래스가 직렬화하여 수신합니다.(alarm함수)


직렬화 및 역직렬화 기능

패킷.h
```c
#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define DATA 0
#define ACK 1
#define EOT 2
#define MAX_DATA_SIZE 1000

typedef struct {
    int type;
    int seqNum;
    int ackNum;
    int length;
    char data[MAX_DATA_SIZE];
} Packet;

void serialize(Packet *pkt, char *buffer);
void deserialize(char *buffer, Packet *pkt);

#endif
```
type: 패킷의 유형을 나타내는 정수 값으로, DATA, ACK, EOT 중 하나를 가집니다.
seqNum: 데이터 패킷의 시퀀스 번호를 나타냅니다.
ackNum: 확인 패킷의 확인 번호를 나타냅니다.
length: 패킷의 데이터 길이를 나타냅니다.
data: 실제 데이터를 저장하는 문자열 배열입니다. 최대 데이터 크기는 MAX_DATA_SIZE로 정의되어 있습니다.

패킷.c
```c
#include "packet.h"
#include <string.h>

void serialize(Packet *pkt, char *buffer) {
    memcpy(buffer, pkt, sizeof(Packet));
}

void deserialize(char *buffer, Packet *pkt) {
    memcpy(pkt, buffer, sizeof(Packet));
}

```
직렬화 및 역직렬화 함수
serialize() 함수는 Packet 구조체를 직렬화하여 문자열로 변환하고, 
deserialize() 함수는 문자열을 Packet 구조체로 역직렬화하여 변환합니다. 
이러한 함수들은 네트워크 통신 시 데이터를 패킷으로 변환하고 다시 원래 형태로 되돌리는 데 사용됩니다.

사용법
헤더 파일을 포함하여 프로그램에서 이 구조체와 함수를 사용하여 데이터를 패킷으로 변환하고 역직렬화할 수 있습니다. 
예를 들어, 데이터를 전송할 때 serialize() 함수를 사용하여 데이터를 패킷으로 변환하고, 
수신측에서는 deserialize() 함수를 사용하여 패킷을 다시 데이터로 변환할 수 있습니다.


송신자
이 프로그램은 네트워크를 통해 파일을 안전하게 전송하는 간단한 프로그램입니다. 이 프로그램은 C 언어로 작성되었으며, UDP를 사용하여 데이터를 전송합니다.

```c
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
    serialize(pkt, buffer);
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, addrlen);
}

void rdt_rcv() {
    char buffer[sizeof(Packet)];
    Packet ack_pkt;
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, &addrlen) > 0) {
        deserialize(buffer, &ack_pkt);
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

```
사용법
./render <송신포트> <수신IP> <수신포트> <타임아웃> <파일이름> <ACK손실확률>

<송신포트>: 데이터를 송신하는 포트 번호입니다.
<수신IP>: 데이터를 수신하는 대상의 IP 주소입니다.
<수신포트>: 데이터를 수신하는 포트 번호입니다.
<타임아웃>: ACK를 기다리는 타임아웃 시간입니다.
<파일이름>: 전송할 파일의 이름입니다.
<ACK손실확률>: ACK를 손실할 확률입니다.

파일 구성
file_transfer.c: 메인 프로그램 소스 코드 파일입니다.
packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

프로그램 작동순서
1. 입력된 파일을 읽어서 데이터 패킷으로 분할합니다.
2. 데이터 패킷을 송신하고, ACK를 기다립니다. 만약 ACK를 받지 못할 경우, 타임아웃이 발생하거나 ACK가 손실될 때까지 재전송을 시도합니다.
3. 파일의 끝에 도달할 때까지 위 과정을 반복합니다.
4. 파일 전송이 완료되면, 종료를 알리는 특별한 종료 패킷(EOT)을 전송합니다.

수신자

이 프로그램은 네트워크를 통해 전송된 파일을 수신하는 간단한 프로그램입니다. 이 프로그램은 C 언어로 작성되었으며, UDP를 사용하여 데이터를 수신합니다.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "packet.h"

int sockfd;
struct sockaddr_in local_addr, sender_addr;
socklen_t addrlen;
float data_drop_prob;
FILE *file;
int expected_seqNum = 0;

void ack_send(int seqNum) {
    Packet ack_pkt = {ACK, 0, seqNum, 0};
    char buffer[sizeof(Packet)];
    serialize(&ack_pkt, buffer);
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, addrlen);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <receiver_port> <data_drop_prob>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int receiver_port = atoi(argv[1]);
    data_drop_prob = atof(argv[2]);

    srand(time(0));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(receiver_port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr));

    addrlen = sizeof(sender_addr);
    char buffer[sizeof(Packet)];
    Packet pkt;

    file = fopen("new file", "wb");

    while (1) {
        if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &addrlen) > 0) {
            deserialize(buffer, &pkt);

            if (pkt.type == DATA) {
                if ((float)rand() / RAND_MAX < data_drop_prob) {
                    printf("seqNum %d번 데이터 드랍\n", pkt.seqNum);
		    sleep(1);
                    continue;
                }

                if (pkt.seqNum == expected_seqNum) {
                    fwrite(pkt.data, 1, pkt.length, file);
                    ack_send(expected_seqNum);
                    expected_seqNum++;
                } else {
                    ack_send(expected_seqNum - 1);
                }
            } else if (pkt.type == EOT) {
                printf("파일 전송 완료!\n");
                break;
            }
        }
    }

    fclose(file);
    close(sockfd);
    return 0;
}

```
사용법
./receiver <수신포트> <데이터손실확률>

<수신포트>: 데이터를 수신하는 포트 번호입니다.
<데이터손실확률>: 데이터를 드랍할 확률입니다.

파일 구성
receiver.c: 메인 프로그램 소스 코드 파일입니다.
packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

프로그램 작동순서
1. 데이터를 송신하는 프로그램에서 전송된 데이터 패킷을 수신합니다.
2. 수신된 패킷의 시퀀스 번호를 확인하여 순서대로 수신하는지 확인합니다.
3. 데이터를 드랍할 확률에 따라 일부 패킷을 드랍하고, 나머지는 파일에 쓰여집니다.
4. 올바른 시퀀스 번호를 가진 데이터 패킷에 대해 ACK(확인)을 송신하는 패킷을 전송합니다.
5. 파일 전송이 완료되면, 특별한 종료 패킷(EOT)을 수신하여 프로그램을 종료합니다.
