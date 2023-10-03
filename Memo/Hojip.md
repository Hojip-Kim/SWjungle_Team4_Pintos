list.c / 
195 : list_push_front가 있음(list의 앞쪽에 push) (반환 : x)

202 : list_push_back이 있음(list 뒷쪽에 push) (반환 : x)

241 : list 내부 특정 인자 삭제(remove) (반환 : remove된 인자)

251 : list_pop_front : list front를 pop (반환 : pop된 list_elem*)

260 : list_pop_back : list back을 pop (반환 : pop된 list_elem*)

269 : list_front : list의 front를 return (반환형식 : list_elem*)

296 : list_empty : list가 empty한지 (반환형식 : bool)

302 : swap : a와 b 스왑 (반환 x)

310 : list_reverse : 리스트 거꾸로

324 : is_sorted : 정렬되어있는지 (반환 : 정렬되어있으면 true , else : false)

381 : list_sort
list_sort (struct list *list, list_less_func *less, void *aux)
: merge sort로 되어있는듯

419 : list_insert_ordered : 삽입을 하면서 정렬

---

threads/thread.c


---
tests/threads/alarm-wait.c

line 51
test_sleep 함수로 테스트를 진행, threads를 위한 malloc공간과 output, 즉 thread가 sleep상태에 들어가며 이 thread가 꺠어나는 순서나 시간정보 등을 output 메모리 영역에 기록함.
=> malloc이 정상적으로 동작했다면, 시작시간(test.start)를 현재시간 + 100틱으로 설정, test.iterations는 현재 들어오는 iteration, 그 다음 출력을위한 lock을 초기화(test.output_lock)하고, 출력버퍼의 포인터를 설정.(test.output_pos)
=> 후에 thread_cnt만큼 스레드들을 생성해줌. sleep_thread *t에 주소(i번째 스레드 구조체의 주소)를 가져오고, 해당 스레드구조체에 테스트 구조체의 주소를 저장,