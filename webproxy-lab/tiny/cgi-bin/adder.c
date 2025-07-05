/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

/// @brief 두 개의 정수를 더하는 CGI Program
int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // QUERY_STRING 환경 변수에서 두 개의 인자를 추출
  // getenv(): 환경 변수의 값을 가져오는 역할
  // ex. http://example.com/cgi-bin/adder?n1=3&n2=5
  // buf = "n1=3&n2=5"
  // 문자열을 파싱
  if ((buf = getenv("QUERY_STRING")) != NULL) 
  {
    p = strchr(buf, '&');   // & 문자를 기준으로 두 인자를 분리
    *p = '\0';              // & 를 null 문자로 바꿔 첫 번째 인자를 끝냄
    strcpy(arg1, buf);      // arg1 = n1=3
    strcpy(arg2, p + 1);    // arg2 = n2=5

    // = 다음의 문자열을 정수로 변환하여 n1, n2에 저장
    n1 = atoi(strchr(arg1, '=') + 1); // n1 = 3
    n2 = atoi(strchr(arg2, '=') + 1); // n2 = 5
  }

  // HTML 본문에 출력할 내용을 작성
  sprintf(content, "QUERY_STRING=%s\r\n<p>", buf); // 원래의 QUERY_STRING 출력
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  // HTTP 응답 헤더 및 본문 출력
  // printf() -> 출력 버퍼에 데이터를 저장
  // fflush(stdout) -> 버퍼에 있는 내용을 화면(서버)에 내보내고 버퍼를 비움
  printf("Content-type: text/html\r\n"); // content 타입은 HTML
  printf("Content-length: %d\r\n", (int)strlen(content)); // content 길이 출력
  printf("\r\n"); // 헤더 종료
  printf("%s", content); // 실제 HTML content 출력
  fflush(stdout); // 출력(내보냄) 및 비움

  exit(0);
}
// 동작 흐름
// 1. 요청 받은 거를 분리
// 2. sprintf()로 content에 출력할 HTML을 작성
// 3. printf()로 버퍼를 채운다
// 4. fflush(stdout) 쌓인 버퍼를 출력하고 fflush로 버퍼를 비운다.