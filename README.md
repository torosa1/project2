# Term Project #2

### 프로젝트 프로그램의 목적

낮은 신뢰성을 가진 UDP에 신뢰성을 주고자 나온 RDT프로토콜을 작동원리를 이해하기 위해 간단한 프로그램을 만드는 것

### 프로젝트 코드 특징

송신자에서 수신자로 파일을 전송할 수 있는 코드입니다. 파일은 고정된 길이의 데이터 청크로 분할되어야 합니다. 각 청크의 데이터는 UDP 소켓을 통해 전송됩니다.

단순화를 위해 프로토콜을 단방향으로 만들었습니다. 즉, 문자 파일은 송신자에서 수신자로, ACK는 수신자에서 송신자로만 흐르게 됩니다.

RDT 프로토콜로 패킷 손실, 중복 패킷 기능이 있습니다.

송신자와 수신자가 사용자 정의 확률에 따라 수신된 패킷을 폐기할 수 있습니다.

네트워크 상황에 따라 패킷 폐기 확률을 정할 수 있습니다.

손실된 패킷을 재전송하는 타이머에 알람 시스템 호출을 사용할 수 있습니다.

Packet 클래스가 직렬화하여 수신합니다.(alarm함수)


## packet.h
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
위 코드는 함수나 구조체의 선언을 담고 있습니다.

type: 패킷의 유형을 나타내는 정수 값입니다.
seqNum: 데이터 패킷의 시퀀스 번호를 나타냅니다.
ackNum: 확인 패킷의 확인 번호를 나타냅니다.
length: 패킷의 데이터 길이를 나타냅니다.
data: 실제 데이터를 저장하는 문자열 배열입니다. 최대 데이터 크기는 MAX_DATA_SIZE로 정의되어 있습니다.

## packet.c(직렬화 및 역직렬화 기능)
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
위 코드는 직렬화 및 역직렬화 함수를 포함합니다.

packet.h에 선언된 함수들의 정의를 활용합니다.

### 직렬화 및 역직렬화 함수
serialize() 함수는 Packet 구조체를 직렬화하여 문자열로 변환하고
deserialize() 함수는 문자열을 Packet 구조체로 역직렬화하여 변환합니다.
이러한 함수들은 네트워크 통신 시 데이터를 패킷으로 변환하고 다시 원래 형태로 되돌리는 데 사용됩니다.


## 송신자
이 프로그램은 UDP를 사용하여 여러 기능을 추가해 파일을 안전하게 전송합니다.
크게 추가된 기능은 수신 측에서 보낸 ACK 확인하며 데이터를 전송하고,
일정시간 ACK를 받지 못한다면 가장 최근 데이터를 재전송하는 기능을 가지고 있습니다.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "packet.h"

int sockfd; // 소켓 파일 디스크립터
struct sockaddr_in receiver_addr; // 수신자의 주소 정보 구조체
socklen_t addrlen; // 주소 구조체의 크기
int timeout_interval; // 타임아웃 간격
float ack_drop_prob; // ACK 손실 확률
FILE *file; // 파일 포인터
int seqNum = 0; // seq 번호
int ack_receiver = 0; // ACK 수신 여부를 나타내는 플래그
Packet now_packet; // 현재 전송 중인 패킷

// 패킷을 전송하는 함수
void rdt_send(Packet *pkt) {
    char buffer[sizeof(Packet)]; // 패킷을 직렬화할 버퍼
    serialize(pkt, buffer); // 패킷 직렬화
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, addrlen); // 직렬화된 데이터를 수신자에게 전송
}

// ACK 패킷을 수신하는 함수
void rdt_rcv() {
    char buffer[sizeof(Packet)]; // 수신한 데이터를 저장할 버퍼
    Packet ack_pkt; // 수신한 ACK 패킷을 저장할 구조체
    if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, &addrlen) > 0) { // 데이터 수신 받음음
        deserialize(buffer, &ack_pkt); // 데이터 역직렬화
        if (ack_pkt.type == ACK && ack_pkt.ackNum == seqNum) { // ACK 패킷의 타입과 시퀀스 번호 확인
            ack_receiver = 1; // ACK 수신 여부 플래그 설정
            printf("ACK: 파일 %dKB 받음 (seqNum %d번)\n", seqNum, seqNum); // ACK 수신 메시지 출력
            sleep(1);
        }
    }
}

// 타임아웃 발생 시 호출되는 핸들러 함수
void signalrm_handler(int sig){
    printf("재전송 요청 (seqNum %d)\n", seqNum);
    sleep(1);
    rdt_send(&now_packet); // 현재 패킷 재전송
    alarm(timeout_interval); // 타임아웃 설정
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "입력방식: %s <송신포트> <수신IP> <수신포트> <타임아웃> <파일이름> <ACK손실확률>\n", argv[0]); //사용자 인수 입력 받음
        return 1;
    }

    int sender_port = atoi(argv[1]); // 송신 포트 번호
    char *receiver_ip = argv[2]; // 수신자의 IP 주소
    int receiver_port = atoi(argv[3]); // 수신 포트 번호
    timeout_interval = atoi(argv[4]); // 타임아웃 간격
    char *file_name = argv[5]; // 전송할 파일 이름
    ack_drop_prob = atof(argv[6]); // ACK 손실 확률

    file = fopen(file_name, "rb"); // 읽기 모드로 파일 열기
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // UDP 소켓 생성
   
    memset(&receiver_addr, 0, sizeof(receiver_addr)); // 주소 구조체 설정
    receiver_addr.sin_family = AF_INET; // 주소 체계 설정
    receiver_addr.sin_port = htons(receiver_port); // 포트 번호 설정
    receiver_addr.sin_addr.s_addr = inet_addr(receiver_ip); // IP 주소 설정
    addrlen = sizeof(receiver_addr); // 주소 구조체 크기 설정

    // 타임아웃 시그널 핸들러 설정
    signal(SIGALRM, signalrm_handler); // SIGALRM 신호 발생 시 signalrm_handler 함수 호출

    char buffer[MAX_DATA]; // 파일 데이터를 읽어올 버퍼
    int bytes_read; // 읽어온 바이트 수

    // 파일에서 데이터를 읽어 패킷을 생성하고 전송
    while ((bytes_read = fread(buffer, 1, MAX_DATA, file)) > 0) {
        now_packet = (Packet){DATA, seqNum, 0, bytes_read}; // 새로운 데이터 패킷 생성
        memcpy(now_packet.data, buffer, bytes_read); // 파일 데이터 패킷에 복사

        ack_receiver = 0; // ACK 수신 여부 초기화
        while (!ack_receiver) { // ACK를 받을 때까지 반복
            rdt_send(&now_packet); // 패킷 전송
            alarm(timeout_interval); // 타임아웃 설정
            rdt_rcv(); // ACK 수신 대기
            alarm(0); // 타임아웃 취소
        }
        seqNum++; // seq 번호 증가
    }
    Packet finish_tran = {EOT, seqNum, 0, 0}; // 전송 완료 패킷 생성
    rdt_send(&finish_tran); // 전송 완료 패킷 전송
    printf("파일 전송 완료!\n");

    fclose(file); // 파일 닫기
    close(sockfd); // 소켓 닫기
    return 0;
}

```
### 사용법
./render <송신포트> <수신IP> <수신포트> <타임아웃> <파일이름> <ACK손실확률>

<송신포트>: 데이터를 송신하는 포트 번호입니다.
<수신IP>: 데이터를 수신하는 대상의 IP 주소입니다.
<수신포트>: 데이터를 수신하는 포트 번호입니다.
<타임아웃>: ACK를 기다리는 타임아웃 시간입니다.
<파일이름>: 전송할 파일의 이름입니다.
<ACK손실확률>: ACK를 손실할 확률입니다.

### 파일 구성
file_transfer.c: 메인 프로그램 소스 코드 파일입니다.
packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

### 프로그램 작동순서
1. 입력된 파일을 읽어서 데이터 패킷으로 분할합니다.
2. 데이터 패킷을 송신하고, ACK를 기다립니다. 만약 ACK를 받지 못할 경우, 타임아웃이 발생하거나 ACK가 손실될 때까지 재전송을 시도합니다.
3. 파일의 끝에 도달할 때까지 위 과정을 반복합니다.
4. 파일 전송이 완료되면, 종료를 알리는 특별한 종료 패킷(EOT)을 전송합니다.

## 수신자
이 프로그램은 여러 기능이 추가된 UDP를 사용하여 안전하게 데이터를 수신합니다.
크게 추가된 기능은 데이터를 받으면 reqNum에 따라 ACK를 송신자에게 송신하여 데이터의 수신여부를 확인합니다.


```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include "packet.h"

int sockfd; // 소켓 파일 디스크립터 변수 선언
struct sockaddr_in local_addr, sender_addr; // 송신자, 수신자 주소 정보 구조체 선언
socklen_t addrlen; // 소켓 주소 길이 변수 선언
float data_drop_prob; // 데이터 손실 확률 변수 선언
FILE *file; // 파일 포인터 변수 선언
int expected_seqNum = 0; // 시퀀스 번호 변수 선언

// ACK 패킷을 송신하는 함수 정의
void ack_send(int seqNum) {
    Packet ack_pkt = {ACK, 0, seqNum, 0}; // ACK 패킷 생성
    char buffer[sizeof(Packet)]; // 패킷을 저장할 버퍼 생성
    serialize(&ack_pkt, buffer); // 패킷을 직렬화하여 버퍼에 저장
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, addrlen); // 송신자에게 ACK 패킷 전송
}

int main(int argc, char *argv[]) {
    if (argc != 3) { //인수가 3개가 아닐 경우
	fprintf("입력 조건에 맞지 않습니다.");
        exit(EXIT_FAILURE); // 강제 종료
    }

    int receiver_port = atoi(argv[1]); // 수신 포트 번호를 정수로 변환하여 저장
    data_drop_prob = atof(argv[2]); // 데이터 손실 확률을 실수로 변환하여 저장

    srand(time(0)); // 난수 생성기 초기화

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); // UDP 소켓 생성

    memset(&local_addr, 0, sizeof(local_addr)); // 지역 주소 정보 초기화
    local_addr.sin_family = AF_INET; // IPv4 주소 체계 사용
    local_addr.sin_port = htons(receiver_port); // 수신 포트 번호 설정
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 네트워크 인터페이스에 바인드
    bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)); // 소켓을 지역 주소에 바인드

    addrlen = sizeof(sender_addr); // 송신자 주소의 길이를 저장할 변수 초기화
    char buffer[sizeof(Packet)]; // 패킷을 저장할 버퍼 선언
    Packet pkt; // 패킷 변수 선언

    file = fopen("new file", "wb"); // "new file"이름으로 쓰기 모드로 파일 열기

    while (1) {
        if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &addrlen) > 0) { // 송신자로부터 패킷 수신
            deserialize(buffer, &pkt); // 받은 데이터를 패킷으로 역직렬화

            if (pkt.type == DATA) { // 받은 패킷이 데이터 패킷 확
                if ((float)rand() / RAND_MAX < data_drop_prob) { // 무작위 확률에 따라 데이터를 드랍할지 결정
                    printf("seqNum %d번 데이터 드랍\n", pkt.seqNum); // 드랍된 데이터의 seq 번호 출력
		    sleep(1);
                    continue;
                }

                if (pkt.seqNum == expected_seqNum) { // 받은 패킷의 시퀀스 번호가 기대하는 번호와 일치하면
                    fwrite(pkt.data, 1, pkt.length, file); // 파일에 데이터 쓰기
                    ack_send(expected_seqNum); // ACK 패킷 송신
                    expected_seqNum++; // 기대하는 시퀀스 번호 증가
                } else {
                    ack_send(expected_seqNum - 1); // 마지막으로 수신한 데이터의 ACK 패킷 송신
                }
            } else if (pkt.type == EOT) { // 받은 패킷이 EOT 패킷이면 종료료
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
### 사용법
./receiver <수신포트> <데이터손실확률>

<수신포트>: 데이터를 수신하는 포트 번호입니다.
<데이터손실확률>: 데이터를 드랍할 확률입니다.

packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

### 프로그램 작동순서
1. 데이터를 송신하는 프로그램에서 전송된 데이터 패킷을 수신합니다.
2. 수신된 패킷의 시퀀스 번호를 확인하여 순서대로 수신하는지 확인합니다.
3. 데이터를 드랍할 확률에 따라 일부 패킷을 드랍하고, 나머지는 파일에 쓰여집니다.
4. 올바른 시퀀스 번호를 가진 데이터 패킷에 대해 ACK(확인)을 송신하는 패킷을 전송합니다.
5. 파일 전송이 완료되면, 특별한 종료 패킷(EOT)을 수신하여 프로그램을 종료합니다.


#### 컴파일 화면
https://github.com/torosa1/project2/assets/165176275/16523009-165b-4481-9bda-f60c05978188

#### 송신자 실행 파일
https://github.com/torosa1/project2/assets/165176275/9146a915-dd51-45cc-9a01-c3128b51a81a

#### 수신자 실행 파일
https://github.com/torosa1/project2/assets/165176275/c8456fea-f648-466a-b823-0a4c534b21e4

#### 참고 사이트

https://www.crocus.co.kr/461

https://surgach.tistory.com/31

https://blog.naver.com/sdug12051205/221057274601

