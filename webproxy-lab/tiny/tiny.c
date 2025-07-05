/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/// @brief Tiny server 메인 함수 -> 클라이언트로 부터 연결을 받아 요청 처리
/// @param argc 명령행 인자 개수
/// @param argv 명령행 인자 배열 
/// @return 성공 시 0, 실패 시 종료
int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 명령행 인자 확인
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1); // 인자가 부족하면 종료
  }

  // 서버 소켓을 열어 지정된 포트 번호에서 클라이언트의 연결을 기다릴 수 있게 설정
  listenfd = Open_listenfd(argv[1]);

  // 클라이언트 연결을 계속해서 받아 처리
  while (1)
  {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체 크기 초기화
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라리언트의 연결을 수락하고 새 연결 소켓 생성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 접속한 클라이언트 정보 확인
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 접속한 클라이언트 정보 출력
    doit(connfd); 
    Close(connfd); // 요청 처리가 끝난 후 연결 종료
  }
}

/// @brief 클라이언트의 HTTP 요청을 처리하는 함수
/// @param fd 클라이언트와 연결된 소켓 디스크립터
void doit(int fd)
{
  int is_static; // 정적 컨텐츠인지 동적 컨텐츠인지 구분 플래그
  struct stat sbuf; // 파일 정보 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청 헤더, HTTP 메소드, uri, HTTP 버전 저장
  char filename[MAXLINE], cgiargs[MAXLINE]; // 파일 경로 및 CGI 인자 저장
  rio_t rio; // RIO 위한 구조체 

  Rio_readinitb(&rio, fd); // RIO 초기화
  rio_readlineb(&rio, buf, MAXLINE); // 라인을 읽어 buf에 저장
  printf("Request headers: \n"); 
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인을 파싱해서 저장

  // 지원하지 않는 메서드일 경우 
  if (strcasecmp(method, "GET"))
  {
    // 501 에러 응답 반환
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  
  // 헤더를 읽고 출력하는 함수
  read_requesthdrs(&rio);
  // URI를 확인하여 filename과 CGI 인자 분리, 정적 / 동적 여부 판단
  is_static = parse_uri(uri, filename, cgiargs);

  // 해당 파일이 존재하는지 확인 
  if (stat(filename, &sbuf) < 0)
  {
    // 404 에러 응답 반환
    clienterror(fd, filename, "404", "not found", "Tiny couldn't find this file");
    return;
  }

  // 정적 컨텐츠 요청인 경우
  if (is_static)
  {
    // 파일이 일반 파일이 아니거나 읽기 권한이 없는 경우 
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      // 403 에러 응답 반환
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }

    // 정적 파일을 클라이언트에게 전송
    serve_static(fd, filename, sbuf.st_size);
  }

  // 동적 컨텐츠 요청인 경우
  else
  {
    // 파일이 일반 파일이 아니거나 실행 권한이 없는 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      // 403 에러 응답 반환
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }

    // CGI 프로그램 실행하여 결과 전송
    serve_dynamic(fd, filename, cgiargs);
  }
}

/// @brief 클라이언트에게 HTTP 오류 메시지를 HTML 형식으로 전송
/// @param fd 클라이언트와 연결된 소켓 디스크립터
/// @param cause 에러 유발한 요청
/// @param errnum 상태 코드 문자열
/// @param shortmsg 간단한 에러 메세지
/// @param longmsg 상세한 에러 메세지
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF]; // HTTP 응답 헤더 및 본문 저장할 버퍼

  // HTML 형식의 에러 응답 본문 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);        // 흰 배경
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);         // 상태 코드, 메세지
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);        // 에러 원인
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 서버 서명

  // HTTP 상태 줄 작성
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // 응답 헤더 작성
  printf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 응답 헤더 작성
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // HTML 형식 본문 작성
  Rio_writen(fd, body, strlen(body));
}

/// @brief 요청 헤더를 한 줄씩 읽어 출력하는 함수 (첫 헤더 출력x)
/// @param rp RIO 버퍼 구조체 포인터
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE]; // 읽은 데이터를 저장할 버퍼

  rio_readlineb(rp, buf, MAXLINE); // 첫 헤더 라인 읽기

  while (strcmp(buf, "\r\n")) // 빈 줄이 나올 때까지 반복
  {
    rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 라인 읽기
    printf("%s", buf); // 읽은 헤더 라인 출력
  }
}

/// @brief URI를 파싱하여 정적 / 동적 요청 구분 및 파일 이름과 CGI 인자 분리
/// @param uri 요청 받은 URI 문자열
/// @param filename 결과롤 생성될 파일 경로 버퍼
/// @param cgiargs CGI 프로그램에 전달할 인자 버퍼
/// @return 정적 컨텐츠 1, 동적 컨텐츠 0 반환
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // URI에 "cgi-bin" 문자열이 없으면 정적 컨텐츠 요청
  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");   // CGI인자 없음
    strcpy(filename, "."); // 파일 경로 초기화 (현재 디렉토리 기준)
    strcat(filename, uri); // URI 파일 경로에 붙임

    // URI가 / 로 끝나면 기본 파일명 "home.html" 추가
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");

    return 1;
  }
  // URI에 "cgi-bin" 문자열이 있다면 동적 컨텐츠 요청
  else
  {
    ptr = index(uri, '?'); // ? 문자 위치 찾기

    if (ptr) // ? 가 있다면
    {
      strcpy(cgiargs, ptr + 1); // ? 뒤 인자 복사
      *ptr = '\0'; // ? 문자르 \0로 바꿔서 URI를 자른다.
    }
    else // ? 가 없다면
    {
      strcpy(cgiargs, ""); // 빈 문자열로 설정
    }    

    strcpy(filename, "."); // 파일 경로 초기화
    strcat(filename, uri); // 남은 URI를 경로에 붙인다.
    return 0;
  }
}

/// @brief 클라이언트에게 정적 파일을 HTTP 응답으로 보내는 함수
/// @param fd 클라이언트와 연결된 파일 디스크립터
/// @param filename 전송할 파일 이름
/// @param filesize 전송할 파일 크기
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;  // 파일 디스크립터
  // 메모리에 매핑된 파일 포인터, MIME Typm 저장용 버퍼, HTTP 응답 헤더 작성 버퍼
  char *srcp, filetype[MAXLINE], buf[MAXBUF]; 
  
  // 파일 확장자에 따른 콘텐츠 타입 설정
  get_filetype(filename, filetype);

  // HTTP 응답 헤더 작성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                  // 상태 라인
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);   // 서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf);         // 연결 종료 알림
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  // 본문 길이
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);// 콘텐츠 타입 및 헤더 끝
  
  // 클라이언트에 헤더 전송
  Rio_writen(fd, buf, strlen(buf)); 

  srcfd = Open(filename, O_RDONLY, 0); // 파일을 읽기 전용으로 오픈
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 메핑
  Close(srcfd); // 파일 디스크립터 닫기

  Rio_writen(fd, srcp, filesize); // 매핑된 파일 내용을 클라이언트에 전송
  Munmap(srcp, filesize); // 메모리 매핑 해제
}

/// @brief CGI 프로그램을 실행하여 동적 콘텐츠를 클라이언트에게 전송하는 함수
/// @param fd 클라이언트와 연결된 파일 디스크립터
/// @param filename 실행할 CGI 프로그램 경로
/// @param cgiargs CGI 인자
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  // HTTP 응답 헤더 버퍼, 인자가 없는 execve 인자 배열
  char buf[MAXLINE], *emptylist[] = { NULL }; 

  // HTTP 응답 헤더 작성 및 전송
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 상태 라인
  Rio_writen(fd, buf, strlen(buf));    // 클라이언트로 전송
  sprintf(buf, "Server: Tiny Web Server\r\n");  // 서버 정보
  Rio_writen(fd, buf, strlen(buf));             // 클라이언트로 전송

  // 자식 프로세스 생성
  if (Fork() == 0) // 자식 프로세스만 실행
  {
    setenv("QUERY_STRING", cgiargs, 1); // CGI 인자를 환경 변수로 설정
    Dup2(fd, STDOUT_FILENO);            // 표준 출력을 클라이언트 소켓으로 리다이렉션
    Execve(filename, emptylist, environ); // CGI 프로그램 실행 (환경 포함)
  }

  // 부모 프로세스는 자식의 종료를 기다림 (좀비 프로세스 방지)
  Wait(NULL);
}

/// @brief 파일 이름을 검사하여 해당 파일의 MIME Type을 결정
/// @param filename 파일 이름 문자열
/// @param filetype 결과로 저장할 MIME Type 문자열 버퍼
void get_filetype(char *filename, char *filetype)
{
  // .html 있다면 -> MIME Type을 text/html로 설정
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
      // .gif 있다면 -> MIME Type을 image/gif로 설정
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
      // .png 있다면 -> MIME Type을 image/png로 설정
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
      // .jpg 있다면 -> MIME Type을 image/jpeg로 설정
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
      // 위에 해당하지 않다면 모든 파일을 text/plain으로 설정
  else
    strcpy(filetype, "text/plain");
}