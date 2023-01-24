#include <utils.h>

long is_number(const char *s) 
{
    char *e = NULL;
    long val = strtol(s, &e, 0);
    if(e != NULL && *e == (char)0) 
        return val; 
    return -1;
}

void msleep(int tms)
{
    struct timeval tv;
    tv.tv_sec  = tms / 1000;
    tv.tv_usec = (tms % 1000) * 1000;
    int r;
    SYSCALL_PRINT(select, r, select(0, NULL, NULL, NULL, &tv), "select", __FILE__, __LINE__);
}

void print_error(char *msg, char *file, int line)
{
    fprintf(stderr, "ERRORE: %s in %s, linea %d\n", msg, file, line);
}

void *safe_malloc(size_t size)
{
    void *buf;
    if((buf = malloc(size)) == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return buf;
}

void *safe_calloc(size_t nmemb, size_t size)
{
    void *buf;
    if((buf = calloc(nmemb, size)) == NULL)
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    return buf;
}

void *safe_realloc(void *ptr, size_t size)
{
    void *buf;
    if((buf = realloc(ptr, size)) == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    return buf;
}

void safe_pthread_mutex_lock(pthread_mutex_t *mtx)
{
    if(pthread_mutex_lock(mtx) != 0)
    {
        perror("pthread_mutex_lock");
        pthread_exit((void *)EXIT_FAILURE);
    }
}

void safe_pthread_mutex_unlock(pthread_mutex_t *mtx)
{
    if(pthread_mutex_unlock(mtx) != 0)
    {
        perror("pthread_mutex_unlock");
        pthread_exit((void *)EXIT_FAILURE);
    }
}

void safe_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx)
{
    if(pthread_cond_wait(cond, mtx) != 0)
    {
        perror("pthread_cond_wait");
        pthread_exit((void *)EXIT_FAILURE);
    }
}

void safe_pthread_cond_signal(pthread_cond_t *cond)
{
    if(pthread_cond_signal(cond) != 0)
    {
        perror("pthread_cond_signal");
        pthread_exit((void *)EXIT_FAILURE);
    }
}

void safe_pthread_cond_broadcast(pthread_cond_t *cond)
{
    if(pthread_cond_broadcast(cond) != 0)
    {
        perror("pthread_cond_broadcast");
        pthread_exit((void *)EXIT_FAILURE);
    }
}

int readn(long fd, void *buf, size_t size) 
{
    size_t left = size;
    int r;
    char *bufptr = (char *)buf;
    while(left > 0) 
    {
        if((r = read((int)fd, bufptr, left)) == -1) 
        {
            if(errno == EINTR) 
                continue;
            return -1;
        }
        if(r == 0) 
            return 0; // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}

int writen(long fd, void *buf, size_t size) 
{
    size_t left = size;
    int r;
    char *bufptr = (char *)buf;
    while(left > 0) 
    {
        if((r = write((int)fd, bufptr, left)) == -1) 
        {
            if (errno == EINTR) 
                continue;
            return -1;
        }
        if(r == 0) 
            return 0;  
        left -= r;
        bufptr += r;
    }
    return 1;
}

node *create_list(node *list, char *elem) // non visibile
{   // la lista deve essere vuota
    list = safe_malloc(sizeof(node));
    strncpy(list->data, elem, strlen(elem));
    (list->data)[strlen(elem)] = '\0';
    list->next = NULL;
    return list;
}

node *append_to_list(node *last_elem, char *elem) // non visibile
{
    last_elem->next = safe_malloc(sizeof(node));
    strncpy(last_elem->next->data, elem, strlen(elem));
    (last_elem->next->data)[strlen(elem)] = '\0';
    last_elem->next->next = NULL;
    return last_elem->next;
}

void add_to_list(node **list, node **last_elem, char *elem)
{
    if(*list == NULL)
    {
        *list = create_list(*list, elem);
        *last_elem = *list;
    }
    else
        *last_elem = append_to_list(*last_elem, elem);
}

void delete_duplicates_from_list(node *list, int (*compare)(const char *, const char *))
{
    node *ptr1 = list, *ptr2, *tmp;
    while(ptr1 != NULL && ptr1->next != NULL)
    {
        ptr2 = ptr1;
        while(ptr2->next != NULL)
        {
            if(compare(ptr1->data, (ptr2->next)->data) == 1)
            {
                tmp = ptr2->next;
                ptr2->next = ptr2->next->next;
                free(tmp);
            }
            else
                ptr2 = ptr2->next;
        }
        ptr1 = ptr1->next;
    }
}

void print_list(node *list, int slash_n)
{
    while(list != NULL)
    {
        printf("%s", list->data);
        slash_n == 0 ? printf(" ") : printf("\n");
        list = list->next;
    }
    if(slash_n == 0)
        printf("\n");    
}

void free_list(node **list)
{
    node *aux;
    while(*list != NULL)
    {
        aux = *list;
        *list = (*list)->next;
        free(aux);
    }
}