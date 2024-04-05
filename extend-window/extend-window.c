#define _GNU_SOURCE
#include <stdio.h> 
#include <stdlib.h>
#include <sched.h> // cpu_set_t, setaffinity, etc. 
#include <unistd.h>  // for sleep()
#include <pthread.h> // for pthread_*()
#include <sys/types.h> // for tid_t
#include <sys/timerfd.h> // for timerfd_*()
#include <sys/select.h> // for select()
#include <time.h> // for time()
#include <sys/epoll.h> // for epoll_create1()

pthread_spinlock_t lock;

// goal -> preempt this thread using timer interrupt and extend the execution of spinTheThread
void *killTheThread(void *to_kill) {
	pthread_spin_lock(&lock);
	clock_t start = clock();
	pthread_t *thread = (pthread_t *)to_kill;
	if(pthread_cancel(*thread)) {
		printf("Failed to kill thread\n");
	}
	clock_t end = clock();
	pthread_spin_unlock(&lock);
	printf("Time to kill thread: %7.2fms\n", (double)10e3*(end-start)/CLOCKS_PER_SEC);
	pthread_exit(NULL);
}

void *spinTheThread() {
	pthread_spin_lock(&lock);
	int o_state;
	pid_t tid = gettid();
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(0, &cpu_set);
	sched_setaffinity(tid, sizeof(cpu_set), &cpu_set);
	// let killTheThread preempt this thread in the loop
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &o_state);
	sleep(10);
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

int main(int argc, char **argv) {
	if(argc < 5)
	{
		fprintf(stderr, "Usage: %s [-e nepolls] [-f fds per epoll]\n", argv[0]);
		exit(1);
	}
	int epoll_instances, fds_per_instance;
	int opt;
	while((opt = getopt(argc, argv, "e:f:")) != -1) {
		switch(opt) {
			case 'e':
				epoll_instances = atoi(optarg);
				break;
			case 'f':
				fds_per_instance = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Usage: %s [-e nepolls] [-f fds per epoll]\n", argv[0]);
				exit(1);
		}
	}
	printf("Running example with EPOLL INSTANCES: %d, FDS PER EPOLL: %d\n", epoll_instances, fds_per_instance);

	// Two thread ids this example uses
	pthread_t spin_thread, kill_thread;

	// Initialize epoll instances
	int epollfds[epoll_instances];
	if(init_epoll(epollfds, epoll_instances))
	{
		fprintf(stderr, "Failed to init epolls. Exiting\n");
		exit(1);
	}
	
	int **tfdups = (int **)malloc(epoll_instances * sizeof(int *));
	for(int i=0; i< epoll_instances; i++){
		tfdups[i] = (int *)malloc(fds_per_instance * sizeof(int));
	}

	// use the system realtime clock for hpt
	int tfd = timerfd_create(CLOCK_REALTIME, 0);
	if(tfd > 0) {
		cpu_set_t cpu_set;
		CPU_ZERO(&cpu_set);
		CPU_SET(1, &cpu_set);
		sched_setaffinity(getpid(), sizeof(cpu_set), &cpu_set);

		if(pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE)) {
			fprintf(stderr, "Failed to init spin\n");
			exit(1);
		}

		struct itimerspec spec;
		struct timespec frequency = {0, 0};
		// the code between timerfr_settime and pthread_create for the killthread takes about ~200K ns
		// adjust as needed depending on machine 
		struct timespec initial = {0, 200000}; // 0s, 200000ns
		spec.it_interval = frequency;
		spec.it_value = initial;
		
		// Add multiple FDs to epoll		
		if(init_tfdups(tfd, tfdups, epollfds, epoll_instances, fds_per_instance)){
			fprintf(stderr, "Failed to init tfdups. Exiting\n");
			cleanup(epollfds, tfdups, epoll_instances, fds_per_instance);
			exit(1);
		}
		// start timer
		if(timerfd_settime(tfd, 0, &spec, NULL)){
			printf("Error setting timer\n");
			exit(1);
		}

		// Start the spinning thread and occupy core 0 with it
		pthread_create(&spin_thread, NULL, spinTheThread, NULL);	
		printf("(1) Spinning Thread\n");	
		// Kill spinning thread	
		pthread_create(&kill_thread, NULL, killTheThread, &spin_thread);
		printf("(2) Killing Thread\n");
		
		// timer should expire here
		struct epoll_event events[fds_per_instance];
		for(int i=0; i<epoll_instances; i++){
			if(epoll_wait(epollfds[i], events, fds_per_instance, -1) == -1){
				printf("Error epoll waiting\n");
				break;
			}
		}	

		pthread_join(kill_thread, NULL);
		printf("(3) After kill thread\n");
		
		pthread_join(spin_thread, NULL);
		printf("(4) After spin thread\n");
	}
	cleanup(epollfds, tfdups, epoll_instances, fds_per_instance);
	pthread_exit(NULL);
}
