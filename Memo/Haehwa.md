2023/09/22
### youtube 널널한 개발자 TV - "Process와 Thread의 차이"를 보고 해당 내용을 정리함.

프로세스가 여러개면 멀티 테스킹
한 프로세스에 thread가 여러개면 multi-threading

OS는 프로세스에게 cpu, virtual memory 사용을 지시함.
프로세스에 속한 모든 스레드는 프로세스의 가상 메모리로 공간이 제약된다.
스레드는 실질적 연산을 하는 주체이다. 스레드 관련하여 동시성(Concurrency), 동기성(Synchronous) 이슈가 존재

프로세스는 한 가구 이며 스레드는 세대원이라 해보자
거실은 가구원이 모두 같이 쓰는 공간이며 가구원은 각자 방을 쓸것이다.
거실은 heap, 각자의 방은 stack, thread local storage(TLS)이라고 할 수 있다.
* TLS - 스레드도 각자의 고유한 전역변수가 필요한 경우가 있다. 이를위해 data영역처럼 스레드별 고유한 영역을 TLS라고 한다. 


### youtube 널널한 개발자 TV - "Chapter03 프로세스와 스레드 매우중요"를 보고 해당 내용을 정리함.

OS는 프로세스 관리를 위한 정보를 PCB(Process Control Block)에 스레드르 위한 정보를 TCB(Thread Control Block)에 담음.
TCB는 시스템 스레드 정보를 위한 도서관처럼 작동함. (프로세스에 대한 중요한 정보들을 담아놓음)
PCB는 시스템은 프로세스에 관한 모든 정보를 담아둠. PCB는 context switching에서 중요한 역할을 담당함.

프로그램은 HDD에 설치됨 >> 운영체제가 프로그램을 램 메모리에 올림(인스턴스화/실행) >> 프로세스
프로세스의 라이프 사이클 : 1.생성 >> 2.준비 >> 3.실행 >> 4.완료 | 3-1대기상태
3-1대기상태 : I/O request를 디바이스로부터 (기다렸다 받으면/실행상태가 유지 되지 않으면) blocking I/O, (안기다렸다 받으면/실행상태가 유지 되면) non blocking I/O.

OS가 프로세스를 관리할 때 Queue 자료구조를 사용함.
dispatcher(운행관리원) 처럼 OS가 (쓰레드를 누구를 보낼지 정함/ready - queue에 담긴 스레드를 dispatch 함). 
dispatch 후 자원이 할당되고 실행됨.


### youtube 널널한 개발자 TV - "Chapter03 프로세스 상태(휴식, 보류)와 문맥 교환", "Sleep 함수와 우연 그리고 CPU로 랜덤뽑기"를 보고 해당 내용을 정리함.

프로세스에는 슬립(휴식), 서스팬드(보류상태)가 존재.
서스팬드는 외부요인에 의한것이며 비자발적인 반면 슬립은 자발적임.
슬립, 서스팬드 모두 프로세스가 대기열/Ready-queue에서 이탈하게 됨. >> 정해진 시간이 지난 후 >> 대기열에 다시 재 진입 하게 됨.
*주의 1초 동안 슬립을 했다고 해서 1초후에 해당 프로세스가 연산을 시작하는게 아님. 1초후에 Ready-queue에 재 진입 하기 때문에 앞에 쌓여 있는 프로세스에 따라 추가로 시간이 더 소요됨.

context 스위칭
P1 스레드가 중간에 멈춤 >> P1의 상태를 pcb1에 저장 >> pcb2에서 P2 스레드의 상태를 가져옴 | 문맥교환 p1 -> p2
P2 스레드가 중간에 멈춤 >> P2의 상태를 pcb2에 저장 >> pcb1에서 P1 스레드의 상태를 가져옴 | 문맥교환 p2 -> p1

### 문제 접근
timer_sleep > thread_yield에 print를 찍어본 결과 쓰레드가 슬립하고 있는게 아님.
따라서 semaphore(수신호)를 통해 슬립을 구현할 수 있을것 같음.
