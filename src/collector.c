#include <collector.h>

int msg_cmp(const void *a, const void *b)
{
    msg_t *m1 = (msg_t *)a;
    msg_t *m2 = (msg_t *)b;
    if(m1->result < m2->result)
        return -1;
    else if(m1->result > m2->result)
        return 1;
    else
        return 0;
}

// ordina l'array di messaggi in modo crescente di risultato e libera la memoria se free_flag è diverso da 0
void print_ordered_result(msg_t *msg_array, int j, int free_flag)
{
    qsort(msg_array, j, sizeof(msg_t), msg_cmp);
    printf("============================================================\n");
    printf("\t—————————————————————————————————\n");
    printf("\t  STAMPA DEL PROCESSO COLLECTOR  \n");
    printf("\t—————————————————————————————————\n");

    for(int i = 0; i < j; i++)
    {   
        printf("%-25ld %s\n", msg_array[i].result, msg_array[i].path);
        if(free_flag)
            free(msg_array[i].path);
    }
    if(free_flag)
        free(msg_array);
    printf("============================================================\n");
}

void collector(long listen_fd)
{
/*  long listen_fd;
    SYSCALL_EXIT(socket, listen_fd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", __FILE__, __LINE__);

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME) + 1);

    int r;
    SYSCALL_EXIT(bind, r, bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "bind", __FILE__, __LINE__);
    SYSCALL_EXIT(listen, r, listen(listen_fd, MAX_BACKLOG), "listen", __FILE__, __LINE__);  */
    
    int j = 0, exit_value = EXIT_SUCCESS, r;
    msg_t *msg_array = NULL; // array di messaggi ricevuti

    while(1)
    {
        msg_array = safe_realloc(msg_array, (j + 1) * sizeof(msg_t));

        long conn_fd;
        SYSCALL_EXIT(accept, conn_fd, accept(listen_fd, (struct sockaddr *)NULL, NULL), "accept", __FILE__, __LINE__);

        // controllo se devo uscire dal ciclo o se devo stampare
        char break_loop_or_print;
        SYSCALL_PRINT(readn, r, readn(conn_fd, &break_loop_or_print, sizeof(char)), "readn", __FILE__, __LINE__);
        if(r == -1)
        {
            exit_value = errno;
            SYSCALL_EXIT(close, r, close(conn_fd), "close", __FILE__, __LINE__);
            goto end;
        }

        if(break_loop_or_print == '1') // esco dal ciclo
            break;
        else if(break_loop_or_print == '2') // stampo i risultati ricevuti fino a questo momento
            print_ordered_result(msg_array, j, 0);
        
        SYSCALL_PRINT(readn, r, readn(conn_fd, &msg_array[j].len_path, sizeof(int)), "readn", __FILE__, __LINE__);
        if(r == -1 || r == 0)
        {
            exit_value = errno;
            SYSCALL_EXIT(close, r, close(conn_fd), "close", __FILE__, __LINE__);
            goto end;
        }
        msg_array[j].path = safe_calloc(msg_array[j].len_path, sizeof(char));
        SYSCALL_PRINT(readn, r, readn(conn_fd, msg_array[j].path, msg_array[j].len_path * sizeof(char)), "readn", __FILE__, __LINE__);
        if(r == -1)
        {
            exit_value = errno;
            SYSCALL_EXIT(close, r, close(conn_fd), "close", __FILE__, __LINE__);
            msg_array[j].result = -1; // per non lasciare valori non inizializzati
            j++;
            goto end;
        }
        SYSCALL_PRINT(readn, r, readn(conn_fd, &msg_array[j].result, sizeof(long)), "readn", __FILE__, __LINE__);
        if(r == -1)
        {
            exit_value = errno;
            SYSCALL_EXIT(close, r, close(conn_fd), "close", __FILE__, __LINE__);
            msg_array[j].result = -1; // per non lasciare valori non inizializzati
            j++;
            goto end;
        }

        SYSCALL_EXIT(close, r, close(conn_fd), "close", __FILE__, __LINE__);
        j++;
    }

end:
    print_ordered_result(msg_array, j, 1); // stampa finale

    SYSCALL_EXIT(close, r, close(listen_fd), "close", __FILE__, __LINE__);
    exit(exit_value);
}