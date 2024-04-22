#include "kernel/types.h"
#include "user/user.h"
#define LARGE_SIZE 4096
#define MAX_ALLOC_SIZE 1024
char* test_malloc(int size) {
    return (char*)new_malloc(size);
}
void test_free(char *ptr) {
    new_free(ptr);
}
//用于测试内存是否可以正确地写入和验证数据
void test_memory(char *ptr, int size, char pattern) {
    for(int i = 0; i < size; i++) {
        ptr[i] = pattern;
    }
    for(int i = 0; i < size; i++) {
        if(ptr[i] != pattern) {
            printf("Memory check failed at position %d\n", i);
            exit(0);
        }
    }
}
int main(int argc, char *argv[]) {
    // 基本的申请和释放测试
    char *ptr = test_malloc(100);
    if(ptr == 0) {
        printf("test_malloc failed to allocate memory\n");
        exit(0);
    }
    test_memory(ptr, 100, 0xAA);
    test_free(ptr);
    // 其他测试添加在此处
    printf("All tests passed\n");
    exit(0);
}