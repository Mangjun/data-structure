# 🚀 Advanced C Data Structures (Linux Kernel Style)

본 리포지토리는 C 언어를 활용하여 핵심 자료구조(ADT, Abstract Data Type)를 구현하는 프로젝트입니다. 단순한 알고리즘 실습을 넘어, **실제 리눅스 커널에서 사용되는 고급 시스템 프로그래밍 기법**을 적용하여 실무 수준의 견고한 소프트웨어 설계 역량을 기르는 것을 목표로 합니다.

특히 아래의 4가지 핵심 원칙을 모든 자료구조 구현에 엄격하게 적용합니다.

* **구조체 임베딩 (Struct Embedding):** `container_of` 매크로를 활용하여 데이터와 노드를 분리하는 제네릭(Generic) 설계
* **함수 포인터 테이블 (Ops Table):** 구조체 내부에 `ops` 인터페이스를 두어 C 언어 환경에서의 다형성(Polymorphism) 구현
* **Pimpl 이디엄 (Pointer to Implementation):** 내부 구현체(Internal)를 숨겨 헤더 파일의 컴파일 의존성을 획기적으로 낮추는 설계
* **메모리 무결점 검증 (Zero-Defect Memory):** `Valgrind`를 활용한 철저한 메모리 누수(Memory Leak) 및 댕글링 포인터 추적

## 💻 개발 및 테스트 환경 (Development Environment)
메모리 관리와 포인터 연산이 핵심인 만큼, 리눅스 네이티브 환경에서 엄격한 디버깅을 진행합니다.

* **OS:** `Ubuntu 24.04 LTS`
* **IDE:** `Visual Studio Code (VSCode)`
* **Compiler:** `GCC (GNU Compiler Collection)`
* **Memory Debugger:** `Valgrind` (Memcheck)
* **Build System:** `Makefile`

## 📂 디렉토리 구조 및 커리큘럼 (Table of Contents)
각 단계별로 선형 자료구조부터 비선형, 탐색 기반 자료구조까지 점진적으로 확장해 나갑니다.

### Phase 1: 선형 자료구조 (Linear Data Structures)
가장 기본이 되는 연결 리스트를 완벽하게 제어하고, 이를 기반으로 스택과 큐를 확장합니다.
- [x] **`01_linked_list/`**
- [ ] **`02_stack/`**
- [ ] **`03_queue/`**

### Phase 2: 비선형 자료구조 (Non-linear Data Structures)
계층적 데이터와 복잡한 관계를 표현하기 위한 트리 및 그래프 구조를 설계합니다.
- [ ] **`04_tree/`**
- [ ] **`05_priority_queue/`**
- [ ] **`06_graph/`**

### Phase 3: 탐색 및 매핑 구조 (Search & Mapping)
효율적인 데이터 검색과 유일성을 보장하기 위한 고급 자료구조를 구현합니다.
- [ ] **`07_set/`**
- [ ] **`08_map/`**

---
*💡 각 디렉토리 내부에는 자료구조 인터페이스(`.h`), 내부 구현체(`.c`), 테스트 코드(`main.c`), 그리고 Valgrind 테스트 자동화를 위한 쉘 스크립트가 포함되어 있습니다.*