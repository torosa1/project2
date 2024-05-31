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
    serialize_packet(&ack_pkt, buffer);
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
            deserialize_packet(buffer, &pkt);

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
