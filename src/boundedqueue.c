#include <boundedqueue.h>

static inline void lock_queue(BQueue_t *q)          
{
    safe_pthread_mutex_lock(&q->m);
}

static inline void unlock_queue(BQueue_t *q)        
{
    safe_pthread_mutex_unlock(&q->m);
}

static inline void wait_to_produce(BQueue_t *q)      
{
    safe_pthread_cond_wait(&q->cfull, &q->m);
}

static inline void wait_to_consume(BQueue_t *q)      
{
    safe_pthread_cond_wait(&q->cempty, &q->m);
}

static inline void signal_producer(BQueue_t *q)     
{
    safe_pthread_cond_signal(&q->cfull);
}

static inline void signal_consumer(BQueue_t *q)     
{
    safe_pthread_cond_signal(&q->cempty);
}

BQueue_t *init_BQueue(size_t n) 
{
    int aux_errno;
    BQueue_t *q = calloc(sizeof(BQueue_t), 1);
    if(q == NULL)
    {
        perror("calloc");
        return NULL;
    }

    q->buf = calloc(sizeof(void *), n); 
    if(q->buf == NULL)
    {
        perror("calloc");
        goto error;
    }

    if(pthread_mutex_init(&q->m, NULL) != 0)
    {
        perror("thread_mutex_init");
        goto error;
    }

    if (pthread_cond_init(&q->cfull, NULL) != 0) 
    {
        perror("pthread_cond_init full");
        goto error;
    }

    if (pthread_cond_init(&q->cempty,NULL) != 0) 
    {
        perror("pthread_cond_init empty");
        goto error;
    }

    q->head  = q->tail = 0;
    q->qlen  = 0;
    q->qsize = n;
    return q;

error:
    aux_errno = errno;
    if(q->buf) 
        free(q->buf);
    if(&q->m) 
        pthread_mutex_destroy(&q->m);
    if(&q->cfull) 
        pthread_cond_destroy(&q->cfull);
    if(&q->cempty) 
        pthread_cond_destroy(&q->cempty);
    free(q);
    errno = aux_errno;
    return NULL;
}

void delete_BQueue(BQueue_t *q, void (*F)(void *)) 
{
    if(q == NULL) 
    {
        errno = EINVAL;
        return ;
    }

    if(F) 
    {
        void *data = NULL;
        while((data = pop(q))) 
            F(data);
    }

    if(q->buf) 
        free(q->buf);
    if(&q->m) 
        pthread_mutex_destroy(&q->m);
    if(&q->cfull) 
        pthread_cond_destroy(&q->cfull);
    if(&q->cempty) 
        pthread_cond_destroy(&q->cempty);
    free(q);
}

int push(BQueue_t *q, void *data) 
{
    if(q == NULL || data == NULL) 
    {
        errno = EINVAL;
        return -1;
    }

    lock_queue(q);
    while(q->qlen == q->qsize) 
        wait_to_produce(q);
    assert(q->buf[q->tail] == NULL);
    q->buf[q->tail] = data;
    q->tail += (q->tail + 1 >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen += 1;
    signal_consumer(q);
    unlock_queue(q);

    return 0;
}

void *pop(BQueue_t *q) 
{
    if(q == NULL) 
    {
        errno = EINVAL;
        return NULL;
    }

    lock_queue(q);
    while(q->qlen == 0) 
        wait_to_consume(q);
    void *data = q->buf[q->head];
    q->buf[q->head] = NULL;
    q->head += (q->head + 1 >= q->qsize) ? (1 - q->qsize) : 1;
    q->qlen -= 1;
    assert(q->qlen >= 0);
    signal_producer(q);
    unlock_queue(q);

    return data;
}