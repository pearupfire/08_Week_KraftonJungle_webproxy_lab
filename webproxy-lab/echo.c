#include "csapp.h"

/// @brief 클라이언트로 데이터를 받아 그대로 다시 보내는 echo 함수
/// @param connfd 클라이언트와 연결된 소켓 디스크립터
void echo(int connfd)
{
    size_t n; // 읽은 바이트 수
    char buf[MAXLINE]; // 수신 및 송신 버퍼
    rio_t rio; // RIO 버퍼 구조체

    // 소켓 디스크립터 RIO 버퍼 초기화
    Rio_readinitb(&rio, connfd); 

    // 클라이언트로부터 한 줄씩 읽고, 그대로 다시 전송
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        // 수신한 바이트 출력
        printf("server received %d bytes \n", (int)n);
        // 받은 대아토룰 그대로 클라리언트에 전송
        Rio_writen(connfd, buf, n);
    }
}