#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE_BLOCK 10

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void proxy(int fd);
void build_http_request(char *http_request, char *hostname, char *path, rio_t *client_rio);
int parse_uri(const char *uri, char *hostname, char *path, char *port);
void *thread(void *vargp);
void cache_insert(char *uri, char *buf, size_t size);
void cache_update_lru(int index);
int cache_find(char *uri, char *buf, size_t * size);
void cache_init();

// 개별 캐시 블록을 나타내는 구조채
typedef struct {
  char uri[MAXLINE]; // 요청된 객체의 URI
  char buf[MAX_OBJECT_SIZE]; // 캐시된 실제 객체 데이터 
  size_t size; // 객체의 크기
  int used; // 해당 캐시 블록이 현재 사용중인 여부 (1 사용, 0 사용 안함)
  int lru; // LRU (least Recenyly Used) 번호 -> 작을 수록 최근에 사용
} cache_block;

// 전체 캐시를 나타내는 구조체
typedef struct {
  cache_block blocks[MAX_CACHE_BLOCK]; // 여러 개의 캐시 블록 배열
  size_t total_size; // 현재 캐시에 저장된 총 객체 크기
  pthread_rwlock_t lock; // 캐시 접근을 위한 읽기-쓰기 락 (동시성 제어)
} cache_t;

cache_t cache;


// /// @brief main 함수 서버 소켕 열고 클라이언트 연결을 받아 proxy 함수로 처리
// /// @param argc 인자 개수 
// /// @param argv 인자 배열
// /// @return 성공 시 0, 실패 시 종료
// int main(int argc, char **argv)
// {
//   int listenfd, connfd;
//   char hostname[MAXLINE], port[MAXLINE];
//   socklen_t clientlen;
//   struct sockaddr_storage clientaddr;
//
//   // 인자 개수 확안
//   if (argc != 2)
//   {
//     fprintf(stderr, "usage: %s <p.ort>\n", argv[0]);
//     exit(1);
//   }
//
//   // 지정 포트 번호로 리슨 소켓 생성 및 초기화
//   listenfd = Open_listenfd(argv[1]);
//
//   while (1)
//   {
//     clientlen = sizeof(clientaddr); // 클라이언트 주소 초기화
//     connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 클라이언트 연결 대기 및 수락, 연결 소켓 생성
//     Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 연결된 클라이언트의 호스트명과 호스트번호 얻기
//     printf("Acceped connection form (%s, %s)\n", hostname, port);
//     proxy(connfd);
//     Close(connfd);
//   }
// }

int main(int argc, char **argv)
{
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid; // 생성할 새 쓰레드 ID 저장할 변수


  // 인자 개수 확안
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  cache_init();
  // 지정 포트 번호로 리슨 소켓 생성 및 초기화
  listenfd = Open_listenfd(argv[1]);
  
  while (1)
  {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 초기화
    connfd = malloc(sizeof(int)); // 파일 스크립터파일 동적 할당 ++
    *connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 클라이언트 연결 대기 및 수락, 연결 소켓 생성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 연결된 클라이언트의 호스트명과 호스트번호 얻기
    printf("Acceped connection form (%s, %s)\n", hostname, port);

    // &tid : 생성된 쓰레드 ID 저장할 변수 포인터
    // NULL : 기본 속성
    // thread : 새 스레드가 실행할 함수 포인터
    // connfd : 함수에 넘겨줄 인자 (파일 디스크립터)
    pthread_create(&tid, NULL, thread, connfd);
  }
}

/// @brief proxy 함수 클라이언트로 요청을 받아 서버에 전달, 서버의 응답을 다시 클라이언트에게 전달
/// @param fd 클라이언트와 연결된 파일 디스크럽터
void proxy(int fd)
{
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 요청과 관련된 정보 저장 버퍼
  char hostname[MAXLINE], path[MAXLINE], port[MAXLINE] = "80"; // 기본 포트 80
  char http_request[MAXLINE]; // 서버에 보낼 HTTP 요청 메시지 저장할 버퍼
  int serverfd; // 서버와 연결할 소켓 디스크립터
  rio_t client_rio, server_rio; // 클라이언트, 서버 RIO
  char cache_buf[MAX_OBJECT_SIZE];
  size_t cache_size = 0;


  Rio_readinitb(&client_rio, fd); // 클라이언트와의 연결을 RIO 버퍼로 초기화
  Rio_readlineb(&client_rio, buf, MAXLINE); // 클라리언트로부터 요청 라인을 읽어옴
  printf("Request header: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // 요청 라인 파싱



  if (strcasecmp(method, "GET")) // GET이 아니라면
  {
    sprintf(buf, "Proxy does not implement this method: %s\r\n", method);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }

  if (parse_uri(uri, hostname, path, port) < 0) // URI를 파싱
  {
    sprintf(buf, "Proxy could not parse URI: %s\r\n", uri);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }
  
  if (cache_find(uri, cache_buf, &cache_size) == 0)
  {
    Rio_writen(fd, cache_buf, cache_size);
    return;
  }

  // 서버에 보낼 HTTP 요청 메시지 생성
  build_http_request(http_request, hostname, path, &client_rio);

  if ((serverfd = Open_clientfd(hostname, port)) < 0) // 서버에 연결 시도
  {
    sprintf(buf, "Connection failed to %s:%s\r\n", hostname, port);
    Rio_writen(fd, buf, strlen(buf)); // 실패 시 에러
    return;
  }

  Rio_readinitb(&server_rio, serverfd); // 서버와 연결을 위한 RIO 초기화
  Rio_writen(serverfd, http_request, strlen(http_request)); // 생성한 요청 메시지를 서버에 전송

  size_t n;
  size_t object_len = 0;
  char object_buf[MAX_OBJECT_SIZE];

  // 서버로부터 한 줄씩 응답을 읽어 buf에 저장 클라이언트로 전달
  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) 
  {
    Rio_writen(fd, buf, n); // 응답의 각 줄을 클라이언트로 전달

    if (object_len + n <= MAX_OBJECT_SIZE)
    {
      memcpy(object_buf + object_len, buf, n);
      object_len += n;
    }
    else
    {
      object_len = MAX_OBJECT_SIZE + 1;
    }
  }

  if (object_len <= MAX_OBJECT_SIZE)
    cache_insert(uri, object_buf, object_len);
    
  Close(serverfd);
}

/// @brief 클라이언트 요청을 읽어 서버로 보낼 HTTP 요청 메시지 생성
/// @param http_request 새로 만들 HTTP 요청 메시지 저장할 버퍼
/// @param hostname 요청할 서버의 호스트 이름
/// @param path 요청할 리소스 경로
/// @param client_rio 클라이언트 연결의 RIO 
void build_http_request(char *http_request, char *hostname, char *path, rio_t *client_rio)
{
  char buf[MAXLINE]; // 클라이언트 요청 헤더 한 줄을 읽어 저장할 버퍼

  sprintf(http_request, "GET %s HTTP/1.0\r\n", path);                     // 요청 라인 생성 -> 명시적으로 1.1에서 1.0으로 변환 
  // 필수 헤더 4개 명시적으로 포함
  sprintf(http_request + strlen(http_request), "Host: %s\r\n", hostname); // 필수 Host 헤더 추가
  sprintf(http_request + strlen(http_request), "%s", user_agent_hdr);     // user_agent_hdr 헤더 추가
  sprintf(http_request + strlen(http_request), "Connection: close\r\n");  // Connection
  sprintf(http_request + strlen(http_request), "Proxy-Connection: close\r\n"); // Proxy-connection 

  // 클라이언트가 보낸 나머지 헤더들을 한 줄씩 읽기
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    // 빈 줄이면 반복 종료
    if (strcmp(buf, "\r\n") == 0)
      break;
    
    // 이미 넣은 헤더들은 건너뜀 -> 클라이언트가 보낸 나머지 헤더는 그대로 전달
        if (!strncasecmp(buf, "Host:", 5) ||
            !strncasecmp(buf, "User-Agent:", 11) ||
            !strncasecmp(buf, "Connection:", 11) ||
            !strncasecmp(buf, "Proxy-Connection:", 17))
            continue;
        
        // 나머지 헤더는 그대로 요청 메세지에 추가
        strcat(http_request, buf);
  }
  
  // 끝을 알리기 위해 빈 줄 추가
  strcat(http_request, "\r\n");
}

/// @brief URI 문자열 파싱하여 정보를 분리하는 함수
/// @param uri 파싱할 uri 문자열
/// @param hostname 파싱된 호스트명 저장 버퍼
/// @param path 파싱된 경로 저장 버퍼
/// @param port 파싱된 포트 저장 버퍼
/// @return 성공 시 0, 실패 시 -1
int parse_uri(const char *uri, char *hostname, char *path, char *port)
{
    char uri_copy[MAXLINE];
    strcpy(uri_copy, uri);

    if (strncasecmp(uri_copy, "http://", 7) != 0)
        return -1;

    char *hostbegin = uri_copy + 7;
    char *pathbegin = strchr(hostbegin, '/');
    char *portbegin = strchr(hostbegin, ':');

    if (pathbegin) 
    {
        strcpy(path, pathbegin);
        *pathbegin = '\0';
    } 
    else 
    {
        strcpy(path, "/");
    }

    
    if (portbegin) 
    {
        *portbegin = '\0';
        strcpy(hostname, hostbegin);
        strcpy(port, portbegin + 1);
    } 
    else 
    {
        strcpy(hostname, hostbegin);
        strcpy(port, "80");
    }

    return 0;
}

/// @brief 클라이언트 연결을 처리하는 스레드 함수
/// @param vargp 파일 디스크립터 주소 
/// @return NULL
void *thread(void *vargp)
{
  int connfd = *((int *)vargp); // 인자로 받은 void*를 int*로 캐시팅 후 역참조하여 실제 connfd 값
  // Pthread_detach() : 종료 시 자동으로 자원 해제
  // pthread_self() : 현재 스레드 ID 반환
  Pthread_detach(pthread_self()); // 현재 쓰레드를 종료시 자동 자원 해제 
  Free(vargp); // 동적 할당된 connfd 포인터 메모리 해제
  proxy(connfd); // 클라이언터 연결
  Close(connfd); // 클라이언트와 연결된 소켓 닫기
  return NULL;
}

/// @brief 캐시 초기화 함수
void cache_init() 
{
  cache.total_size = 0; // 전채 캐시 크기 0
  pthread_rwlock_init(&cache.lock, NULL); // 읽기 쓰기 락 초기화

  for (int i = 0; i < MAX_CACHE_BLOCK; i++) //각 캐시 블록 초기화
    cache.blocks[i].used = 0;
}

/// @brief 캐시에 해당 URI 존재하는지 확인
/// @param uri 요청된 URI
/// @param buf 캐시된 객체 데이터 복사할 버퍼
/// @param size 복사된 객체으 크기 저장 변수
/// @return hit 0, miss -1
int cache_find(char *uri, char *buf, size_t *size)
{
  int found = -1; 
  pthread_rwlock_rdlock(&cache.lock); // 읽기 락 획득 -> 여러 스레드가 동시에 캐시를 읽을 수 있도록 허용
  
  // 캐시 블록들을 순회하면서 URI가 일치하는 항목 탐색
  for (int i = 0; i < MAX_CACHE_BLOCK; i++)
  {
    // 해당 블록이 사용중이며 URI가 일치하는 경우
    if (cache.blocks[i].used && strcmp(cache.blocks[i].uri, uri) == 0)
    {
      // 캐시된 데이터를 요청한 버퍼로 복사
      memcpy(buf, cache.blocks[i].buf, cache.blocks[i].size);
      // 해당 개게의 크기를 반환할 포인터에 저장
      *size = cache.blocks[i].size;
      // LRU 갱신 -> 해당 블록이 최근에 사용되었음을 기록
      cache_update_lru(i);
      found = 0; // hit
      break;
    }
  }

  pthread_rwlock_unlock(&cache.lock); // -> 읽기 락 해제
  return found; // hit 0, miss -1
}

/// @brief LRU 정책을 기반으로 캐시 블록의 사용 순서 갱신
/// @param index 최근에 접근한 캐시 블록의 인덱스
void cache_update_lru(int index)
{
  // 현재 접근한 블록의 기존 LRU 값 저장
  int old = cache.blocks[index].lru;

  // 모든 캐시 블록을 순회
  for (int i = 0; i < MAX_CACHE_BLOCK; i++)
  {
    // 사용 중인 블록 중에서 접근한 블록보다 이전에 사용된 블록 (LRU 값이 더 작은 경우)
    if (cache.blocks[i].used && cache.blocks[i].lru < old)
      cache.blocks[i].lru++; // 이 블록의 LRU값을 증가시켜 덜 최근에 사용 되었음을 표시
  }
  
  // 접근한 블록의 LRU 값을 0으로 설정 -> 가장 최근에 사용된 블록
  cache.blocks[index].lru = 0;
}

/// @brief 새로운 데이터를 캐시에 삽입
/// @param uri 요청된 객체의 URI
/// @param buf 객체 버퍼
/// @param size 객체 데이터 크기
void cache_insert(char *uri, char *buf, size_t size)
{
  // 객체 크기가 너무 크면 리턴 -> 예외처리
  if (size > MAX_OBJECT_SIZE)
    return;
  
  // 쓰기 락 획득 (다른 쓰기 / 읽기 차단)
  pthread_rwlock_wrlock(&cache.lock);

  int evict_index = -1; // 데이터를 삽입할 블록의 인덱스
  int max_lru = -1; // 가장 오래된 블록을 찾기 위한 변수

  // 캐시 블록 순회
  for (int i = 0; i < MAX_CACHE_BLOCK; i++)
  {
    // 캐시 블록 중 사용되지 않은 블록을 찾기
    if (!cache.blocks[i].used)
    {
      evict_index = i;
      break;
    }

    // 사용 중인 블록 중 가장 오래된 블록 찾기 (LRU 값이 가장 큰 값)
    if (cache.blocks[i].lru > max_lru)
    {
      max_lru = cache.blocks[i].lru;
      evict_index = i;
    }
  }

  // 삽입할 블록을 찾지 못하면
  if (evict_index == -1)
  {
    pthread_rwlock_unlock(&cache.lock); // 락 풀고 종료
    return;
  }

  // 기존 사용 중인 블록이라면, 총 캐시 크기에서 제거
  if (cache.blocks[evict_index].used)
    cache.total_size -= cache.blocks[evict_index].size;

  // 선택된 블록에 새 데이터 저장
  cache.blocks[evict_index].used = 1;
  strcpy(cache.blocks[evict_index].uri, uri);       // URI 저장
  memcpy(cache.blocks[evict_index].buf, buf, size); // 데이터 복사
  cache.blocks[evict_index].size = size;            // 크기 저장
  cache_update_lru(evict_index);                    // LRU 갱신
  cache.total_size += size;                         // 총 캐시 크기 증가

  // 총 캐시 크기가 허용 크기를 넘으면 오래된 블록부터 제거
  while (cache.total_size > MAX_CACHE_SIZE)
  {
    int del_index = -1;
    int max_lru = -1;

    // 가장 오래된 블록 찾기
    for (int i = 0; i < MAX_CACHE_BLOCK; i++)
    {
      if (cache.blocks[i].used && cache.blocks[i].lru > max_lru)
      {
        max_lru = cache.blocks[i].lru;
        del_index = i;
      }
    }
    
    // 삭제할 블록이 없으면 종료
    if (del_index == -1)
      break;
    
    // 선택된 블록 제거
    cache.total_size -= cache.blocks[del_index].size;
    cache.blocks[del_index].used = 0;
  }

  // 쓰기 락 해제
  pthread_rwlock_unlock(&cache.lock);
}