#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stddef.h>

typedef struct Job {
    void (*function)(void *arg);
    void *arg;
    struct Job *next;
} Job;

typedef struct {
    Job *head;
    Job *tail;
    int count;
    int working;
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_cond_t work_done;
    int shutdown;
} JobQueue;

typedef struct {
    pthread_t *threads;
    int num_threads;
    JobQueue queue;
} ThreadPool;

ThreadPool* threadPoolCreate(int num_threads);
int threadPoolAddJob(ThreadPool *pool, void (*function)(void*), void *arg);
void threadPoolWait(ThreadPool *pool);
void threadPoolDestroy(ThreadPool *pool);

#endif // THREADPOOL_H
