#include "csapp.h"
#include "echo.c"

// echo.c에 구현된 함수의 프로토타입만 선언하여 컴파일러에게 미리 알려줌
void echo(int connfd);

int main(int argc, char **argv) {
  int listenfd, connfd;
  socklen_t clientlen;

  // sockaddr_storage: 어떤 형태의 주소(IPv4, IPv6 등)든 충분히 담을 수 있는 큰
  // 구조체
  struct sockaddr_storage clientaddr;
  char client_hostname[MAXLINE],
      client_port[MAXLINE]; // getnameinfo 결과 문자열을 담을 버퍼

  // 프로그램 실행 시 인자가 2개인지 확인 (./echoserver <port>)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  /*
   * 1. 듣기 소켓을 생성하고 준비시킴
   *    Open_listenfd는 내부적으로 socket(), bind(), listen() 시스템 콜을 차례로
   * 실행해줌 argv[1] (포트 번호)에서 클라이언트의 연결 요청을 받을 준비를 마침
   *    like '가게 문을 열고 손님을 기다리는 상태'
   */
  listenfd = Open_listenfd(argv[1]);

  /*
   * 2. 무한 루프를 돌면서 클라이언트의 연결을 계속해서 기다림
   *    cause 서버는 특별한 일이 없는 한 꺼지면 안되기 때문 !
   */
  while (1) {
    clientlen = sizeof(struct sockaddr_storage); // accept()를 호출하기 전, 주소
                                                 // 구조체의 크기를 초기화

    /*
     * 3. 클라이언트의 연결 요청을 수락(Accept)함
     *    Accept 함수는 클라이언트가 연결을 요청할 때까지 여기서 '대기(block)'함
     *    연결 요청이 오면, 클라이언트와 통신할 새로운 '연결 소켓(connfd)'을
     * 만들어 반환하고, clientaddr 구조체에는 클라이언트의 주소 정보를 채워 넣음
     *    '손님이 오면, 그 손님 전용 직원을 한 명 배정하는 것'과 같음
     */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /*
     * 4. 연결된 클라이언트의 정보를 사람이 읽을 수 있는 형태로 변환
     *    Getnameinfo 함수는 clientaddr에 담긴 바이너리 주소 정보를
     *    우리가 아는 호스트 이름과 포트 번호 문자열로 바꿔줌
     */
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);

    /*
     * 5. 실제 데이터 송수신(echo)은 echo 함수에게 위임
     *    방금 생성된 따끈따끈한 연결 소켓(connfd)을 넘겨줌
     */
    echo(connfd);

    /*
     * 6. 클라이언트와의 통신이 끝나면, 연결 소켓을 닫음
     *    '전담 직원이 손님 응대를 마치고 돌아오는 것'과 같음
     *    이걸 하지 않으면 시스템에 좀비 연결이 계속 쌓이게 됨 (리소스 누수!)
     */
    Close(connfd);
  }
  exit(0); // 실제로는 while(1) 때문에 이 코드는 실행되지 X
}

// 전체 요약
// 1. 개업 준비: Open_listenfd로 특정 포트에 귀를 기울이는 듣기 소켓(listenfd)을
// 열어놓음
// 2. 영업 시작 (무한 루프)
//     - Accept로 손님(클라이언트)이 올 때까지 대기
//     - 손님이 오면, 그 손님 전용 연결 소켓(connfd)을 새로 만듬
//     - echo 함수를 호출하여 connfd를 통해 손님과 대화(데이터 송수신)함
//     - 대화가 끝나면(손님이 연결을 끊으면) Close로 connfd를 닫고 뒷정리
// 3. 계속 영업: 다시 루프의 처음으로 돌아가 다음 손님을 기다림
