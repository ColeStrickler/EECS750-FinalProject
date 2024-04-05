# Extending the Race Window
Necessary to maximize time when speculative uaf can be triggered

## Mechanism
- Schedule a high precision timer (ns resolution) using `timerfd`
- Monitor with `epoll` which ties its watches to file descriptors
- Create long wait queues by duplicating (using `dup`) the timerfd file descriptor and adding it to an `epoll` instance 
- Timer should be configured to expire during the target thread execution in order to preempt it

## Usage
```sh
$ clang extend-window.c -pthread -o extend-window
$ ./extend-window [-e epoll instances] [-f duplicate fds per epoll instace]
```

## References
[Racing aginst the lock - Google Project Zero](https://googleprojectzero.blogspot.com/2022/03/racing-against-clock-hitting-tiny.html)
[Epoll Tutorial](https://suchprogramming.com/epoll-in-3-easy-steps/)
