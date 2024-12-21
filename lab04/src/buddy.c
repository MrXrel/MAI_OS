#include <stddef.h>
#include <stdint.h>  // Для использования uintptr_t

// Структура блока
typedef struct Block {
    size_t size;         // Размер блока
    struct Block *next;  // Указатель на следующий свободный блок
    int free;            // Флаг, указывающий, свободен ли блок
} Block;

// Функция для нахождения соседнего блока с использованием XOR
Block *find_buddy(Block *block_to_free) {
    // Преобразуем указатель на блок в целое число
    uintptr_t block_addr = (uintptr_t)block_to_free;

    // Вычисляем адрес соседнего блока с помощью операции XOR
    uintptr_t buddy_addr = block_addr ^ block_to_free->size;

    // Преобразуем адрес соседнего блока обратно в указатель
    Block *buddy = (Block *)buddy_addr;

    return buddy;
}

typedef struct Allocator {
    Block *free_blocks;  
} Allocator;


void *allocator_alloc(Allocator *allocator, size_t size) {
    Block *current = allocator->free_blocks;
    Block *prev = NULL;
    size_t required_size = 1;
    while (required_size < size) {
        required_size <<= 1; 
    }
    size = required_size;
    
    while (current) {
        if (current->free && current->size >= size) {
            while (current->size > size) {
                // Разбиваем блок на два меньших
                // Находим адрес для нового блока
                Block *buddy = (Block *)((uintptr_t)current + current->size / 2);
                buddy->size = current->size / 2;
                buddy->free = 1;

                current->size /= 2;

                // Связываем buddy в список
                buddy->next = current->next; 
                current->next = buddy;        
            }
            current->free = 0;

            // Убираем блок из списка свободных
            if (prev) {
                prev->next = current->next;
            } else {
                allocator->free_blocks = current->next;
            }

            // Возвращаем указатель на данные после заголовка
            return (char *)current + sizeof(Block);
        }

        prev = current;
        current = current->next;
    }
    return NULL;
}

void allocator_free(Allocator *allocator, void *ptr) {
    Block *block_to_free = (Block *)((char *)ptr - sizeof(Block));
    block_to_free->free = 1;
    
    Block *buddy = find_buddy(block_to_free);

    // Проверяем, свободен ли соседний блок
    if (buddy && buddy->free && buddy->size == block_to_free->size) {
        // Сливаем два блока
        if (buddy > block_to_free) {
            // Сосед правый, обновляем текущий блок
            block_to_free->size *= 2;
            block_to_free->next = buddy->next;
        } else {
            // Сосед левый, обновляем buddy
            buddy->size *= 2;
            buddy->next = block_to_free->next;
        }
        // Рекурсивно пытаемся слить с соседями
        allocator_free(allocator, (char *)buddy + sizeof(Block));
    } else {
        // Если не можем слить, добавляем блок в список свободных
        block_to_free->next = allocator->free_blocks;
        allocator->free_blocks = block_to_free;
    }
}

Allocator *allocator_create(void *memory, size_t size) {
    Allocator *allocator = (Allocator *)memory;
    allocator->free_blocks = (Block *)(memory + sizeof(Allocator));
    allocator->free_blocks->size = size - sizeof(Allocator);
    allocator->free_blocks->free = 1;
    allocator->free_blocks->next = NULL;

    return allocator;
}

void allocator_destroy(Allocator *allocator) {
    allocator->free_blocks = NULL;
}
