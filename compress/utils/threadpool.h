#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stddef.h>

/**
 * THREAD POOL - Sistema de workers reutilizables para procesamiento paralelo
 * 
 * Concepto:
 *   En vez de crear 1 thread por tarea (costoso si hay miles de tareas),
 *   creamos N threads al inicio que se quedan esperando trabajo.
 *   Les damos trabajos mediante una cola thread-safe.
 * 
 * Flujo:
 *   1. threadPoolCreate(8)      → Crea 8 workers esperando
 *   2. threadPoolAddJob(...)     → Añade trabajo a la cola
 *   3. Workers toman jobs y los ejecutan automáticamente
 *   4. threadPoolWait()          → Espera a que terminen todos los jobs
 *   5. threadPoolDestroy()       → Limpia recursos
 */

/**
 * Job - Representa un trabajo a ejecutar
 * 
 * Es un nodo de linked list que contiene:
 *   - Función a ejecutar
 *   - Argumento para esa función
 *   - Puntero al siguiente job (para la cola)
 */
typedef struct Job {
    void (*function)(void *arg);  // Puntero a función que toma void* y retorna void
    void *arg;                    // Argumento que se pasa a la función
    struct Job *next;             // Siguiente job en la cola (linked list)
} Job;

/**
 * JobQueue - Cola thread-safe de trabajos
 * 
 * Estructura protegida por mutex que almacena jobs pendientes.
 * Los workers compiten por tomar jobs de aquí.
 */
typedef struct {
    Job *head;                    // Primer job en la cola (NULL si vacía)
    Job *tail;                    // Último job en la cola
    int count;                    // Cantidad de jobs en la cola
    int working;                  // Cantidad de threads actualmente ejecutando jobs
    pthread_mutex_t lock;         // Mutex para proteger acceso a la cola
    pthread_cond_t notify;        // Condition var para despertar workers cuando hay trabajo
    pthread_cond_t work_done;     // Condition var para señalizar cuando todo está completo
    int shutdown;                 // Flag: 1 = ordenar a workers que terminen
} JobQueue;

/**
 * ThreadPool - El pool completo de workers
 */
typedef struct {
    pthread_t *threads;           // Array de thread IDs
    int num_threads;              // Cantidad de workers
    JobQueue queue;               // Cola compartida de trabajos
} ThreadPool;

/**
 * Crea un thread pool con N workers
 * 
 * @param num_threads Cantidad de workers a crear (recomendado: núcleos CPU)
 * @return Puntero al ThreadPool creado, o NULL si error
 * 
 * Ejemplo:
 *   ThreadPool *pool = threadPoolCreate(8);  // 8 workers
 */
ThreadPool* threadPoolCreate(int num_threads);

/**
 * Añade un trabajo al thread pool
 * 
 * El trabajo se ejecutará en algún momento por un worker disponible.
 * Esta función retorna inmediatamente (no espera a que se ejecute).
 * 
 * @param pool El thread pool
 * @param function Función a ejecutar (debe tener firma: void func(void *arg))
 * @param arg Argumento que se pasará a la función
 * @return 0 si éxito, -1 si error
 * 
 * Ejemplo:
 *   void miTrabajo(void *arg) {
 *       int *num = (int*)arg;
 *       printf("Procesando: %d\n", *num);
 *   }
 *   
 *   int data = 42;
 *   threadPoolAddJob(pool, miTrabajo, &data);
 */
int threadPoolAddJob(ThreadPool *pool, void (*function)(void*), void *arg);

/**
 * Espera a que todos los trabajos pendientes terminen
 * 
 * Bloquea hasta que:
 *   - La cola esté vacía
 *   - Ningún worker esté ejecutando trabajo
 * 
 * @param pool El thread pool
 * 
 * Ejemplo:
 *   // Añadir 100 jobs
 *   for (int i = 0; i < 100; i++) {
 *       threadPoolAddJob(pool, trabajo, &data[i]);
 *   }
 *   
 *   threadPoolWait(pool);  // Esperar a que los 100 terminen
 *   printf("Todos completados!\n");
 */
void threadPoolWait(ThreadPool *pool);

/**
 * Destruye el thread pool y libera recursos
 * 
 * Secuencia:
 *   1. Señaliza a todos los workers que deben terminar
 *   2. Espera a que todos los threads terminen (join)
 *   3. Libera memoria
 * 
 * IMPORTANTE: Asegúrate de que todos los jobs hayan terminado antes
 * (o llama threadPoolWait() primero)
 * 
 * @param pool El thread pool a destruir
 */
void threadPoolDestroy(ThreadPool *pool);

#endif // THREADPOOL_H
