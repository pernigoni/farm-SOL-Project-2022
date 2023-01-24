#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/time.h>
#include <pthread.h>

#ifndef MAX_FILENAME_LEN
#define MAX_FILENAME_LEN 255
#endif

#ifndef SOCKET_NAME
#define SOCKET_NAME "./farm.sck"
#endif

#ifndef MAX_BACKLOG
#define MAX_BACKLOG 32
#endif

#ifdef DEBUG
#undef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define debug_printf(...) ((void)(DEBUG && printf(__VA_ARGS__))) // così non dà warning per le variabili non usate

#ifndef ANSI_COLOR_RED
#define ANSI_COLOR_RED   "\x1b[31m"
#endif

#ifndef ANSI_COLOR_GREEN
#define ANSI_COLOR_GREEN "\x1b[32m"
#endif

#ifndef ANSI_COLOR_BLUE
#define ANSI_COLOR_BLUE "\x1b[34m"
#endif

#ifndef ANSI_COLOR_RESET
#define ANSI_COLOR_RESET "\x1b[0m"
#endif

/*  name: nome della syscall
    value: dove sarà salvato il risultato della syscall
    system_call: syscall con tutti i suoi parametri
    error_msg: messaggio di errore
    file: __FILE__
    line: __LINE__ 
*/
#define SYSCALL_EXIT(name, value, system_call, error_msg, file, line)   \
    if((value = system_call) == -1)                                     \
    {                                                                   \
        perror(#name);                                                  \
        int aux_errno = errno;                                          \
        print_error(error_msg, file, line);                             \
        exit(aux_errno);                                                \
    }

#define SYSCALL_RETURN(name, value, system_call, error_msg, file, line) \
    if((value = system_call) == -1)                                     \
    {                                                                   \
        perror(#name);                                                  \
        int aux_errno = errno;                                          \
        print_error(error_msg, file, line);                             \
        errno = aux_errno;                                              \
        return value;                                                   \
    }

#define SYSCALL_PRINT(name, value, system_call, error_msg, file, line)  \
    if((value = system_call) == -1)                                     \
    {                                                                   \
        perror(#name);                                                  \
        int aux_errno = errno;                                          \
        print_error(error_msg, file, line);                             \
        errno = aux_errno;                                              \
    }

typedef struct _msg // messaggio
{
    int len_path;
    char *path;
    long result;
} msg_t;

long is_number(const char *s);

void msleep(int tms);

void print_error(char *msg, char *file, int line);

void *safe_malloc(size_t size);

void *safe_calloc(size_t nmemb, size_t size);

void *safe_realloc(void *ptr, size_t size);

void safe_pthread_mutex_lock(pthread_mutex_t *mtx);

void safe_pthread_mutex_unlock(pthread_mutex_t *mtx);

void safe_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx);

void safe_pthread_cond_signal(pthread_cond_t *cond);

void safe_pthread_cond_broadcast(pthread_cond_t *cond);

/**
 * @brief Evita letture parziali. 
 * @return size se termina con successo, 
 *      0 se durante la lettura da fd leggo EOF,
 *      -1 in caso di errore (setta errno).
 */
int readn(long fd, void *buf, size_t size);

/**
 * @brief Evita scritture parziali.
 * @return 1 se termina con successo,
 *      0 se durante la scrittura la write ritorna 0,
 *      -1 in caso di errore (setta errno).
 */
int writen(long fd, void *buf, size_t size);

// semplice lista di strighe
typedef struct _node    
{
    char data[1024];
    struct _node *next;
} node;

void add_to_list(node **list, node **last_elem, char *elem);

void delete_duplicates_from_list(node *list, int (*compare)(const char *, const char *));

void print_list(node *list, int slash_n);

void free_list(node **list);

#endif