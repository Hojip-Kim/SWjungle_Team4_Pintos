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