// Necesario para POSIX threads
#define _POSIX_C_SOURCE 200809L

#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Worker thread en loop que crea y ejecuta trabajos
static void* workerThread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;  // Cast del argumento a ThreadPool
    
    while (1) {
        pthread_mutex_lock(&pool->queue.lock);
        
        while (pool->queue.head == NULL && !pool->queue.shutdown) {
            pthread_cond_wait(&pool->queue.notify, &pool->queue.lock);
        }
        
        // si hay shutdown y no hay trabajos chao
        if (pool->queue.shutdown && pool->queue.head == NULL) {
            pthread_mutex_unlock(&pool->queue.lock);
            break;  // Sale del while(1), el thread termina
        }
        
        // PASO 4: Tomar un job de la cola (dequeue)
        Job *job = pool->queue.head;
        if (job) {
            // Mover head al siguiente
            pool->queue.head = job->next;
            
            // Si era el último, actualizar tail
            if (pool->queue.head == NULL) {
                pool->queue.tail = NULL;
            }
            
            pool->queue.count--;
            pool->queue.working++;  // Incrementar contador de workers activos
        }
        
        //  Liberar el mutex ANTES de ejecutar el trabajo
        pthread_mutex_unlock(&pool->queue.lock);
        
        // PEjecutar el trabajo (sin lock = no bloquea otros threads)
        if (job) {
            job->function(job->arg);  // Llamar a la función con su argumento
            free(job);                // Liberar memoria del job
            
            // PASO 7: Notificar que terminamos un trabajo
            pthread_mutex_lock(&pool->queue.lock);
            pool->queue.working--;  // Decrementar contador de workers activos
            
            // Si ya no hay trabajos pendientes ni en ejecución, señalizar
            if (pool->queue.count == 0 && pool->queue.working == 0) {
                pthread_cond_signal(&pool->queue.work_done);
            }
            
            pthread_mutex_unlock(&pool->queue.lock);
        }
    }
    
    return NULL;
}

// Creación del threadPOol
ThreadPool* threadPoolCreate(int num_threads) {
    if (num_threads <= 0) {
        fprintf(stderr, "Error: num_threads debe ser > 0\n");
        return NULL;
    }
    
    // Alojar memoria para el pool
    ThreadPool *pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("malloc ThreadPool");
        return NULL;
    }
    
    pool->num_threads = num_threads;
    
    // Alojar array de pthread_t (Ids de threads)
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!pool->threads) {
        perror("malloc threads");
        free(pool);
        return NULL;
    }
    
    // Inicializar la cola de trabajos
    pool->queue.head = NULL;
    pool->queue.tail = NULL;
    pool->queue.count = 0;
    pool->queue.working = 0;
    pool->queue.shutdown = 0;
    
    // pthread_mutex_init: Inicializa un mutex
    pthread_mutex_init(&pool->queue.lock, NULL);
    
    // pthread_cond_init: Crea una condition variable
    // Se usa para que threads se despierten entre sí
    pthread_cond_init(&pool->queue.notify, NULL);
    pthread_cond_init(&pool->queue.work_done, NULL);
    
    // Crear los worker threads
    for (int i = 0; i < num_threads; i++) {
        // pthread_create: Crea un nuevo thread
        // Parámetros:
        //   1. &pool->threads[i] - Donde guardar el ID del thread
        //   2. NULL              - Atributos por defecto
        //   3. workerThread      - Función que ejecutará el thread
        //   4. pool              - Argumento que se pasa a workerThread
        if (pthread_create(&pool->threads[i], NULL, workerThread, pool) != 0) {
            perror("pthread_create");
            // Si falla, destruir lo creado hasta ahora
            threadPoolDestroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Añadir job al pool
int threadPoolAddJob(ThreadPool *pool, void (*function)(void*), void *arg) {
    if (!pool || !function) {
        return -1;
    }
    
    // Crear el job
    Job *job = malloc(sizeof(Job));
    if (!job) {
        perror("malloc Job");
        return -1;
    }
    
    job->function = function;
    job->arg = arg;
    job->next = NULL;
    
    // Añadir a la cola (operación crítica, necesita mutex)
    pthread_mutex_lock(&pool->queue.lock);
    
    // Añadir al final de la linked list
    if (pool->queue.tail) {
        pool->queue.tail->next = job;  // Enlazar el último job con el nuevo
    } else {
        pool->queue.head = job;        // Si estaba vacía, este es el primero
    }
    pool->queue.tail = job;            // Actualizar tail
    pool->queue.count++;
    
    // Despertar a UN worker que esté dormido
   // pthread_cond_signal: Despierta a 1 thread esperando en pthread_cond_wait
    pthread_cond_signal(&pool->queue.notify);
    
    pthread_mutex_unlock(&pool->queue.lock);
    
    return 0;
}

// Esperar a que todos los trabajos terminen
void threadPoolWait(ThreadPool *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue.lock);
    
    // Esperar mientras haya trabajos pendientes O en ejecución
    while (pool->queue.count > 0 || pool->queue.working > 0) {
        // pthread_cond_wait libera el mutex y duerme
        // Cuando work_done se señaliza, despierta y re-adquiere el mutex
        pthread_cond_wait(&pool->queue.work_done, &pool->queue.lock);
    }
    
    pthread_mutex_unlock(&pool->queue.lock);
}


void threadPoolDestroy(ThreadPool *pool) {
    if (!pool) return;
    

    // Señalizar shutdown
    pthread_mutex_lock(&pool->queue.lock);
    pool->queue.shutdown = 1;
    
    // pthread_cond_broadcast: Despierta a TODOS los threads esperando
    pthread_cond_broadcast(&pool->queue.notify);
    
    pthread_mutex_unlock(&pool->queue.lock);
    
    // Esperar a que todos los threads terminen
    for (int i = 0; i < pool->num_threads; i++) {
        // bloqueamos hasta q  el thread termine
        pthread_join(pool->threads[i], NULL);
    }
    
    // Limpiar jobs restantes (si los hay)
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
