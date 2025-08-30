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
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);
    Close(connfd);
  }
}

void doit(int fd)
{
  int is_static;    // 요청이 정적 콘텐츠인지 동적 콘텐츠인지 구별하기 위한 플래그
  struct stat sbuf; // 파일의 정보를 담기 위한 구조체 (크기, 권한 등)
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
   
  /* 요청 line과 헤더 읽기 */
  Rio_readinitb(&rio, fd);
  // rio 버퍼를 통해 요청 라인 한 줄 (e.g. "GET /cgi-bin/adder?1&2 HTTP/1.0")을 읽어옴
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  // 읽어온 요청 라인을 공백 기준으로 쪼개서 method(GET), uri(/cgi-bin/adder?1&2), version(HTTP/1.0) 변수에 각각 저장
  sscanf(buf, "%s %s %s", method, uri, version);

  // tiny는 GET 메서드만 지원하므로, 다른 메서드(POST 등)가 오면 에러 처리
  if (strcasecmp(method, "GET")) { // strcasecmp는 대소문자 구분 없이 비교. 같으면 0을 반환
    clienterror(fd, method, "501", "NOT implemented", "Tiny does not implement this method");
    return;
  }
  // 요청 라인 다음에 오는 나머지 요청 헤더들(User-Agent 등)을 읽고 무시함(빈 줄이 나올 때까지 while 문으로 다 읽어냄)
  read_requesthdrs(&rio);

  /* 2. URI를 파싱하여 파일 이름과 CGI 인자 추출 */
  // uri를 분석해서 정적(static) 콘텐츠인지 동적(dynamic) 콘텐츠인지 판단
  // uri에 cgi-bin이라는 문자열이 포함되어 있으면 동적 콘텐츠로 판단
  // 정적 콘텐츠일 경우: filename에 파일 경로(e.g. ./home.html)를 저장
  // 동적 콘텐츠일 경우: filename에 CGI 프로그램 경로(예: ./cgi-bin/adder)를, cgiargs에는 CGI 프로그램에 넘겨줄 인자(예: 1&2)를 저장
  is_static = parse_uri(uri, filename, cgiargs);

  // 파일이 존재하지 않거나 접근할 수 없으면 에러를 발생시키고 종료
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  
  /* 3. 콘텐츠 종류에 따라 정적/동적 서버에 처리 위임 */
  // S_IRUSR: Stat Is Readable by USeR (파일 소유자가 읽을 수 있는 권한)
  // S_IXUSR: Stat Is eXecutable by USeR (파일 소유자가 실행할 수 있는 권한)
  // sbuf.st_mode: 파일의 종류와 권한 정보가 비트 마스크 형태로 한꺼번에 저장되어 있음
  if (is_static) { /* 정적 콘텐츠(Static content) 제공 */
    // 파일이 일반 파일(regular file)이 아니거나, 읽기 권한(S_IRUSR)이 없는 경우 '403 Forbidden' 에러 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else { /* 동적 콘텐츠(Dynamic content) 제공 */
    // 파일이 일반 파일이 아니거나, 실행 권한(S_IXUSR)이 없는 경우 '403 Forbidden' 에러 처리
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/*
 * clienterror - 클라이언트에게 HTTP 에러 메시지를 전송 (안전한 snprintf 사용)
 * cause: 에러 원인 (보통 파일 이름)
 * errnum: HTTP 상태 코드 번호 (e.g. "404")
 * shortmsg: 상태 코드에 대한 짧은 메시지 (e.g. "Not Found")
 * longmsg: 브라우저에 표시될 긴 설명 메시지
 */
void clienterror(int fd, char *cause, char *errnum, 
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* 1. HTTP 응답 본문(body)을 snprintf로 안전하게 만들기 */
  // 책에 나온 여러 번의 sprintf 호출을 하나의 안전한 snprintf 호출로 합침
  // MAXBUF 크기를 넘지 않도록 안전하게 문자열을 생성
  snprintf(body, MAXBUF, "<html><title>Tiny Error</title>"
                         "<body bgcolor=""#ffffff"">\r\n"
                         "%s: %s\r\n"
                         "<p>%s: %s\r\n"
                         "<hr><em>The Tiny Web server</em>\r\n",
                         errnum, shortmsg, longmsg, cause);

  /* 2. HTTP 응답 헤더도 snprintf로 안전하게 전송하기 */
  // 각 헤더 라인을 만들 때 버퍼 크기(MAXLINE)를 명시하여 오버플로우를 방지
  
  // 응답 라인
  snprintf(buf, MAXLINE, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  
  // Content-type 헤더
  snprintf(buf, MAXLINE, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // Content-length 헤더와 빈 줄
  snprintf(buf, MAXLINE, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  
  // 응답 본문 전송
  Rio_writen(fd, body, strlen(body));
}