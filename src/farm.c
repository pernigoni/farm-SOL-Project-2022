#define _GNU_SOURCE

#include <utils.h>
#include <boundedqueue.h>
#include <threadpool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <collector.h>

#define DEFAULT_N 4
#define DEFAULT_Q 8
#define DEFAULT_T 0

int count_exited_threads = 0; // variabile condivisa
pthread_mutex_t mtx_count = PTHREAD_MUTEX_INITIALIZER;

int collector_must_print = 0; // variabile condivisa
pthread_mutex_t mtx_collector_print = PTHREAD_MUTEX_INITIALIZER;

// struttura contenente le informazioni da passare al signal handler thread
typedef struct _sig_handler_t
{
    sigset_t *set; // set dei segnali da gestire (mascherati)
    int *soft_term; // vale 1 se ho ricevuto SIGHUP, SIGINT, SIGQUIT, SIGTERM; 0 altrimenti
} sig_handler_t; 

void print_parsing_usage()
{
    fprintf(stderr, "usage:  prog -n <nthread> -q <qlen> -d <dir-name> -t <delay>\n");
}

int parsing(int argc, char *argv[], int *n_val, int *q_val, int *t_val, char **d_val, node **filename_list)
{
    int opt,
        n_flag = 0, // 1 se ho trovato -n, 0 altrimenti
        q_flag = 0, // 1 se ho trovato -q, 0 altrimenti
        d_flag = 0, // 1 se ho trovato -d, 0 altrimenti
        t_flag = 0; // 1 se ho trovato -t, 0 altrimenti

    opterr = 0; // altrimenti getopt di default stampa errore quando trova un comando non riconosciuto

    for(int i = 1; i < argc; i++)
    {   
        // #define _GNU_SOURCE altrimenti getopt si ferma al primo non-argomento
        opt = getopt(argc, argv, "n:q:d:t:");
        switch(opt)
        {   // in optarg c'è l'argomento di -n/q/d/t
            case 'n':
                if(n_flag == 1)
                {
                    fprintf(stderr, "ERRORE: opzione \"-n\" settata più volte\n");
                    print_parsing_usage();
                    return -1;
                }
                else if((*n_val = is_number(optarg)) == -1 || *n_val <= 0)
                {   // optarg non è un numero
                    fprintf(stderr, "ERRORE: opzione \"-n\" deve essere seguita da un intero positivo\n");
                    print_parsing_usage();
                    return -1;
                }
                else
                    n_flag = 1;
                break;
            case 'q':
                if(q_flag == 1)
                {
                    fprintf(stderr, "ERRORE: opzione \"-q\" settata più volte\n");
                    print_parsing_usage();
                    return -1;
                }
                else if((*q_val = is_number(optarg)) == -1 || *q_val <= 0)
                {   // optarg non è un numero
                    fprintf(stderr, "ERRORE: opzione \"-q\" deve essere seguita da un intero positivo\n");
                    print_parsing_usage();
                    return -1;
                }
                else
                    q_flag = 1;
                break;
            case 'd':
                if(d_flag == 1)
                {
                    fprintf(stderr, "ERRORE: opzione \"-d\" settata più volte\n");
                    print_parsing_usage();
                    return -1;
                }
                else
                {
                    *d_val = optarg;
                    d_flag = 1;
                }
                break;
            case 't':
                if(t_flag == 1)
                {
                    fprintf(stderr, "ERRORE: opzione \"-t\" settata più volte\n");
                    print_parsing_usage();
                    return -1;
                }
                else if((*t_val = is_number(optarg)) == -1 || *t_val < 0)
                {   // optarg non è un numero
                    fprintf(stderr, "ERRORE: opzione \"-t\" deve essere seguita da un intero positivo\n");
                    print_parsing_usage();
                    return -1;
                }
                else
                    t_flag = 1;
                break;
            case '?':
            /*  Si entra qui per 2 motivi:
                    1) optarg di -n/q/d/t è vuoto.
                    2) opt sconosciuta.     */
                switch(optopt)
                {
                    case 'n':
                        fprintf(stderr, "ERRORE: opzione \"-n\" deve essere seguita da un intero positivo\n");
                        print_parsing_usage();
                        return -1;
                    case 'q':
                        fprintf(stderr, "ERRORE: opzione \"-q\" deve essere seguita da un intero positivo\n");
                        print_parsing_usage();
                        return -1;
                    case 'd':
                        fprintf(stderr, "ERRORE: opzione \"-d\" argomento mancante\n");
                        print_parsing_usage();
                        return -1;
                    case 't':
                        fprintf(stderr, "ERRORE: opzione \"-t\" deve essere seguita da un intero positivo\n");
                        print_parsing_usage();
                        return -1;
                    default:
                        fprintf(stderr, "ERRORE: opzione \"-%c\" sconosciuta\n", optopt);
                        print_parsing_usage();
                        return -1;
                }
        }
    }

    node *last_elem = NULL;
    for(int i = optind; i < argc; i++)
    {
        if(strlen(argv[i]) > MAX_FILENAME_LEN)
            fprintf(stderr, "ERRORE: nome file \"%s\" troppo lungo\n", argv[i]);
        else
            add_to_list(filename_list, &last_elem, argv[i]);
    }
    return 0;
}

int is_regular_file(const char *path)
{
    struct stat statbuf;
    int r;
    SYSCALL_RETURN(stat, r, stat(path, &statbuf), "stat", __FILE__, __LINE__);
    return S_ISREG(statbuf.st_mode);
}

int is_directory(const char *path)
{
    struct stat statbuf;
    int r;
    SYSCALL_RETURN(stat, r, stat(path, &statbuf), "stat", __FILE__, __LINE__);
    return S_ISDIR(statbuf.st_mode);
}

int is_dot(const char *dir)
{
    int len = strlen(dir);
    if(len > 0 && dir[len - 1] == '.')
        return 1;
    return 0;
}

// naviga la directory, stampa il contenuto e inserisce in una lista i file regolari contenuti al suo interno
void browse_directory(const char *dirname, node **list, node **last_elem)
{   // dirname DEVE essere il nome di una cartella esistente, fare i controlli fuori
    DIR *dir;
    if((dir = opendir(dirname)) == NULL)
    {
        perror("opendir");
        return ;
    }

    struct dirent *file;
    while((errno = 0, file = readdir(dir)) != NULL)
    {
        struct stat statbuf;
        char filename[MAX_FILENAME_LEN];
        int len1 = strlen(dirname), len2 = strlen(file->d_name);
        if((len1 + len2 + 2) > MAX_FILENAME_LEN)
        {
            fprintf(stderr, "ERRORE: nome file \"%s/%s\" troppo lungo\n", dirname, file->d_name);
            closedir(dir);
            return ;
        }

        strncpy(filename, dirname, MAX_FILENAME_LEN - 1);
        strncat(filename, "/", MAX_FILENAME_LEN - 1);
        strncat(filename, file->d_name, MAX_FILENAME_LEN - 1);

        int r;
        SYSCALL_PRINT(stat, r, stat(filename, &statbuf), "stat", __FILE__, __LINE__);
        if(r == -1)
        {
            closedir(dir);
            return ;
        }

        if(S_ISDIR(statbuf.st_mode))
        {
            if(is_dot(filename) == 0)
                browse_directory(filename, list, last_elem);
        }
        else
        {
            if(is_regular_file(filename) == 1)
            {
                debug_printf("%-30s è un file regolare\n", filename);
                add_to_list(list, last_elem, filename);
            }
            else
                debug_printf("%-30s non è un file regolare\n", filename);
        }
    }

    if(errno != 0)
        perror("readdir");
    closedir(dir);
}

// 1 se le due stringhe sono uguali, 0 altrimenti
int str_eq(const char *s1, const char *s2)
{
    if(strlen(s1) == strlen(s2) && strncmp(s1, s2, strlen(s1)) == 0)
        return 1;
    return 0;
}

// 1 se i due file sono lo stesso file, 0 altrimenti
int same_file(const char *f1, const char *f2)
{
    struct stat statbuf_f1, statbuf_f2;
    int r;
    SYSCALL_RETURN(stat, r, stat(f1, &statbuf_f1), "stat", __FILE__, __LINE__);
    SYSCALL_RETURN(stat, r, stat(f2, &statbuf_f2), "stat", __FILE__, __LINE__);
    if(statbuf_f1.st_dev == statbuf_f2.st_dev && statbuf_f1.st_ino == statbuf_f2.st_ino)
        return 1; // stesso device e stesso i-nodo
    return 0;
}

// calcola il risultato da inviare al processo Collector
long calculate_result(const char *path, int *err_flag)
{
    *err_flag = 0;
    FILE *fp_bin;
    if((fp_bin = fopen(path, "rb")) == NULL)
    {
        *err_flag = 1;
        perror("fopen");
        fprintf(stderr, "ERRORE: impossibile aprire il file %s\n", path);
        return -1;
    }    

    long buf, res = 0, i = 0;
    while(1)
    {
        fread(&buf, sizeof(long), 1, fp_bin);
        if(feof(fp_bin))
            break;
        res += i * buf;
        i++;
    }
    fclose(fp_bin);
    return res;    
}

void *sig_handler_fun(void *arg)
{
    assert(arg);
    sigset_t *set = ((sig_handler_t *)arg)->set; // set dei segnali da gestire (mascherati)
    int *soft_term = ((sig_handler_t *)arg)->soft_term;

    while(1)
    {
        int signum, r;
        if((r = sigwait(set, &signum)) != 0)
        {
            errno = r;
            perror("sigwait");
            return NULL;
        }

        if(signum == SIGUSR1)
        {
            debug_printf(ANSI_COLOR_RED "Ricevuto SIGUSR1" ANSI_COLOR_RESET "\n");

            safe_pthread_mutex_lock(&mtx_collector_print);
            collector_must_print++;
            safe_pthread_mutex_unlock(&mtx_collector_print);
        }
        else if(signum == SIGHUP || signum == SIGINT || signum == SIGQUIT || signum == SIGTERM)
        {
            if(DEBUG)
            {
                if(signum == SIGHUP)
                    debug_printf(ANSI_COLOR_RED "Ricevuto SIGHUP" ANSI_COLOR_RESET "\n");
                else if(signum == SIGINT)
                    debug_printf(ANSI_COLOR_RED "Ricevuto SIGINT" ANSI_COLOR_RESET "\n");
                else if(signum == SIGQUIT)
                    debug_printf(ANSI_COLOR_RED "Ricevuto SIGQUIT" ANSI_COLOR_RESET "\n");
                else if(signum == SIGTERM)
                    debug_printf(ANSI_COLOR_RED "Ricevuto SIGTERM" ANSI_COLOR_RESET "\n");
            }

            *soft_term = 1;            
            return NULL;
        }
    }
}

void *worker(void *arg)
{
    assert(arg);
    BQueue_t *Q = ((thread_args_t *)arg)->Q;
    int my_id = ((thread_args_t *)arg)->tid, consumed = 0, collector_print_flag = 0;

    while(1)
    {
        safe_pthread_mutex_lock(&mtx_collector_print);
        if(collector_must_print > 0)
        {
            collector_print_flag = 1;
            collector_must_print--;
        }
        safe_pthread_mutex_unlock(&mtx_collector_print);

        char *path = pop(Q);
        assert(path != NULL);
        if(strncmp(path, " ", strlen(" ")) == 0)
            break; // esco dal ciclo

        int sock_fd;
        SYSCALL_EXIT(socket, sock_fd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", __FILE__, __LINE__);

        struct sockaddr_un serv_addr;      
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sun_family = AF_UNIX;    
        strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME) + 1);

        int r;
        SYSCALL_EXIT(connect, r, connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "connect", __FILE__, __LINE__);

        char e;
        if(collector_print_flag) // notifico al processo Collector che deve stampare
        {
            collector_print_flag = 0;
            e = '2';
        }
        else // notifico al processo Collector che la lista dei file NON è finita
            e = '0';

        r = writen(sock_fd, &e, sizeof(char));
        /*  N.B. errno è settato a EPIPE quando si scrive su una pipe o su un socket 
                che non ha più nessun lettore (i lettori hanno chiuso la connessione).  */
        if(r == -1 && errno != EPIPE)
        {   
            perror("writen");
            print_error("writen", __FILE__, __LINE__);
            SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
            exit(errno);
        }

        consumed++;
        debug_printf("Worker %d popped: %s\n", my_id, path);

        int err_flag = 0;
        long result = calculate_result(path, &err_flag);
        
        int len_path = strlen(path) + 1; 
        SYSCALL_PRINT(writen, r, writen(sock_fd, &len_path, sizeof(int)), "writen", __FILE__, __LINE__);
        if(r == -1)
        {
            SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__); 
            break;
        }
        SYSCALL_PRINT(writen, r, writen(sock_fd, path, len_path * sizeof(char)), "writen", __FILE__, __LINE__);
        if(r == -1)
        {
            SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
            break;
        }

        if(err_flag == 0)
        {
            SYSCALL_PRINT(writen, r, writen(sock_fd, &result, sizeof(long)), "writen", __FILE__, __LINE__);
            if(r == -1)
            {
                SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
                break;
            }
        }
        else
        {   // per entrare forzatamente qui usare opzione delay e eliminare velocemente "a mano" un file regolare
            fprintf(stderr, "ERRORE: in calculate_result\n");

            result = -1;
            SYSCALL_PRINT(writen, r, writen(sock_fd, &result, sizeof(long)), "writen", __FILE__, __LINE__);
            if(r == -1)
            {
                SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
                break;
            }
        }

        debug_printf(ANSI_COLOR_GREEN "Worker %d sent: %d %s %ld" ANSI_COLOR_RESET "\n", my_id, len_path, path, result);
        SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
    }

    debug_printf("Worker %d, consumed %d messages, now it exits\n", my_id, consumed);

    safe_pthread_mutex_lock(&mtx_count);
    count_exited_threads++;
    safe_pthread_mutex_unlock(&mtx_count);

    return NULL;
}

void cleanup()
{
    unlink(SOCKET_NAME);
}

int main(int argc, char *argv[])
{
    cleanup();
    atexit(cleanup);

    debug_printf("PID: %d\n", getpid());

    // PARSING
    int n_val   = DEFAULT_N, // numero di thread Worker
        q_val   = DEFAULT_Q, // lunghezza della coda concorrente tra il Master e i Worker
        t_val   = DEFAULT_T; // millisecondi che intercorrono tra l'invio di due richieste successive ai Worker da parte del Master
    char *d_val = NULL; // nome di una directory

    node *filename_list = NULL; // lista dei filename passati al programma

    if(parsing(argc, argv, &n_val, &q_val, &t_val, &d_val, &filename_list) == -1)
        return EXIT_FAILURE;

    debug_printf("============================================================\n");
    debug_printf("-n: %d\n-q: %d\n-t: %d\n", n_val, q_val, t_val);
    if(d_val != NULL)
        debug_printf("-d: %s\n", d_val);
    if(filename_list != NULL)
    {
        delete_duplicates_from_list(filename_list, &str_eq);
        debug_printf("args: ");
        if(DEBUG)
            print_list(filename_list, 0); 
    }
    debug_printf("============================================================\n");

    // CONTROLLO FILE REGOLARI
    node *curr = filename_list, 
        *regfile_list = NULL, // lista di file regolari
        *last_elem = NULL;
    while(curr != NULL)
    {
        int is_reg = is_regular_file(curr->data);
        if(is_reg == -1)
            fprintf(stderr, "%-25s stat fallita\n", curr->data);
        else if(is_reg == 1)
        {
            debug_printf("%-25s è un file regolare\n", curr->data);
            add_to_list(&regfile_list, &last_elem, curr->data);
        }
        else
            debug_printf("%-25s non è un file regolare\n", curr->data);
        curr = curr->next;
    }
    free_list(&filename_list);
    debug_printf("============================================================\n");

    // NAVIGAZIONE DIRECTORY
    if(d_val != NULL)
    {
        int is_dir = is_directory(d_val);
        if(is_dir == -1)
            fprintf(stderr, "%-25s stat fallita\n", d_val);
        else if(is_dir == 1)
        {
            debug_printf("Navigo la directory \"%s\"\n", d_val);
            browse_directory(d_val, &regfile_list, &last_elem); // navigo la directory
        }
        else
            debug_printf("%-25s non è una directory\n", d_val);
    }
    debug_printf("============================================================\n");

    debug_printf("Lista di tutti i file regolari:\n"); // sia quelli passati come argomento che quelli presenti nella directory
    delete_duplicates_from_list(regfile_list, &same_file);
    if(DEBUG)
        print_list(regfile_list, 1);
    debug_printf("============================================================\n");

    // ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket
    struct sigaction s;
    memset(&s, 0, sizeof(s));    
    s.sa_handler = SIG_IGN;
    int r;
    SYSCALL_EXIT(sigaction, r, sigaction(SIGPIPE, &s, NULL), "sigaction", __FILE__, __LINE__);

    // maschero i segnali prima della fork in modo che il figlio non li veda
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);   // 1
    sigaddset(&mask, SIGINT);   // 2
    sigaddset(&mask, SIGQUIT);  // 3
    sigaddset(&mask, SIGUSR1);  // 10
    sigaddset(&mask, SIGTERM);  // 15
    if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
    {
        perror("pthread_sigmask");
        exit(EXIT_FAILURE);
    }

    /* ------------------------------------------------------------ */
    long listen_fd;
    SYSCALL_EXIT(socket, listen_fd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", __FILE__, __LINE__);

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME) + 1);
    
    // bind e listen nel main e non nel figlio (Collector) per essere sicuro di farli prima della connect dei Worker
    SYSCALL_EXIT(bind, r, bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "bind", __FILE__, __LINE__);
    SYSCALL_EXIT(listen, r, listen(listen_fd, MAX_BACKLOG), "listen", __FILE__, __LINE__);
    /* ------------------------------------------------------------ */

    // FORK
    int pid, status;
    fflush(stdout);
    if((pid = fork()) == -1) // errore
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    else if(pid == 0) // figlio
    {
        free_list(&regfile_list);
        collector(listen_fd);
    }
    else // padre
    {
        SYSCALL_EXIT(close, r, close(listen_fd), "close", __FILE__, __LINE__);

        // CREO IL SIGNAL HANDLER THREAD IN MODALITÀ DETACHED
        pthread_t sig_handler_thread;
        
        pthread_attr_t attr;
        if(pthread_attr_init(&attr) != 0) // inizializzo con gli attributi di default
        {
            perror("pthread_attr_init");
            return EXIT_FAILURE;
        }
        if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
        {
            perror("pthread_attr_setdetachstate");
            return EXIT_FAILURE;
        }

        int soft_term = 0;
        sig_handler_t handler_args = {&mask, &soft_term};
        if(pthread_create(&sig_handler_thread, &attr, sig_handler_fun, &handler_args) != 0)
        {
            perror("pthread_create");
            return EXIT_FAILURE;
        }

        // MASTER ---> CODA CONCORRENTE ---> WORKERS

        BQueue_t *Q = init_BQueue(q_val);
        if(Q == NULL)
        {
            fprintf(stderr, "ERRORE: inizializzando la coda\n");
            return EXIT_FAILURE;
        }

        threadpool_t *pool;
        if((pool = create_threadpool(n_val, Q, worker)) == NULL)
        {
            fprintf(stderr, "ERRORE: creando il threadpool\n");
            return EXIT_FAILURE;
        }

        int done = 0;
        
        curr = regfile_list;
        while(curr != NULL)
        {
            if(soft_term == 1)
                break;

            safe_pthread_mutex_lock(&mtx_count);
            done = (count_exited_threads == n_val);
            safe_pthread_mutex_unlock(&mtx_count);
            if(done)
                break;

            if(push(Q, &(curr->data)) == -1)
            {
                fprintf(stderr, "ERRORE: push nella coda\n");
                return EXIT_FAILURE;
            }
            debug_printf("Master pushed: %s\n", curr->data);
            curr = curr->next;

            if(t_val > 0)
                msleep(t_val);
                // usleep(t_val * 1000);
        }

        // notifico ai Worker che la lista dei file è finita
        char end_of_list[1] = " "; 
        if(!done)
            for(int i = 0; i < n_val; i++)
            {
                safe_pthread_mutex_lock(&mtx_count);
                done = (count_exited_threads == n_val);
                safe_pthread_mutex_unlock(&mtx_count);
                if(done)
                    break;

                if(push(Q, &end_of_list) == -1)
                {
                    fprintf(stderr, "ERRORE: push nella coda\n");
                    return EXIT_FAILURE;
                }
            }

        for(int i = 0; i < n_val; i++)
            if(pthread_join((pool->tid_array)[i], NULL) != 0)
            {
                perror("pthread_join");
                return EXIT_FAILURE;
            }

        delete_BQueue(Q, NULL);
        destroy_threadpool(pool);
        free_list(&regfile_list);

        /* ------------------------------------------------------------ */
        // notifico al processo Collector che deve uscire
        int sock_fd;
        SYSCALL_EXIT(socket, sock_fd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", __FILE__, __LINE__);

        struct sockaddr_un serv_addr;      
        memset(&serv_addr, '0', sizeof(serv_addr));
        serv_addr.sun_family = AF_UNIX;    
        strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME) + 1);

        SYSCALL_EXIT(connect, r, connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "connect", __FILE__, __LINE__);

        char e = '1';
        r = writen(sock_fd, &e, sizeof(char));
        /*  N.B. errno è settato a EPIPE quando si scrive su una pipe o su un socket 
                che non ha più nessun lettore (i lettori hanno chiuso la connessione).  */
        if(r == -1 && errno != EPIPE)
        {   
            perror("writen");
            print_error("writen", __FILE__, __LINE__);
            SYSCALL_EXIT(close, r, close(sock_fd), "close", __FILE__, __LINE__);
            exit(errno);
        }
        /* ------------------------------------------------------------ */

        // per far ritornare il signal handler thread nel caso non lo abbia già fatto
        SYSCALL_PRINT(kill, r, kill(getpid(), SIGINT), "kill", __FILE__, __LINE__);

        if((pid = waitpid(-1, &status, 0)) == -1)
        {
            perror("waitpid");
            return EXIT_FAILURE;
        }
        else if(pid != 0)
            if(WIFEXITED(status))
            {
                debug_printf(ANSI_COLOR_BLUE "Processo Collector %d figlio di %d terminato con status %d" ANSI_COLOR_RESET "\n", pid, getpid(), WEXITSTATUS(status));
                fflush(stdout);
            }        
    }

    return 0;
}