#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct FreeListNode {
    struct FreeListNode *next; // Указатель на следующий блок
    size_t size;               // Размер блока памяти
} FreeListNode;

typedef struct Allocator {
    void *memory;             
    size_t total_size;        
    FreeListNode *free_list;  
} Allocator;

Allocator* allocator_create(void *const memory, const size_t size) {
    if (memory == NULL || size == 0) {
        return NULL;
    }

    Allocator *allocator = (Allocator *) memory;
    allocator->total_size = size - sizeof(Allocator);  
    allocator->free_list = (FreeListNode *)((uintptr_t)memory + sizeof(Allocator));

    allocator->free_list->next = NULL;
    allocator->free_list->size = allocator->total_size;

    return allocator;
}

void allocator_destroy(Allocator *const allocator) {
    if (allocator != NULL) {
        allocator->memory = NULL;
        allocator->free_list = NULL;
    }
}
void* allocator_alloc(Allocator *const allocator, const size_t size) {
    if (allocator == NULL || size == 0) {
        return NULL;
    }
    FreeListNode **prev = &allocator->free_list;
    FreeListNode *curr = *prev;

    while (curr != NULL) {
        if (curr->size >= size) {
            if (curr->size > size + sizeof(FreeListNode)) {
                // Создаем новый блок, оставшийся после выделения
                FreeListNode *new_block = (FreeListNode *)((uintptr_t)curr + size);
                new_block->size = curr->size - size;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            // Убираем блок из списка свободных блоков
            *prev = curr->next;

            // Возвращаем указатель на выделенный блок (после свободного списка)
            return (void *)((uintptr_t)curr + sizeof(FreeListNode));
        }
        prev = &curr->next;
        curr = curr->next;
    }
    return NULL;
}

void allocator_free(Allocator *const allocator, void *const ptr) {
    if (allocator == NULL || ptr == NULL) {
        return;
    }
    FreeListNode *block_to_free = (FreeListNode *)((uintptr_t)ptr - sizeof(FreeListNode));

    // Добавляем освобожденный блок в начало списка
    block_to_free->next = allocator->free_list;
    allocator->free_list = block_to_free;
}
