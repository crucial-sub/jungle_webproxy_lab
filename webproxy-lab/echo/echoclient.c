#include "csapp.h"

int main(int argc, char **argv) {
  int clientfd; // 클라이언트 소켓을 위한 파일 디스크립터(파일처럼 읽고/쓰는
                // 대상이 됨)
  char *host, *port,
      buf[MAXLINE]; // 서버 주소(IP), 포트 번호, 데이터를 담을 버퍼
  rio_t rio; // RIO(Robust I/O) 구조체. 안정적인 입출력을 도와주는 도구

  // 프로그램 실행 시 필요한 인자(argument)가 3개가 맞는지 확인
  // ex) ./echoclient 127.0.0.1 8080
  //        프로그램      host    port
  if (argc != 3) {
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }
  host = argv[1];
  port = argv[2];

  /*
   * 1. 서버에 접속. Open_clientfd 함수가 모든 복잡한 과정 처리.
   *    내부적으로 socket(), connect() 시스템 콜이 일어나고,
   *    성공하면 서버와 연결된 소켓의 번호표, 즉 파일 디스크럽터(clientfd)를
   * 반환
   */
  clientfd = Open_clientfd(host, port);

  /*
   * 2. RIO 읽기 버퍼를 초기화.
   *    우리가 만든 파일 디스크럽터(clientfd)와 RIO 구조체(rio)를 연결해주는
   * 작업 이제 rio라는 도구를 통해 clientfd에서 데이터를 안정적으로 읽을 수 있게
   * 됨
   */
  Rio_readinitb(&rio, clientfd);

  /*
   * 3. 사용자로부터 입력을 받아 서버와 통신하는 메인 루프.
   *    Fgets가 stdin(표준 입력, 즉 키보드)로부터 한 줄을 읽어 buf에 저장함
   *    사용자가 Ctrl+D (EOF)를 입력하면 Fgets는 NULL을 반환하고 루프가 종료됨
   */
  while (Fgets(buf, MAXLINE, stdin) != NULL) {
    /*
     * 4. 사용자에게 입력받은 데이터를 서버로 전송
     *    Rio_writen은 데이터(buf)를 clientfd에 연결된 서버로 확실하게(n 바이트
     * 전부) 보냄
     */
    Rio_writen(clientfd, buf, strlen(buf));

    /*
     * 5. 서버가 보낸 "에코" 데이터를 다시 읽어들임
     *    Rio_readlineb는 rio 버퍼를 통해 서버로부터 한 줄(\n을 만날 때까지)을
     * 읽어 buf에 저장
     */
    Rio_readlineb(&rio, buf, MAXLINE);

    // 6. 서버로부터 받은 데이터를 화면(stdout, 표준 출력)에 출력
    Fputs(buf, stdout);
  }

  /*
   * 7. 통신이 끝났으니 연결을 닫음 (자원해제)
   *    => 전화기를 끊는 것과 같음. 매우 중요한 습관 !
   */
  Close(clientfd);
  exit(0);
}

// 전체 요약
// 1. 준비: 프로그램 실행 시 서버의 주소와 포트 번호를 인자로 받는다.
// 2. 연결: Open_clientfd로 서버에 TCP 연결을 요청하고, 통신용
// 번호표(clientfd)를 받는다.
// 3. 초기화: Rio_readinitb로 안정적인 데이터 수신을 위한 RIO 도구를 준비한다.
// 4. 대화 (루프):
//     - 사용자에게 메시지를 입력받는다. (Fgets)
//     - 입력받은 메시지를 서버로 보낸다. (Rio_writen)
//     - 서버가 되돌려준 메시지를 받는다. (Rio_readlineb)
//     - 받은 메시지를 화면에 출력한다. (Fputs)
// 5. 종료: 사용자가 입력을 멈추면, Close로 연결을 끊고 프로그램을 종료한다.