#include "csapp.h"

void echo(int connfd);

/// @brief echo server program. 지정한 포트에서 클라이언트 연결을 받아 서비스를 제공
/// @param argc 명령행 인자의 개수
/// @param argv 명령행 인자 배열 [1] = 서버가 바인딩할 포트 번호
/// @return 
int main(int argc, char **argv)
{
    int listenfd, connfd; // 서버의 리스닝 소켓, 클라이언트와 연결된 소켓
    socklen_t clientlen;  // 클라이언트 주소 구조체 크기
    struct sockaddr_storage clientaddr; // 클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE]; // 클라이언트 호스트 이름과 포트 문자열 저장용

    // 명령행 인자 개수 체크: 포트 번호가 있어야 함
    if (argc != 2)
    {
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0);
    }

    // Open_listenfd() : 
    // 지정된 포트에 대해 서버 소켓(리스닝) 생성 및 바인딩 후 리스닝 시작
    listenfd = Open_listenfd(argv[1]);

    // 클라이언트 연결을 계속해서 기다리고 처리
    while (1)
    {
        // 클라이언트 주소 구조체 크기 초기화
        clientlen = sizeof(struct sockaddr_storage);

        // Accept(): 클라이언트의 연결 요청을 받아 새로운 연결 소켓(connfd)해서 반환
        //           clientaddr에 클라이언트 주소 저장, clientlen은 주소 크기 저장
        // (SA *) : struct sockaddr* 로 형변환
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        // Getnameinfo() : 클라이언트 주소를 호스트명과 서비스 포트 문자열로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        // 연결된 클라이언트 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        // echo 호출 : 클라이언트와 데이터를 주고받으며 echo 서비스 수행
        echo(connfd);
        // 연결 소켓 닫기
        close(connfd);
    }

    // 정상 종료
    exit(0);
}