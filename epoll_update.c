
/* These are debug messages */
static const char* __attribute__((unused)) epoll_update_msgs[] = {
    "    setting up %d to listen only for error events\n",
    "    setting up %d to listen for read events\n",
    "    setting up %d to listen for write events\n",
    "    setting up %d to listen for read and write events\n"};


static void epoll_update(int fd) {
    struct epoll_event ev;
    ev.events = EPOLLONESHOT;
    
    dpf(epoll_update_msgs[fdinfo[fd].we_should_epoll_for_reads + 2*fdinfo[fd].we_should_epoll_for_writes], fd);

    if (!fdinfo[fd].current_quota) {
	dpf("        but the quota is exhausted\n");
    }

    if (fdinfo[fd].we_should_epoll_for_reads && fdinfo[fd].current_quota) {
	ev.events |= EPOLLIN;
    }
    if (fdinfo[fd].we_should_epoll_for_writes) {
	ev.events |= EPOLLOUT;
    }
    ev.data.fd = fd;
    int ret = epoll_ctl(kdpfd, EPOLL_CTL_MOD, fd, &ev);
    if(ret<0) {
	fprintf(stderr, "epoll_ctl MOD error errno=%d\n", errno);
    }
}
