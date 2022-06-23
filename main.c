#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <threads.h>

#include "utils.h"
#include "mymemmalloc.h"

#define XSTR(s) STR(s)
#define STR(s) #s

/* default percentage of reads */
#define DEFAULT_READS 80
#define DEFAULT_UPDATES 20

/* default number of threads */
#define DEFAULT_NUM_THREADS 1

/* default experiment duration in miliseconds */
#define DEFAULT_DURATION 1000


static uint32_t finds;
static uint32_t malloc_size = 8;
static uint32_t size = 806400;
static uint32_t count = 1;

/* used to signal the threads when to stop */
static ALIGNED(64) uint8_t running[64];



pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;




/* a simple barrier implementation, used to make sure all threads start the
 * experiment at the same time.
 */
typedef struct barrier {
    pthread_cond_t complete;
    pthread_mutex_t mutex;
    int count;
    int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
    pthread_mutex_lock(&b->mutex);
    b->crossing++;                /* One more thread through */
    if (b->crossing < b->count) { /* If not all here, wait */
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        pthread_cond_broadcast(&b->complete);
        b->crossing = 0; /* Reset for next time */
    }
    pthread_mutex_unlock(&b->mutex);
}

/* data structure through which we send parameters to and get results from the
 * worker threads.
 */
typedef ALIGNED(64) struct thread_data {
    barrier_t *barrier;  /* pointer to the global barrier */
    uintptr_t *pad;
    uintptr_t *address;
    unsigned long n_ops; /* operations each thread performs */
    unsigned long n_malloc;
    unsigned long n_free;
    uintptr_t *pad2;
    uintptr_t *pad3;
} thread_data_t;



void *testthrouput(void *data)
{
    thread_data_t *d = (thread_data_t *) data; /* per-thread data */

    int last = -1;

    char *ptr = NULL;
    vm_head_t vm;

    int i = 0;

    /* Wait on barrier */
    barrier_cross(d->barrier);
    while (*running) { /* start the test */

        if(last == -1) { // malloc
            #ifdef MY_MALLOC
            ptr = vm_add(malloc_size,&vm);
            #else
            ptr = malloc(malloc_size);
            #endif
            d->address[i] = ptr;
            *ptr = i++;
            d->n_malloc++;
            if(i >= count) {
                i--;
                last = 1;
            }
        } else {
            #ifdef MY_MALLOC
            vm_remove(d->address[i],malloc_size,&vm);
            #else
            free(d->address[i]);
            #endif
            i--;
            d->n_free++;
            if(i < 0) {
                i = 0;
                last = -1;
            }
        }
        d->n_ops++; 
    }
    return NULL;
}


void catcher(int sig)
{
    static int nb = 0;
    printf("CAUGHT SIGNAL %d\n", sig);
    if (++nb >= 3)
        exit(1);
}

int main(int argc, char *const argv[])
{
    printf("%d\n",sizeof(thread_data_t));

    pthread_t *threads;
    pthread_attr_t attr;
    barrier_t barrier;
    struct timeval start, end;
    struct timespec timeout;

    thread_data_t *data;
    sigset_t block_set;

     pthread_mutex_init(&mutex, 0);

    /* initially, set parameters to their default values */
    int n_threads = DEFAULT_NUM_THREADS;
    uint32_t updates = DEFAULT_UPDATES;
    finds = DEFAULT_READS;
    int duration = DEFAULT_DURATION;
    
    /* now read the parameters in case the user provided values for them.
     * we use getopt, the same skeleton may be used for other bechmarks,
     * though the particular parameters may be different.
     */
    struct option long_options[] = {
        /* These options don't set a flag */
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"malloc-size", required_argument, NULL, 's'},
        {"malloc-free-pair-count", required_argument, NULL, 'c'},
        {"num-threads", required_argument, NULL, 'n'},
        {"updates", required_argument, NULL, 'u'},
        {NULL, 0, NULL, 0}};

    /* actually get the parameters form the command-line */
    while (1) {
        int i = 0;
        int c = getopt_long(argc, argv, "hd:n:u:c:s:", long_options, &i);
        if (c == -1) // all parsed
            break;
        if (c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch (c) {
        case 0:
            /* Flag is automatically set */
            break;
        case 'h':
            printf("stress test\n"
                   "\n"
                   "Usage:\n"
                   "  %s [options...]\n"
                   "\n"
                   "Options:\n"
                   "  -h, --help\n"
                   "        Print this message\n"
                   "  -d, --duration <int>\n"
                   "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                   "  -u, --updates <int>\n"
                   "        Percentage of update operations (default=" XSTR(DEFAULT_UPDATES) ")\n"
                   "  -s, --mallocsize <int>\n"
                   "        malloc size (default=8)\n"
                   "  -c, --mallocfreepair <int>\n"
                   "        malloc-free-pair count (default=1)\n"
                   "  -n, --num-threads <int>\n"
                   "        Number of threads (default=" XSTR(DEFAULT_NUM_THREADS) ")\n",
		   argv[0]
            );
            exit(0);
        case 'd':
            duration = atoi(optarg);
            break;
        case 'u':
            updates = atoi(optarg);
            finds = 100 - updates;
            break;
        case 's':
            malloc_size = atoi(optarg);
            break;
        case 'c':
            count = atoi(optarg);
            break;
        case 'n':
            n_threads = atoi(optarg);
            break;
        case '?':
            printf("Use -h or --help for help\n");
            exit(0);
        default:
            exit(1);
        }
    }

    size = size / n_threads;
    uintptr_t address[n_threads][size];





    /* initialize the data which will be passed to the threads */
    if ((data = malloc(n_threads * sizeof(thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    if ((threads = malloc(n_threads * sizeof(pthread_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    /* flag signaling the threads until when to run */
    *running = 1;

    /* global barrier init (used to start the threads at the same time) */
    barrier_init(&barrier, n_threads + 1);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    timeout.tv_sec = duration / 1000;
    timeout.tv_nsec = (duration % 1000) * 1000000;

    /* set the data for each thread and create the threads */
    for (int i = 0; i < n_threads; i++) {
        // data[i].id = i;
        data[i].n_ops = 0;
        data[i].address = &address[i];
        data[i].n_malloc = 0;
        data[i].n_free = 0;
        data[i].barrier = &barrier;
        if (pthread_create(&threads[i], &attr, testthrouput, (void *) (&data[i])) !=
            0) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }
    }
    pthread_attr_destroy(&attr);

    /* Catch some signals */
    if (signal(SIGHUP, catcher) == SIG_ERR ||
        signal(SIGINT, catcher) == SIG_ERR ||
        signal(SIGTERM, catcher) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    /* Start threads */
    barrier_cross(&barrier);
    gettimeofday(&start, NULL);
    if (duration > 0) {
        /* sleep for the duration of the experiment */
        nanosleep(&timeout, NULL);
    } else {
        sigemptyset(&block_set);
        sigsuspend(&block_set);
    }

    /* signal the threads to stop */
    *running = 0;
    gettimeofday(&end, NULL);

    /* Wait for thread completion */
    for (int i = 0; i < n_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error waiting for thread completion\n");
            exit(1);
        }
    }

    /* compute the exact duration of the experiment */
    duration = (end.tv_sec * 1000 + end.tv_usec / 1000) -
               (start.tv_sec * 1000 + start.tv_usec / 1000);

    unsigned long operations = 0;
    long reported_total = 0;
    /* report some experiment statistics */
    for (int i = 0; i < n_threads; i++) {
        printf("Thread %d\n", i);
        printf("  #operations   : %lu\n", data[i].n_ops);
        printf("  #malloc   : %lu\n", data[i].n_malloc);
        printf("  #free   : %lu\n", data[i].n_free);
        operations += data[i].n_ops;
    }

    printf("Duration      : %d (ms)\n", duration);
    printf("#txs     : %lu (%f / s)\n", operations,
           operations * 1000.0 / duration);

    free(threads);
    free(data);

    return 0;
}
