/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <strings.h>

#define FILETYPE_MAX 256

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *range);
int parse_uri(char *uri, char *filename, char *cgiargs);
/* 정적 파일 전송: 단일 Range 요청(206/416)과 전체 전송(200)을 모두 처리 */
void serve_static(int fd, char *filename, size_t filesize, char *range,
                  int is_head);
void get_filetype(const char *filename, char *filetype, size_t cap);
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN); // 클라이언트가 먼저 끊어도 프로세스가 안 죽게

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}

void doit(int fd) {
  int is_static; // // 요청이 정적/동적 콘텐츠인지 구별하는 플래그
  struct stat sbuf; // 파일 정보를 담기 위한 구조체 (크기, 권한 등)
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  char range[MAXLINE] = ""; // Range 헤더 값을 저장할 변수 추가 및 초기화
  rio_t rio;

  /* 요청 line과 헤더 읽기 */
  Rio_readinitb(&rio, fd);
  // rio 버퍼를 통해 요청 라인 한 줄 (e.g. "GET /cgi-bin/adder?1&2 HTTP/1.0")을
  // 읽어옴
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;

  printf("Request headers:\n");
  printf("%s", buf);
  // 요청 라인 파싱하여 각각 변수에 담음: method / uri / version
  sscanf(buf, "%s %s %s", method, uri, version);

  // Tiny는 GET와 HEAD만 지원(그 외 메서드는 501)
  // strcasecmp는 대소문자 구분 없이 비교. 같으면 0을 반환
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "NOT implemented",
                "Tiny does not implement this method");
    return;
  }
  int is_head = (strcasecmp(method, "HEAD") == 0); // HEAD 요청이면 1, 아니면 0

  // 나머지 요청 헤더를 읽는다.
  // 여기서 Range 헤더가 있으면 range 버퍼에 저장(그 외는 무시).
  read_requesthdrs(&rio, range);

  /* 2. URI 파싱: 정적 vs 동적, 파일 경로/CGI 인자 추출 */
  is_static = parse_uri(uri, filename, cgiargs);

  // 파일이 존재하지 않거나 접근할 수 없으면 에러를 발생시키고 종료
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }

  /* 3. 콘텐츠 종류에 따라 정적/동적 서버에 처리 위임 */
  // S_IRUSR: Stat Is Readable by USeR (파일 소유자가 읽을 수 있는 권한)
  // S_IXUSR: Stat Is eXecutable by USeR (파일 소유자가 실행할 수 있는 권한)
  // sbuf.st_mode: 파일의 종류와 권한 정보가 비트 마스크 형태로 한꺼번에
  // 저장되어 있음
  if (is_static) { /* 정적 콘텐츠(Static content) 제공 */
    // 파일이 일반 파일(regular file)이 아니거나, 읽기 권한(S_IRUSR)이 없는 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    // 정적 파일 전송: Range(단일 범위)와 전체 전송 모두 지원
    serve_static(fd, filename, sbuf.st_size, range, is_head);
  } else { /* 동적 콘텐츠(Dynamic content) 제공 */
    // 파일이 일반 파일이 아니거나, 실행 권한(S_IXUSR)이 없는 경우 '403 에러'
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, is_head);
  }
}

/*
 * clienterror - 클라이언트에게 HTTP 에러 메시지를 전송 (안전한 snprintf 사용)
 * cause: 에러 원인 (보통 파일 이름)
 * errnum: HTTP 상태 코드 번호 (e.g. "404")
 * shortmsg: 상태 코드에 대한 짧은 메시지 (e.g. "Not Found")
 * longmsg: 브라우저에 표시될 긴 설명 메시지
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* 1) HTML 본문 구성 */
  snprintf(body, sizeof(body),
           "<html><title>Tiny Error</title>"
           "<body bgcolor=\"#ffffff\">\r\n"
           "%s: %s\r\n"
           "<p>%s: %s</p>\r\n"
           "<hr><em>The Tiny Web server</em>\r\n"
           "</body></html>\r\n",
           errnum, shortmsg, longmsg, cause);

  /* 2) 헤더 전송 */
  // 응답 라인
  snprintf(buf, sizeof(buf), "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  // Content-type 헤더
  snprintf(buf, sizeof(buf), "Content-Type: text/html; charset=utf-8\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // Content-length 헤더와 빈 줄
  snprintf(buf, sizeof(buf), "Content-length: %zu\r\n\r\n", strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 응답 본문 전송
  Rio_writen(fd, body, strlen(body));
}

/*
 * read_requesthdrs - 나머지 요청 헤더를 읽는다.
 * - 빈 줄("\r\n")이 나올 때까지 읽고, 로그에 출력
 * - "Range:" 헤더가 있으면 range에 그대로 복사해둔다.
 *   (예: "Range: bytes=0-1023\r\n")
 */
void read_requesthdrs(rio_t *rp, char *range) {
  char buf[MAXLINE];

  while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
    if (!strcmp(buf, "\r\n"))
      break;           // 헤더 끝
    printf("%s", buf); // 로그 출력

    if (strncasecmp(buf, "Range:", 6) == 0) {
      // 전체 라인을 안전하게 저장(나중에 serve_static에서 파싱)
      snprintf(range, MAXLINE, "%s", buf);
    }
  }
  return;
}

/*
 * parse_uri - URI를 파싱해 정적/동적을 구분하고, filename / cgiargs를 채운다.
 * - 정적: filename = "." + uri, 디렉터리면 기본 문서("home.html") 추가
 * - 동적: "?" 뒤를 cgiargs로 분리한 뒤, filename = "." + (경로 부분)
 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
  // strncmp를 사용해 uri가 정확히 "/cgi-bin/"으로 '시작'하는지 검사
  // strstr보다 더 엄격하고 안전한 방법. 9는 "/cgi-bin/"의 길이
  int is_cgi = (strncmp(uri, "/cgi-bin/", 9) == 0);

  // cgiargs를 미리 빈 문자열로 초기화
  cgiargs[0] = '\0';

  if (!is_cgi) { // 정적 콘텐츠 처리
    // 빈 URI 방어
    if (!uri[0])
      uri = "/";

    /* filename = "." + uri */
    snprintf(filename, MAXLINE, ".%s", uri);

    // uri가 디렉터리 경로('/')로 끝나는 경우 기본 문서 붙이기
    size_t len = strlen(filename);
    if (len && filename[len - 1] == '/') {
      // snprintf를 사용해 filename의 끝에 "home.html"을 안전하게 이어붙임
      snprintf(filename + len, // 쓸 위치: 기존 문자열의 끝(NULL 문자 위치)
               MAXLINE - len, // 남은 버퍼 공간 크기
               "home.html");  // 쓸 내용
    }

    return 1; // 정적 콘텐츠이므로 1을 리턴.
  } else {    // 동적 콘텐츠 처리

    // strchr 함수로 '?' 문자가 uri 어디에 있는지 찾음 ('?'는 CGI 인자의 시작을
    // 의미)
    char *q = strchr(uri, '?');

    if (q) { // '?'가 있다면 (CGI 인자가 있다면)
      snprintf(cgiargs, MAXLINE, "%s",
               q + 1); // snprintf로 안전하게 cgiargs를 복사.
      *q = '\0'; // '?' 위치를 NULL 문자로 바꿔서 uri 문자열을 경로 부분에서
                 // 잘라냄.
    }

    // 마찬가지로 "." 뒤에 프로그램 경로를 이어붙여 filename을 만들어 줌
    snprintf(filename, MAXLINE, ".%s", uri);

    return 0; // 동적 콘텐츠이므로 0을 리턴.
  }
}

// serve_static - 정적 파일을 클라이언트에게 전송
void serve_static(int fd, char *filename, size_t filesize, char *range,
                  int is_head) {
  int srcfd; // 요청된 파일을 가리킬 파일 디스크립터
  char buf[MAXBUF];
  char filetype[FILETYPE_MAX];
  long start = 0,
       end = (filesize > 0) ? (long)filesize - 1 : 0; // 기본값은 파일 전체
  int partial = 0;

  // Range 헤더가 있는지, 그리고 그 형식이 맞는지 확인
  // ("Range: bytes=START-END")
  if (range && range[0] != '\0') {
    long s = 0, e = -1;
    // "Range: bytes=START-END" 또는 "Range: bytes=START-" 를 파싱
    int n = sscanf(range, "Range: bytes=%ld-%ld", &s, &e); // "START-"면 n==1
    if (n >= 1 && s >= 0) {
      start = s;
      if (n == 1 || e < 0)
        e = (long)filesize - 1; // 열린 범위 START-
      end = e;
      partial = 1;
    }
  }

  // 유효성/경계 보정 및 416 처리
  if (partial) {
    if (start >= (long)filesize) {
      int m = snprintf(buf, sizeof(buf),
                       "HTTP/1.0 416 Range Not Satisfiable\r\n"
                       "Server: Tiny Web Server\r\n"
                       "Connection: close\r\n"
                       "Content-Range: bytes */%zu\r\n"
                       "Content-Length: 0\r\n\r\n",
                       filesize);
      Rio_writen(fd, buf, (size_t)m);
      return;
    }
    if (end < 0 || end >= (long)filesize)
      end = (long)filesize - 1;
    if (end < start)
      end = (long)filesize - 1; // "START-" 케이스
  }

  long length = (end - start + 1); // 전송할 바이트 수
  size_t tosend = (size_t)length;  // 헤더/복사 루틴에서 쓸 타입

  /* 1. HTTP 응답 헤더(Response headers)를 클라이언트에게 전송 */

  // 파일 이름의 접미사를 보고 파일 타입을 결정 (예: .html -> text/html)
  get_filetype(filename, filetype, sizeof(filetype));

  // filetype은 위에서 cap만큼만 채워졌지만, 포맷 단계에서도 상한을 한 번 더
  int ft_prec = (int)strnlen(filetype, sizeof(filetype) - 1);

  if (partial) {
    int n = snprintf(buf, sizeof(buf),
                     "HTTP/1.0 206 Partial Content\r\n"
                     "Server: Tiny Web Server\r\n"
                     "Connection: close\r\n"
                     "Accept-Ranges: bytes\r\n"
                     "Content-Length: %zu\r\n"
                     "Content-Range: bytes %ld-%ld/%zu\r\n"
                     "Content-Type: %.*s\r\n\r\n",
                     tosend, start, end, filesize, ft_prec, filetype);
    Rio_writen(fd, buf, (size_t)n);
  } else {
    int n = snprintf(buf, sizeof(buf),
                     "HTTP/1.0 200 OK\r\n"
                     "Server: Tiny Web Server\r\n"
                     "Connection: close\r\n"
                     "Accept-Ranges: bytes\r\n"
                     "Content-Length: %zu\r\n"
                     "Content-Type: %.*s\r\n\r\n",
                     filesize, ft_prec, filetype);
    Rio_writen(fd, buf, (size_t)n);
  }
  printf("Response headers:\n%s", buf);

  if (is_head) {
    return;
  }

  /* 2. HTTP 응답 본문(Response body)을 클라이언트에게 전송 */
  srcfd = Open(filename, O_RDONLY, 0); // 요청된 파일을 읽기 전용으로 열기

  // 메모리 효율을 위해 청크 단위 복사
  static const size_t CHUNK = 64 * 1024; // 64KB
  // 큰 파일을 한 번에 malloc(filesize) 하지 않고,
  // 고정 크기 블록으로 처리하니 메모리 사용이 일정하고,
  // 소켓의 백프레셔(상대가 느리면 write가 막힘) 에도 유연하게 대응.
  char *blk = Malloc(CHUNK ? CHUNK : 1);

  // 파일 오프셋을 Range 시작점(start)으로 이동.
  // Range가 없으면 start==0이라 파일 맨 처음으로 이동.
  // off_t 캐스팅은 플랫폼별 타입 일치용.
  Lseek(srcfd, (off_t)start, SEEK_SET);

  // left: 아직 보내야 할 바이트 수(이미 tosend = end - start + 1로 계산해둠).
  size_t left = tosend;

  // 아래의 루프 덕에 큰 파일도 적은 메모리로 안정적으로 스트리밍 가능.
  // 네트워크 지연/혼잡으로 write가 분할되어도 문제 없이 이어서 전송.
  // Range 요청이든 전체 전송이든 같은 루틴으로 처리 가능
  // (시작 오프셋 / 길이만 다름).
  while (left > 0) {
    // want: 이번 회차에 읽고 싶은 최대 바이트 수(CHUNK 또는 남은 양).
    size_t want = (left < CHUNK) ? left : CHUNK;

    // 파일 디스크립터에서 정확히 want바이트를 읽기 위해 반복하는 robust read.
    ssize_t nrd = Rio_readn(srcfd, blk, want);
    if (nrd <= 0)
      break; // EOF/에러 방어

    // 소켓으로 바이트를 정확히 nrd만큼 다 보낼 때까지 반복하는 robust write.
    Rio_writen(fd, blk, (size_t)nrd);

    // 누적 전송량 갱신.left가 0이 되면 목표 길이 전송 완료.
    left -= (size_t)nrd;
  }

  // 메모리 해제 + 파일 디스크립터 닫기로 리소스 정리.
  Free(blk);
  Close(srcfd);
}

/*
 * get_filetype - 파일 이름으로부터 HTTP Content-type을 결정 (안전하고 개선된
 * 버전) filename: 분석할 파일 경로 (const char*로 받아 원본을 수정하지 않겠다는
 * 약속) filetype: 결정된 Content-type 문자열이 저장될 버퍼
 */
void get_filetype(const char *filename, char *filetype, size_t cap) {
  if (cap == 0) // 쓸 공간이 전혀 없으면 그냥 종료
    return;
  filetype[0] = '\0'; // 최소한의 안전 초기화

  // strrchr: 문자열의 '오른쪽 끝에서부터' '.' 문자를 검색하여 포인터를 반환
  // => 파일 확장자를 찾는 가장 정확한 방법
  const char *ext = strrchr(filename, '.');

  // 확장자가 없는 경우 or dotfile(e.g. .env)인 경우
  if (!ext || ext == filename) {
    // 기본값으로 일반 텍스트(UTF-8) 타입을 설정하고 함수 종료.
    snprintf(filetype, cap, "text/plain; charset=utf-8");
    return;
  }
  // 포인터를 1 증가시켜 '.' 바로 다음 문자를 가리키게 함 (실제 확장자의 시작)
  ext++;

  // strcasecmp: 대소문자를 구분하지 않고 문자열을 비교해서 같으면 0 반환
  // e.g. "HTML"과 "html"을 같게 취급
  if (!strcasecmp(ext, "html") || !strcasecmp(ext, "htm"))
    snprintf(filetype, cap, "text/html; charset=utf-8");
  else if (!strcasecmp(ext, "css"))
    snprintf(filetype, cap, "text/css; charset=utf-8");
  else if (!strcasecmp(ext, "js"))
    snprintf(filetype, cap, "application/javascript");
  else if (!strcasecmp(ext, "gif"))
    snprintf(filetype, cap, "image/gif");
  else if (!strcasecmp(ext, "jpg") || !strcasecmp(ext, "jpeg"))
    snprintf(filetype, cap, "image/jpeg");
  else if (!strcasecmp(ext, "png"))
    snprintf(filetype, cap, "image/png");
  else if (!strcasecmp(ext, "mpg") || !strcasecmp(ext, "mpeg"))
    snprintf(filetype, cap, "video/mpeg");
  else if (!strcasecmp(ext, "mp4"))
    snprintf(filetype, cap, "video/mp4");
  else // 위 목록에 없는 모든 다른 확장자들의 경우
       // 일반적인 바이너리 파일 타입을 의미하는 "application/octet-stream"으로
       // 설정
    snprintf(filetype, cap, "application/octet-stream");
}

/*
 * serve_dynamic - CGI 프로그램을 실행하고 그 결과를 클라이언트에게 전송
 * filename: 실행할 CGI 프로그램의 경로
 * cgiargs: CGI 프로그램에 전달할 인자 문자열
 */
void serve_dynamic(int fd, char *filename, char *cgiargs, int is_head) {
  char buf[MAXLINE];
  char *emptylist[] = {NULL}; // CGI 실행을 위한 빈 인자 목록

  /* 1. 성공을 가정하고, HTTP 응답의 첫 부분을 클라이언트에게 먼저 보냄 */
  // CGI 프로그램이 성공적으로 실행될 것을 가정하고, 기본적인 성공 헤더를 먼저
  // 전송. 상태 라인 (200 OK)
  snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  // 서버 정보 헤더
  snprintf(buf, sizeof(buf), "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 만약 HEAD 요청이면, 기본 헤더만 보내고 자식 프로세스를 생성하지 않고 종료.
  if (is_head) {
    return;
  }

  /* 2. 자식 프로세스를 생성하여 CGI 프로그램 실행 */
  if (Fork() == 0) { /* 자식 프로세스(Child)의 코드 블록 */

    // setenv: "QUERY_STRING"이라는 환경 변수를 cgiargs 값으로 설정.
    // CGI 프로그램은 이 환경 변수를 읽어서 인자를 파싱함. (adder.c의 getenv
    // 참고)
    setenv("QUERY_STRING", cgiargs, 1);

    // Dup2: 자식 프로세스의 표준 출력(STDOUT_FILENO, 즉 화면 출력)을
    // 클라이언트 소켓(fd)으로 리다이렉션(redirection)함.
    // 이제부터 자식 프로세스에서 printf로 출력하는 모든 것은 화면이 아닌
    // 클라이언트에게 전송됨.
    Dup2(fd, STDOUT_FILENO);

    // Execve: 현재 실행 중인 자식 프로세스(tiny의 복사본)를
    // filename에 지정된 새로운 CGI 프로그램으로 완전히 교체하여 실행.
    // 성공하면 이 함수는 절대 리턴되지 않음.
    Execve(filename, emptylist, environ);
  }

  // Fork()의 리턴 값이 0이 아닌 경우 -> 부모 프로세스(Parent)의 코드
  // Wait: 부모 프로세스는 자식 프로세스가 종료될 때까지 여기서 기다림.
  // 자식이 끝난 후 그 자원을 정리(reap)하여 좀비 프로세스가 되는 것을 방지.
  Wait(NULL); /* Parent waits for and reaps child */
}