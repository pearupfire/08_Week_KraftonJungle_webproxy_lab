#include "csapp.h"

/// @brief echo client program. 사용자 입력을 서버로 보내고, 서버가 다시 보내준 데이터를 출력
/// @param argc 명령행 인자의 개수 (프로그램 이름 포함)
/// @param argv 명령행 인자 배열. [1] = 서버 호스트 이름, [2] = 포트 번호
/// @return 정상 졸료 시 0 반환
int main(int argc, char **argv)
{
    int clientfd;   // 서버와 연결된 클라이언트 소켓 디스크립터
    char *host, *port, buf[MAXLINE]; // 서버 호스트명과 포트번호, 송수신 데이터 버퍼
    rio_t rio; // Robust I/O 버퍼 구조체

    // 인자 개수가 잘못된 경우
    if (argc != 3)
    {
        fprintf(stderr, "usage : %s <host> <port> \n", argv[0]); // 출력
        exit(0); // 종료
    }

    // argc : 인자의 개수
    // argv[0] : 실행한 프로그램 이름
    // argv[1] : 첫 번쨰 사용자 입력 인자 (호스트 이름)
    // argv[2] : 두 번째 사용자 입력 인자 (포트 번호)
    // 명령행 인자에서 호스트와 포트 추출
    host = argv[1];
    port = argv[2];

    // Open_clientfd(host, port) : 주어진 호스트 주소와 포트 번호를 이용해 TCP로 서버에 연결하고
    //                             연결된 소켓 디스크립터를 반환
    // 서버에 연결된 클라이언트 소켓 열기
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd); // RIO 버퍼를 초기화 (clientfd에 대한 읽기 버퍼 설정)

    // Fgets() : 표준 입력에서 한 줄 읽기 -> EOF나 에러 발생 시 종료
    // 사용자 입력을 받아 서버로 보내고, 서버 응답을 다시 출력
    while (Fgets(buf, MAXLINE, stdin) != NULL)
    {
        // 사용자 입력을 서버에 전송
        Rio_writen(clientfd, buf, strlen(buf));
        // 서버 응답을 읽어 buf에 저장
        Rio_readlineb(&rio, buf, MAXLINE);
        // 서버 응답을 표준 출력에 출력
        Fputs(buf, stdout);
    }
    
    // 클라이언트 소켓 닫기
    Close(clientfd);
    // 정상 종료
    exit(0);
}