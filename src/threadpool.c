#include <threadpool.h>

threadpool_t *create_threadpool(int n_threads, BQueue_t *Q, void *(*fun)(void *arg))
{
    if(n_threads <= 0 || Q == NULL)
        return NULL;

    threadpool_t *pool = malloc(sizeof(threadpool_t));
    if(pool == NULL)
        return NULL;
    
    pool->tid_array = malloc(n_threads * sizeof(pthread_t));
    if(pool->tid_array == NULL)
        return NULL;

    pool->args = malloc(n_threads * sizeof(thread_args_t));
    if(pool->args == NULL)
        return NULL;

    for(int i = 0; i < n_threads; i++)
    {
        (pool->args)[i].tid = i;
        (pool->args)[i].Q = Q;
    }
    for(int i = 0; i < n_threads; i++)
        if(pthread_create(&(pool->tid_array)[i], NULL, fun, &(pool->args)[i]) != 0)
        {
            perror("pthread_create");
            return NULL;
        }
    return pool;
}

void destroy_threadpool(threadpool_t *pool)
{
    free(pool->tid_array);
    free(pool->args);
    free(pool);
}