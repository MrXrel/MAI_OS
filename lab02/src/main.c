#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

const int RUN = 32;

typedef struct TaskInfo {
    int start;
    int end;
    int is_active;
    int* data;
} TaskInfo;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void log_message(int fd, const char* msg) {
    size_t length = 0;
    while (msg[length] != '\0') {
        length++;
    }
    write(fd, msg, length);
}


void log_number(long number) {
    char buffer[64];
    int pos = 0;

    long temp = number;
    if (temp == 0) {
        buffer[pos++] = '0';
    } else {
        while (temp > 0) {
            buffer[pos++] = (temp % 10) + '0';
            temp /= 10;
        }
    }
    for (int i = 0; i < pos / 2; i++) {
        char tmp = buffer[i];
        buffer[i] = buffer[pos - 1 - i];
        buffer[pos - 1 - i] = tmp;
    }
    write(STDOUT_FILENO, buffer, pos);
}

void write_configuration(int array_size, int thread_count) {
    // "Array size: "
    log_message(STDOUT_FILENO, "Array size: ");
    log_number(array_size);
    log_message(STDOUT_FILENO, "\n");


    log_message(STDOUT_FILENO, "Thread count: ");
    log_number(thread_count);
    log_message(STDOUT_FILENO, "\n");
}

void insertion_sort(int* data, int start, int end) {
    for (int i = start + 1; i <= end; i++) {
        int key = data[i];
        int j = i - 1;
        while (j >= start && data[j] > key) {
            data[j + 1] = data[j];
            j--;
        }
        data[j + 1] = key;
    }
}

void merge_sections(int* data, int start, int middle, int end) {
    int left_size = middle - start + 1;
    int right_size = end - middle;

    int* left_part = (int*)malloc(left_size * sizeof(int));
    int* right_part = (int*)malloc(right_size * sizeof(int));

    if (!left_part || !right_part) {
        log_message(STDOUT_FILENO, "Memory error\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < left_size; i++) {
        left_part[i] = data[start + i];
    }
    for (int i = 0; i < right_size; i++) {
        right_part[i] = data[middle + 1 + i];
    }

    int i = 0, j = 0, k = start;

    while (i < left_size && j < right_size) {
        if (left_part[i] <= right_part[j]) {
            data[k] = left_part[i];
            ++i;
        } else {
            data[k] = right_part[j];
            ++j;
        }
        ++k;
    }

    while (i < left_size) {
        data[k] = left_part[i];
        ++k;
        ++i;
    }

    while (j < right_size) {
        data[k] = right_part[j];
        ++k;
        ++j;
    }

    free(left_part);
    free(right_part);
}

void tim_sort(int* array, int left, int right) {
    if (right - left + 1 <= RUN) {
        insertion_sort(array, left, right);
        return;
    }

    int mid = left + (right - left) / 2;
    tim_sort(array, left, mid);
    tim_sort(array, mid + 1, right);
    merge_sections(array, left, mid, right);
}

void* thread_sort_task(void* arg) {
    TaskInfo* data = (TaskInfo*)arg;
    tim_sort(data->data, data->start, data->end);
    pthread_exit(NULL);
}

long get_current_time_ms() {
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
}

int main(int argc, char** argv) {
    const int array_size = 1e8;
    int thread_count;

    if (argc != 2) {
        log_message(STDOUT_FILENO, "Usage: program <thread_count>\n");
        exit(EXIT_FAILURE);
    }

    thread_count = atoi(argv[1]);
    
    if (thread_count > 16 || thread_count <= 0) {
        log_message(STDOUT_FILENO, "Wrong number of threads\n");
        exit(EXIT_FAILURE);
    }

    write_configuration(array_size, thread_count);

    int* data = (int*)malloc(sizeof(int) * array_size);

    srand((unsigned)time(NULL));
    for (int i = 0; i < array_size; i++) {
        data[i] = rand();
    }

    log_message(STDOUT_FILENO, "Array randomized.\n");

    pthread_t* thread_pool = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    TaskInfo* tasks = (TaskInfo*)malloc(sizeof(TaskInfo) * thread_count);

    int segment_length = array_size / thread_count;
    int offset = 0;

    long start_time = get_current_time_ms();

    for (int i = 0; i < thread_count; i++, offset += segment_length) {
        TaskInfo* task = &tasks[i];
        task->data = data;
        task->is_active = 1;
        task->start = offset;
        task->end = (i == thread_count - 1) ? array_size - 1 : offset + segment_length - 1;

        pthread_create(&thread_pool[i], NULL, thread_sort_task, task);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(thread_pool[i], NULL);
    }

    pthread_mutex_lock(&lock);
    TaskInfo* main_task = &tasks[0];
    for (int i = 1; i < thread_count; i++) {
        TaskInfo* next_task = &tasks[i];
        merge_sections(main_task->data, main_task->start, next_task->start - 1, next_task->end);
    }
    pthread_mutex_unlock(&lock);

    long end_time = get_current_time_ms();

    log_message(STDOUT_FILENO, "Sorting completed in ");
    log_number(end_time - start_time);
    log_message(STDOUT_FILENO, " ms\n");
    
    for (int i = 1; i < array_size; i++) {
        if (data[i] < data[i - 1]) {
            log_message(STDOUT_FILENO, "Not sorted\n");
            break;
        }
    }

    free(tasks);
    free(thread_pool);
    free(data);

    pthread_mutex_destroy(&lock);

    return 0;
}
