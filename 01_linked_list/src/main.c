#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/list.h"

/* -------------------------------------------------------------------------
 * 1. 사용자 정의 구조체 및 콜백 함수 세팅
 * ------------------------------------------------------------------------- */
struct student {
    int id;
    char name[32];
    struct node node; /* 리스트와 연결될 핵심 부품 (임베딩) */
};

/* [헬퍼] 학생 데이터 동적 할당 */
struct student* create_student(int id, const char* name)
{
    struct student *stu = (struct student *)malloc(sizeof(struct student));
    stu->id = id;
    strncpy(stu->name, name, sizeof(stu->name) - 1);
    stu->name[sizeof(stu->name) - 1] = '\0';
    
    stu->node.prev = NULL;
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

/* [콜백] for_each를 위한 출력 함수 */
void print_student_node(struct node *node)
{
    struct student *stu = container_of(node, struct student, node);
    printf("    [%d] %s", stu->id, stu->name);
}

/* [헬퍼] 리스트 현재 상태 한 줄로 출력 (디버깅용) */
void print_current_list(struct list *my_list)
{
    printf("  [현재 리스트]: ");
    if (my_list->ops->is_empty(my_list)) {
        printf("(비어 있음)");
    } else {
        my_list->ops->for_each(my_list, print_student_node);
    }
    printf("\n");
}

/* [콜백] find를 위한 조건 검색 (ID 비교) */
int compare_student_id(struct node *node, void *arg)
{
    struct student *stu = container_of(node, struct student, node);
    int target_id = *(int *)arg;
    return (stu->id == target_id) ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * 2. 메인 테스트 로직
 * ------------------------------------------------------------------------- */
int main(void)
{
    printf("🚀 Linked List ADT 시각화 테스트 시작\n\n");

    /* 1. 리스트 초기화 */
    struct list my_list;
    if (init_list(&my_list) != 0) {
        fprintf(stderr, "[ERROR] 리스트 초기화 실패\n");
        return 1;
    }
    printf("[SUCCESS] 리스트 초기화 완료\n");
    
    /* 2. 테스트용 데이터 생성 */
    struct student *s1 = create_student(101, "Alice");
    struct student *s2 = create_student(102, "Bob");
    struct student *s3 = create_student(103, "Charlie");
    struct student *s4 = create_student(104, "Dave");
    struct student *s5 = create_student(105, "Eve");

    /* 3. 삽입(Insert) API 테스트 (단계별 출력 적용) */
    printf("\n--- 📥 Insert 테스트 ---\n");
    
    printf("1. insert_last (Alice)\n");
    my_list.ops->insert_last(&my_list, &s1->node);
    print_current_list(&my_list);

    printf("2. insert_last (Bob)\n");
    my_list.ops->insert_last(&my_list, &s2->node);
    print_current_list(&my_list);

    printf("3. insert_first (Charlie)\n");
    my_list.ops->insert_first(&my_list, &s3->node);
    print_current_list(&my_list);

    printf("4. insert_at 인덱스 1 (Dave)\n");
    my_list.ops->insert_at(&my_list, 1, &s4->node);
    print_current_list(&my_list);

    printf("=> Insert 완료 후 Size: %d (예상: 4)\n", my_list.ops->size(&my_list));

    /* 4. 이동(Move) API 테스트 (단계별 출력 적용) */
    printf("\n--- 🚚 Move 테스트 ---\n");
    
    printf("1. move_first (Alice를 맨 앞으로)\n");
    my_list.ops->move_first(&my_list, &s1->node);
    print_current_list(&my_list);
    
    printf("2. move_last (Charlie를 맨 뒤로)\n");
    my_list.ops->move_last(&my_list, &s3->node);
    print_current_list(&my_list);

    printf("3. move 인덱스 2 (Dave를 인덱스 2로)\n");
    my_list.ops->move(&my_list, 2, &s4->node);
    print_current_list(&my_list);

    /* 5. 검색(Find) 및 접근(At) API 테스트 */
    printf("\n--- 🔍 Find & At 테스트 ---\n");
    int search_id = 102; /* Bob 찾기 */
    struct node *found_node = my_list.ops->find(&my_list, compare_student_id, &search_id);
    if (found_node) {
        struct student *found_stu = container_of(found_node, struct student, node);
        printf("[Find 성공] ID %d 학생 이름: %s\n", search_id, found_stu->name);
    }

    struct node *at_node = my_list.ops->at(&my_list, 2);
    if (at_node) {
        struct student *at_stu = container_of(at_node, struct student, node);
        printf("[At 성공] 인덱스 2번 학생 이름: %s (예상: Dave)\n", at_stu->name);
    }

    /* 6. 삭제(Remove) API 테스트 (단계별 출력 적용) */
    printf("\n--- 📤 Remove & Memory Free 테스트 ---\n");
    struct node *removed_node;
    struct student *target;

    printf("1. remove_first (첫 노드 삭제)\n");
    if ((removed_node = my_list.ops->remove_first(&my_list))) {
        target = container_of(removed_node, struct student, node);
        free(target);
        print_current_list(&my_list);
    }

    printf("2. remove_last (끝 노드 삭제)\n");
    if ((removed_node = my_list.ops->remove_last(&my_list))) {
        target = container_of(removed_node, struct student, node);
        free(target);
        print_current_list(&my_list);
    }

    printf("3. remove_at 인덱스 0 (0번 노드 삭제)\n");
    if ((removed_node = my_list.ops->remove_at(&my_list, 0))) {
        target = container_of(removed_node, struct student, node);
        free(target);
        print_current_list(&my_list);
    }

    /* 7. 명시적 리스트 비우기(Clear) API 테스트 */
    printf("\n--- 🧹 Clear 테스트 ---\n");
    printf("1. Eve 추가 (Clear 전 데이터 확보)\n");
    my_list.ops->insert_last(&my_list, &s5->node);
    print_current_list(&my_list);
    
    printf("\n2. 리스트 비우기(clear) 호출!\n");
    my_list.ops->clear(&my_list, free_student_node);
    print_current_list(&my_list);

    /* 8. 객체 파괴(Destroy) 테스트 */
    printf("\n--- 💥 Destroy 테스트 ---\n");
    destroy_list(&my_list, free_student_node);
    printf("🏁 모든 테스트 완료 및 객체 소멸 정상 처리.\n");

    return 0;
}