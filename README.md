# C 문법 및 선형 자료 구조 복습

## 체크리스트

### 1. 배열 스택 (`stack`)
- [x] 헤더 / 테스트 뼈대 (`include/stack.h`, `tests/test_stack.c`)
- [x] 구현 (`src/stack.c`)
- [x] `make test-stack` 통과 (ASan/UBSan 포함)
- [x] 리뷰 완료 + 지적 사항 처리

### 2. 링버퍼 큐 (`ringbuf`)
- [x] 헤더 / 테스트 뼈대 (`include/ringbuf.h`, `tests/test_ringbuf.c`)
- [x] full/empty 구분 방식 선택 + 근거를 `ringbuf.h` 상단에 기록
- [x] 구현: 임의 크기 버전 (`ringbuf_*`)
- [x] 구현: 2의 거듭제곱 + 마스크 버전 (`ringbuf_p2_*`)
- [x] (C) free-running 오버플로 테스트(`t_p2_index_overflow`) 통과
- [x] `make test-ringbuf` 통과
- [x] ISR 안전성 설계를 `ringbuf.h` 에 기록 (소유권, `volatile` 의 한계, 메모리 순서, 인덱스 타입) — 별도 문서 대신 헤더에 둠
- [x] 헤더 필드 선언 반영 (`_Atomic rb_index_t` + lock-free `_Static_assert`)
- [x] 구현을 `atomic_load_explicit` / `atomic_store_explicit` 로 전환 (일반, p2)
- [x] 리뷰 완료 + 지적 사항 처리

### 3. 고정 크기 메모리 풀 (`pool`)
- [x] 헤더 / 테스트 뼈대 (`include/pool.h`, `tests/test_pool.c`)
- [x] 이중 해제 검출용 상태(state) 표현/배치 선택 + 근거를 `pool.h` 에 기록 (블록 뒤 비트맵)
- [ ] free list 구현
- [ ] 블록 정렬 처리 (블록 크기 반올림, 저장소 시작 주소 정렬)
- [ ] 할당 실패(고갈) / 해제 실패(풀 밖 포인터, 이중 해제, 정렬 안 맞는 포인터) 처리
- [ ] `make test-pool` 통과
- [ ] 리뷰 완료

### 4. 연결리스트 v1: 정적 배열 + 인덱스 기반 next
- [x] 헤더 / 테스트 뼈대 (`include/list_v1.h`, `tests/test_list_v1.c`)
- [ ] 구현
- [ ] 테스트 통과
- [ ] 리뷰 완료

### 5. 연결리스트 v2: 풀 노드 + 이중 포인터 삭제
- [x] 헤더 / 테스트 뼈대 (`include/list_v2.h`, `tests/test_list_v2.c`)
- [ ] 구현 (3단계 `pool` 사용)
- [ ] 테스트 통과
- [ ] 리뷰 완료

### 6. 연결리스트 v3: intrusive (`list_head` + `offsetof` / `container_of`)
- [x] 헤더 / 테스트 뼈대 (`include/list_v3.h`, `tests/test_list_v3.c`)
- [ ] `list3_container_of` / `list3_for_each(_safe)` 매크로 구현 (헤더 자리표시 교체)
- [ ] 구현
- [ ] 테스트 통과
- [ ] 리뷰 완료