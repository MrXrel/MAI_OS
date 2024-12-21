#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct Allocator {
    void *(*allocator_create)(void *addr, size_t size);
    void *(*allocator_alloc)(void *allocator, size_t size);
    void (*allocator_free)(void *allocator, void *ptr);
    void (*allocator_destroy)(void *allocator);
} Allocator;

void *standard_allocator_create(void *memory, size_t size) {
    (void)size;
    (void)memory;
    return memory;
}

void *standard_allocator_alloc(void *allocator, size_t size) {
    uint32_t *memory = mmap(NULL, size + sizeof(uint32_t), PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        return NULL;
    }
    *memory = (uint32_t)(size + sizeof(uint32_t));
    return memory + 1;
}

void standard_allocator_free(void *allocator, void *memory) {
    if (memory == NULL)
        return;
    uint32_t *mem = (uint32_t *)memory - 1;
    munmap(mem, *mem);
}

void standard_allocator_destroy(void *allocator) {
    (void)allocator;
}

void load_allocator(const char *library_path, Allocator *allocator) {
    void *library = dlopen(library_path, RTLD_LOCAL | RTLD_NOW);
    if (!library) {
        // Если не удалось загрузить библиотеку, используем стандартные функции
        char message[] = "WARNING: failed to load shared library\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        allocator->allocator_create = standard_allocator_create;
        allocator->allocator_alloc = standard_allocator_alloc;
        allocator->allocator_free = standard_allocator_free;
        allocator->allocator_destroy = standard_allocator_destroy;
        return;
    }

    // Загружаем функции из библиотеки
    allocator->allocator_create = dlsym(library, "allocator_create");
    allocator->allocator_alloc = dlsym(library, "allocator_alloc");
    allocator->allocator_free = dlsym(library, "allocator_free");
    allocator->allocator_destroy = dlsym(library, "allocator_destroy");

    if (!allocator->allocator_create || !allocator->allocator_alloc || !allocator->allocator_free || !allocator->allocator_destroy) {
        const char msg[] = "Error: failed to load all allocator functions\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        dlclose(library);
    }
}

int main(int argc, char **argv) {
    const char *library_path = (argc > 1) ? argv[1] : NULL;

    // Статически выделяем память для структуры Allocator
    Allocator allocator;
    load_allocator(library_path, &allocator);

    size_t size = 4104;
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) {
        char message[] = "mmap failed\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return EXIT_FAILURE;
    }

    void *allocator_instance = allocator.allocator_create(addr, size);
    if (!allocator_instance) {
        char message[] = "Failed to initialize allocator\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        munmap(addr, size);
        return EXIT_FAILURE;
    }

    char start_message[] = "Allocator initialized\n";
    write(STDOUT_FILENO, start_message, sizeof(start_message) - 1);

    void *block1 = allocator.allocator_alloc(allocator_instance, 256);
    void *block2 = allocator.allocator_alloc(allocator_instance, 1023);

    if (block1 == NULL || block2 == NULL) {
        char alloc_fail_message[] = "Memory allocation failed\n";
        write(STDERR_FILENO, alloc_fail_message, sizeof(alloc_fail_message) - 1);
    } else {
        char alloc_success_message[] = "Memory allocated successfully\n";
        write(STDOUT_FILENO, alloc_success_message, sizeof(alloc_success_message) - 1);
    }

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Block 1 address: %p\n", block1);
    write(STDOUT_FILENO, buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "Block 2 address: %p\n", block2);
    write(STDOUT_FILENO, buffer, strlen(buffer));

    allocator.allocator_free(allocator_instance, block1);
    allocator.allocator_free(allocator_instance, block2);

    char free_message[] = "Memory freed\n";
    write(STDOUT_FILENO, free_message, sizeof(free_message) - 1);

    allocator.allocator_destroy(allocator_instance);
    munmap(addr, size);

    char exit_message[] = "Program exited successfully\n";
    write(STDOUT_FILENO, exit_message, sizeof(exit_message) - 1);

    return EXIT_SUCCESS;
}
