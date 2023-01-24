#ifndef _BOUNDED_QUEUE_H
#define _BOUNDED_QUEUE_H

#include <pthread.h>
#include <assert.h>
#include <utils.h>

typedef struct BQueue 
{
    void **buf;
    size_t head;
    size_t tail;
    size_t qsize;
    size_t qlen;
    pthread_mutex_t m;
    pthread_cond_t cfull;
    pthread_cond_t cempty;
} BQueue_t;

/*  Alloca e inizializza una coda di dimensione n.
    Deve essere chiamata da un solo thread (tipicamente il thread main).
    Ritorna il puntatore alla coda allocata o NULL in caso di errore (setta errno). */
BQueue_t *init_BQueue(size_t n);

/*  Cancella la coda puntata da q.
    Deve essere chiamata da un solo thread (tipicamente il thread main).    */
void delete_BQueue(BQueue_t *q, void (*F)(void *));

/*  Inserisce un dato nella coda.
    Ritorna 0 in caso di successo, -1 altrimenti (setta errno).   */
int push(BQueue_t *q, void *data);

/*  Estrae un dato dalla coda.
    Ritorna il puntatore al dato estratto.    */
void *pop(BQueue_t *q);

#endif