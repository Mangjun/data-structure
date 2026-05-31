#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/queue.h" // 헤더 파일 이름에 맞게 수정해 주세요.

/* -------------------------------------------------------------------------
 * 1. 사용자 정의 구조체 및 콜백 함수 세팅
 * ------------------------------------------------------------------------- */
struct student {
    int id;
    char name[32];
    struct node node; /* 큐와 연결될 핵심 부품 (임베딩) */
};

/* [헬퍼] 학생 데이터 동적 할당 */
struct student* create_student(int id, const char* name)
{
    struct student *stu = (struct student *)malloc(sizeof(struct student));
    stu->id = id;
    strncpy(stu->name, name, sizeof(stu->name) - 1);
    stu->name[sizeof(stu->name) - 1] = '\0';
    
    // Queue의 노드는 단방향이므로 next만 초기화
    stu->node.next = NULL;
    
    return stu;
}

/* [콜백] 노드 메모리 해제용 */
void free_student_node(struct node *node) 
{
    struct student *target = container_of(node, struct student, node);
    printf("  -> [자동 정리] ID: %d, Name: %s 메모리 해제\n", target->id, target->name);
    free(target);
}

/* [헬퍼] 큐 현재 상태 요약 출력 (디버깅용) */
void print_queue_status(struct queue *my_queue)
{
    printf("  [큐 상태]: Size = %d, ", my_queue->ops->size(my_queue));
    if (my_queue->ops->is_empty(my_queue)) {
        printf("(비어 있음)\n");
    } else {
        struct node *front_node = my_queue->ops->peek(my_queue);
        struct student *front_stu = container_of(front_node, struct student, node);
        printf("Front = [%d] %s\n", front_stu->id, front_stu->name);
    }
}

/* -------------------------------------------------------------------------
 * 2. 메인 테스트 로직
 * ------------------------------------------------------------------------- */
int main(void)
{
    printf("🚀 Queue ADT 시각화 테스트 시작\n\n");

    /* 1. 큐 초기화 */
    struct queue my_queue;
    if (init_queue(&my_queue) != 0) {
        fprintf(stderr, "[ERROR] 큐 초기화 실패\n");
        return 1;
    }
    printf("[SUCCESS] 큐 초기화 완료\n");
    
    /* 2. 테스트용 데이터 생성 */
    struct student *s1 = create_student(101, "Alice");
    struct student *s2 = create_student(102, "Bob");
    struct student *s3 = create_student(103, "Charlie");
    struct student *s4 = create_student(104, "Dave");

    /* 3. 삽입(Enqueue) API 테스트 */
    printf("\n--- 📥 Enqueue 테스트 ---\n");
    
    printf("1. enqueue (Alice)\n");
    my_queue.ops->enqueue(&my_queue, &s1->node);
    print_queue_status(&my_queue);

    printf("2. enqueue (Bob)\n");
    my_queue.ops->enqueue(&my_queue, &s2->node);
    print_queue_status(&my_queue);

    printf("3. enqueue (Charlie)\n");
    my_queue.ops->enqueue(&my_queue, &s3->node);
    print_queue_status(&my_queue);

    /* 4. 참조(Peek) API 테스트 */
    printf("\n--- 👀 Peek 테스트 ---\n");
    struct node *peeked_node = my_queue.ops->peek(&my_queue);
    if (peeked_node) {
        struct student *peek_stu = container_of(peeked_node, struct student, node);
        // 큐는 FIFO이므로 가장 먼저 들어간 Alice가 Front에 있어야 합니다.
        printf("[Peek 성공] 현재 Front 학생: %s (예상: Alice)\n", peek_stu->name);
    }

    /* 5. 꺼내기(Dequeue) API 테스트 */
    printf("\n--- 📤 Dequeue 테스트 ---\n");
    struct node *dequeued_node;
    struct student *target;

    printf("1. dequeue (현재 Front 꺼내기)\n");
    if ((dequeued_node = my_queue.ops->dequeue(&my_queue))) {
        target = container_of(dequeued_node, struct student, node);
        printf("  -> [Dequeue 성공] 꺼낸 학생: %s\n", target->name);
        free(target); // 사용이 끝난 메모리는 사용자가 직접 해제
        print_queue_status(&my_queue);
    }

    printf("2. dequeue (다음 Front 꺼내기)\n");
    if ((dequeued_node = my_queue.ops->dequeue(&my_queue))) {
        target = container_of(dequeued_node, struct student, node);
        printf("  -> [Dequeue 성공] 꺼낸 학생: %s\n", target->name);
        free(target);
        print_queue_status(&my_queue);
    }

    /* 6. 명시적 큐 비우기(Clear) API 테스트 */
    printf("\n--- 🧹 Clear 테스트 ---\n");
    printf("1. Dave 추가 (Clear 전 데이터 확보)\n");
    my_queue.ops->enqueue(&my_queue, &s4->node);
    print_queue_status(&my_queue);
    
    printf("\n2. 큐 비우기(clear) 호출!\n");
    // clear 내부에서 free_student_node 콜백을 호출하여 메모리를 자동 정리함
    my_queue.ops->clear(&my_queue, free_student_node);
    print_queue_status(&my_queue);

    /* 7. 객체 파괴(Destroy) 테스트 */
    printf("\n--- 💥 Destroy 테스트 ---\n");
    // 만약 큐에 남은 데이터가 있다면 여기서도 free 콜백이 호출됨
    destroy_queue(&my_queue, free_student_node);
    printf("🏁 모든 테스트 완료 및 객체 소멸 정상 처리.\n");

    return 0;
}