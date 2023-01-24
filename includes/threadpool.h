#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <pthread.h>
#include <boundedqueue.h>

// struttura contenente le informazioni da passare ai thread del pool
typedef struct _thread_args_t
{
    int tid;
    BQueue_t *Q;
} thread_args_t;

// oggetto threadpool
typedef struct _threadpool_t
{
    pthread_t *tid_array;
    thread_args_t *args;
} threadpool_t;

/**
 * @brief Crea un threadpool.
 * @param n_threads Numero dei thread nel pool.
 * @param Q Coda concorrente.
 * @param fun Funzione da cui iniziano l'esecuzione i thread. 
 * @return Pool di thread se termina con successo,
 *      NULL in caso di errore.
 */
threadpool_t *create_threadpool(int n_threads, BQueue_t *Q, void *(*fun)(void *arg));

/**
 * @brief Distrugge un threadpool.
 * @param pool Oggetto da liberare.
 */
void destroy_threadpool(threadpool_t *pool);

#endif