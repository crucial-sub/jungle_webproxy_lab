#include "csapp.h"

// connfd: main 함수로부터 받은 '클라이언트와의 전용 연결 소켓'
void echo(int connfd) {
  size_t n; // 읽어온 데이터의 바이트 수를 저장할 변수. size_t는 'unsigned
            // long'과 비슷한 타입으로, 크기를 나타낼 때 주로 사용됨
  char buf[MAXLINE]; // 클라이언트로부터의 데이터를 읽어올 버퍼
  rio_t rio;         // 견고한 I/O(Robust I/O)를 위한 구조체

  /*
   * 1. RIO 읽기 버퍼와 connfd를 연결하여 초기화
   *    이제 이 rio 구조체를 통해 connfd로부터 데이터를 안전하게 읽을 준비가 됨
   *    클라이언트와의 모든 '읽기' 작업은 이제 rio를 통해 이루어짐
   */
  Rio_readinitb(&rio, connfd);

  /*
   * 2. 클라이언트로부터 데이터를 읽고, 다시 써주는 것을 반복하는 루프
   *    Rio_readlineb 함수는 connfd로부터 한 줄을 읽어 buf에 저장하고, 읽은
   * 바이트 수를 반환 클라이언트가 연결을 끊으면(EOF), Rio_readlineb는 0을
   * 반환하고 루프는 종료됨
   */
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    // (서버 터미널에) 몇 바이트를 받았는지 로그를 출력
    printf("server received %d bytes\n", (int)n);

    /*
     * 3. 읽은 데이터를 클라이언트에게 그대로 다시 보냄 (Echo!)
     *    방금 읽어온 buf의 내용을, 정확히 n 바이트만큼 connfd를 통해 다시 전송
     *    => 이것이 바로 '에코(echo)' 서버의 핵심 동작
     */
    Rio_writen(connfd, buf, n);
  }

  // 루프가 끝났다는 것은 클라이언트와의 대화가 끝났다는 의미
  // 함수가 종료되고, 제어권은 main 함수의 'Close(connfd);' 라인으로 돌아감
}

// 전체 요약
// 1. 준비: 클라이언트와의 전용선(connfd)을 받아 RIO 읽기 도구를 초기화
// 2. 대화 (루프):
//     - 클라이언트가 보낸 메시지를 한 줄 읽음
//     - 읽은 메시지를 그대로 클라이언트에게 되돌려 보냄
// 3. 종료: 클라이언트가 연결을 끊으면 대화를 멈추고 함수를 종료