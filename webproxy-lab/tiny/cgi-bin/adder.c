/*
 * adder.c - 두 숫자를 더해주는 최소한의 CGI 프로그램
 */
#include "csapp.h"

int main(void) {
  char *buf, *p; // QUERY_STRING을 가리킬 포인터(buf)와 문자열 처리를 위한 보조
                 // 포인터(p)
  char arg1[MAXLINE], arg2[MAXLINE],
      content[MAXLINE]; // 인자1, 인자2, 그리고 최종 HTML 본문을 담을 버퍼
  int n1 = 0, n2 = 0; // 문자열에서 변환된 정수를 저장할 변수

  /* 1. 두 인자(argument)를 추출하는 과정 */
  // getenv 함수로 "QUERY_STRING" 환경 변수의 값을 가져옴
  // 웹 서버는 URL의 '?' 뒤에 오는 문자열을 이 환경 변수에 담아 CGI 프로그램에게
  // 전달 예: http://.../adder?100&200 -> QUERY_STRING="100&200" (책의 예제는
  // num1=100&num2=200 과 같은 형태를 가정하고 있으나, 코드 구현은 약간 다름
  //  여기서는 책의 코드 구현(num1=100&num2=200)에 맞춰 설명)
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    // buf: "num1=100&num2=200"
    p = strchr(buf, '&'); // '&' 문자를 찾아서 p가 가리키게 함
    *p = '\0'; // 찾은 '&'를 NULL 문자('\0')로 바꿔서 문자열을 둘로 나눔
               // 이제 buf는 "num1=100" 이라는 문자열이 됨

    strcpy(arg1, buf); // "num1=100"을 arg1에 복사
    strcpy(arg2,
           p + 1); // NULL 문자 바로 다음부터 끝까지("num2=200")를 arg2에 복사

    // 이제 각 인자에서 숫자 부분만 추출
    n1 = atoi(strchr(arg1, '=') +
              1); // arg1("num1=100")에서 '='를 찾고, 그 다음 위치("100")부터
                  // atoi(문자열->정수)로 변환
    n2 = atoi(strchr(arg2, '=') + 1); // arg2("num2=200")에서 '='를 찾고, 그
                                      // 다음 위치("200")부터 atoi로 변환
  }

  /* 2. 응답 본문(Response Body) 만들기 */
  // sprintf는 printf와 유사하지만, 화면이 아닌 문자열 버퍼(content)에 내용을
  // 출력(저장)
  sprintf(content, "QUERY_STRING=%s\r\n<p>",
          buf); // 디버깅용으로 원본 QUERY_STRING을 출력
  sprintf(content + strlen(content),
          "Welcome to add.com: "); // 문자열 뒤에 이어서 씀
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1,
          n2, n1 + n2); // 계산 결과를 HTML 형식으로 추가
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* 3. HTTP 응답(Response) 생성하기 */
  // CGI 프로그램은 '표준 출력(stdout)'으로 결과를 출력해야 합니다. 웹 서버가 이
  // 출력을 가로채서 브라우저에 전달함
  printf("Content-type: text/html\r\n"); // HTTP 헤더: "이 콘텐츠는 HTML
                                         // 문서입니다."
  printf("Content-length: %d\r\n",
         (int)strlen(content)); // HTTP 헤더: "콘텐츠의 길이는 이만큼입니다."
  printf("\r\n"); // HTTP 규약: 헤더와 본문을 구분하는 ★★★아주 중요한★★★ 빈 줄
  printf("%s", content); // 실제 내용, 즉 위에서 만든 HTML 본문을 출력
  fflush(stdout); // 표준 출력 버퍼에 남아있는 내용을 강제로 모두 내보냄

  exit(0);
}
/* $end adder */

// adder.c CGI 프로그램의 일생을 요약하면 다음과 같다.
// 1. 출생: 웹 서버가 사용자의 요청을 받아 adder.c 프로그램을 실행시킴
// 2. 임무 수령: getenv("QUERY_STRING")을 통해 URL에 담긴 데이터를 환경 변수로
// 전달받음
// 3. 임무 분석: 문자열 함수들로 전달받은 데이터를 파싱하여 숫자 n1, n2를 추출함
// 4. 결과 계산: n1 + n2를 계산
// 5. 보고서 작성: sprintf를 이용해 계산 결과를 담은 HTML 문자열(content)을 만듬
// 6. 보고서 제출: printf를 이용해 표준 HTTP 응답 양식(헤더 + 빈 줄 + 본문)에
// 맞춰 결과를 표준 출력으로 내보냄
// 7. 소멸: 웹 서버가 이 출력을 받아 브라우저에게 전달해주고, adder.c 프로그램은
// 임무를 완수하고 조용히 종료