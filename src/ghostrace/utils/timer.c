#include "timer.h"

int **tfdups;
int timerfd;
int epollfds[EPOLL_INSTANCES];
struct itimerspec spec;
unsigned long timer_ns_delay;


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
		//printf("closing 1\n");
		close(epollfds[i]);
		if(tfdups[i] != NULL) {
			for(int j = 0; j < num_tfdups; j++)
			{
				//printf("closing2 %d\n", j);
				close(tfdups[i][j]);
			}
			//printf("free1 %d\n", i);
			free(tfdups[i]);
			tfdups[i] = NULL;
		}
	}
	//printf("free 2\n");
	if(tfdups != NULL) {
	    free(tfdups);
		tfdups = NULL;	
	}
	//pthread_spin_destroy(&lock);
	return 0;
}


void timer_init(uint32_t interval)
{
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

    // Add multiple FDs to epoll		
	if(init_tfdups(timerfd, tfdups, epollfds, EPOLL_INSTANCES, FD_PER_EPOLL)){
		fprintf(stderr, "Failed to init tfdups. Exiting\n");
		cleanup(epollfds, tfdups, EPOLL_INSTANCES, FD_PER_EPOLL);
		exit(1);
	}
    timer_ns_delay = interval;

    spec.it_interval.tv_nsec = 0;
    spec.it_interval.tv_sec = 0;
    spec.it_value.tv_nsec = timer_ns_delay; // ns as argument
    spec.it_value.tv_sec = 0;
}

/*
    start the timer on the victim CPU
*/
void timer_start()
{
    if(timerfd_settime(timerfd, 0, &spec, NULL)){
		printf("Error setting timer\n");
		exit(1);
	}
}


void timer_wait()
{
    // timer should expire here
    struct epoll_event events[FD_PER_EPOLL];
    for(int i=0; i < EPOLL_INSTANCES; i++){
    	if(epoll_wait(epollfds[i], events, FD_PER_EPOLL, -1) == -1){
    		printf("Error epoll waiting\n");
    		break;
    	}
       //begin_ipi_storm();
    }
}

void timer_cleanup()
{
    cleanup(epollfds, tfdups, EPOLL_INSTANCES, FD_PER_EPOLL);
}