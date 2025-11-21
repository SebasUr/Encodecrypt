#define _POSIX_C_SOURCE 200809L

#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void* workerThread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->queue.lock);
        
        while (pool->queue.head == NULL && !pool->queue.shutdown) {
            pthread_cond_wait(&pool->queue.notify, &pool->queue.lock);
        }
        
        if (pool->queue.shutdown && pool->queue.head == NULL) {
            pthread_mutex_unlock(&pool->queue.lock);
            break;
        }
        
        Job *job = pool->queue.head;
        if (job) {
            pool->queue.head = job->next;
            
            if (pool->queue.head == NULL) {
                pool->queue.tail = NULL;
            }
            
            pool->queue.count--;
            pool->queue.working++;
        }
        
        pthread_mutex_unlock(&pool->queue.lock);
        
        if (job) {
            job->function(job->arg);
            free(job);
            
            pthread_mutex_lock(&pool->queue.lock);
            pool->queue.working--;
            
            if (pool->queue.count == 0 && pool->queue.working == 0) {
                pthread_cond_signal(&pool->queue.work_done);
            }
            
            pthread_mutex_unlock(&pool->queue.lock);
        }
    }
    
    return NULL;
}

ThreadPool* threadPoolCreate(int num_threads) {
    if (num_threads <= 0) {
        fprintf(stderr, "Error: num_threads must be > 0\n");
        return NULL;
    }
    
    ThreadPool *pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("malloc ThreadPool");
        return NULL;
    }
    
    pool->num_threads = num_threads;
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        perror("malloc threads");
        free(pool);
        return NULL;
    }
    
    pool->queue.head = NULL;
    pool->queue.tail = NULL;
    pool->queue.count = 0;
    pool->queue.working = 0;
    pool->queue.shutdown = 0;
    
    pthread_mutex_init(&pool->queue.lock, NULL);
    pthread_cond_init(&pool->queue.notify, NULL);
    pthread_cond_init(&pool->queue.work_done, NULL);
    
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, workerThread, pool) != 0) {
            perror("pthread_create");
            threadPoolDestroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

int threadPoolAddJob(ThreadPool *pool, void (*function)(void*), void *arg) {
    if (!pool || !function) {
        return -1;
    }
    
    Job *job = malloc(sizeof(Job));
    if (!job) {
        perror("malloc Job");
        return -1;
    }
    
    job->function = function;
    job->arg = arg;
    job->next = NULL;
    
    pthread_mutex_lock(&pool->queue.lock);
    
    if (pool->queue.tail) {
        pool->queue.tail->next = job;
    } else {
        pool->queue.head = job;
    }
    pool->queue.tail = job;
    pool->queue.count++;
    
    pthread_cond_signal(&pool->queue.notify);
    
    pthread_mutex_unlock(&pool->queue.lock);
    
    return 0;
}

void threadPoolWait(ThreadPool *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue.lock);
    
    while (pool->queue.count > 0 || pool->queue.working > 0) {
        pthread_cond_wait(&pool->queue.work_done, &pool->queue.lock);
    }
    
    pthread_mutex_unlock(&pool->queue.lock);
}

void threadPoolDestroy(ThreadPool *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue.lock);
    pool->queue.shutdown = 1;
    
    pthread_cond_broadcast(&pool->queue.notify);
    
    pthread_mutex_unlock(&pool->queue.lock);
    
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    Job *current = pool->queue.head;
    while (current) {
        Job *next = current->next;
        free(current);
        current = next;
    }
    
    pthread_mutex_destroy(&pool->queue.lock);
    pthread_cond_destroy(&pool->queue.notify);
    pthread_cond_destroy(&pool->queue.work_done);
    
    free(pool->threads);
    free(pool);
}
