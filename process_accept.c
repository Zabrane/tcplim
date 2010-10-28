
static void process_accept(int ss) {
    /* Accepting the client socket */
    struct sockaddr_in sa, da; 
    struct epoll_event ev;
    memset(&ev, 0, sizeof ev);
    size_t len = sizeof sa;
    int client = accept(ss, (struct sockaddr *) &sa, &len);
    if (client < 0) {
	perror("accept");
	return;
    }
    fcntl(client, F_SETFL, O_NONBLOCK);

    memset(&da, 0, sizeof da);
#ifndef SO_ORIGINAL_DST
#define SO_ORIGINAL_DST 80
#endif
    if (need_address_redirection || need_port_redirection) {
	len = sizeof da;
	if (getsockopt
	    (client, SOL_IP, SO_ORIGINAL_DST,
	     (struct sockaddr *) &da, &len) != 0) {
	    write(client, "Unable to get destination address\n", 34);
	    close(client);
	    printf("%s:%d -> ???\n", inet_ntoa(sa.sin_addr),
		   ntohs(sa.sin_port));
	    return;
	}
    }

    da.sin_family = PF_INET;
    if (!need_port_redirection) {
	da.sin_port = htons(connect_port);
    }
    if (!need_address_redirection) {
	inet_aton(connect_ip, &da.sin_addr);
    }


    /* Now start connecting to the peer */

    int destsocket = socket(PF_INET, SOCK_STREAM, 0);
    if (destsocket == -1) {
	const char* msg = "Cannot create a socket to destination address.\n";

	if(need_port_redirection || need_address_redirection) {
	    msg = "Cannot create a socket to destination address.\n"
		"If you are using REDIRECT feature, you should not connect here directly.\n"
		"Also check you iptables rule. It should not redirect tcplim's connections back to tcplim. Normal example:\n"
		"\"iptables -t nat -A OUTPUT -p tcp -m owner ! --uid-owner tcplim_user -j REDIRECT --to tcplim_port\"\n";
	}

	write(client, msg, strlen(msg));
	close(client);
	fprintf(stderr, "%s", msg);
	return;
    }
    fcntl(destsocket, F_SETFL, O_NONBLOCK);
    if (-1 == connect(destsocket, (struct sockaddr *) &da, sizeof da)) {
	if (errno != EWOULDBLOCK && errno != EINPROGRESS) {
	    fprintf(stderr, "Cannot connect a socket to destination address\n");
	    write(client, "Cannot connect a socket to destination address\n", 58-11);
	    close(client);
	    return;
	}
    }

    /* Set up events and pipes and fdinfo*/
    
    ev.events = EPOLLOUT  | EPOLLONESHOT;
    ev.data.fd = destsocket;
    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, destsocket, &ev) < 0) {
	fprintf(stderr, "epoll peer set insertion error\n");
	write(client, "epoll peer set insertion error\n", 31);
	close(client);
	close(destsocket);
	return;
    }
		
    ev.events = EPOLLOUT | EPOLLONESHOT;
    ev.data.fd = client;
    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, client, &ev) < 0) {
	write(client, "epoll set insertion error\n", 26);
	close(client);
	close(destsocket);
	return;
    }
    
    fdinfo[client].peerfd = destsocket;
    fdinfo[client].status='|';
    fdinfo[client].address=sa;
    fdinfo[client].writeready=0;
    fdinfo[client].readready=0;
    fdinfo[client].buff=NULL;
    fdinfo[client].debt=0;
    fdinfo[client].we_should_epoll_for_writes=1;  /* we've already set up epoll above */
    fdinfo[client].we_should_epoll_for_reads=0; /* first we get writing ready, only then start reading peer */
    fdinfo[client].group='c';
    fdinfo[client].total_read = 0;
    fdinfo[client].current_quota = 1024; /* Let's allow to send it request without extra delays */
    fdinfo[client].speed_limit = fd_upload_limit;
    fdinfo[client].nice=10;
    memset(&fdinfo[client].last_quota_bump_time, 0, sizeof(struct timeval));
    fdinfo[client].rate=0;
    fdinfo[client].total_read_last=0;
    fdinfo[destsocket].peerfd = client;
    fdinfo[destsocket].status='|';
    fdinfo[destsocket].address=da;
    fdinfo[destsocket].writeready=0;
    fdinfo[destsocket].readready=0;
    fdinfo[destsocket].buff=NULL;
    fdinfo[destsocket].debt=0;
    fdinfo[destsocket].we_should_epoll_for_writes=1; /* we've already set up epoll above */
    fdinfo[destsocket].we_should_epoll_for_reads=0;  /* first we get writing ready, only then start reading peer */ 
    fdinfo[destsocket].group='d';
    fdinfo[destsocket].total_read = 0;
    fdinfo[destsocket].current_quota = 4096; /* bump_quotas will cut it down if speed limit is less than 2048 */
    fdinfo[destsocket].speed_limit = fd_download_limit;
    fdinfo[destsocket].nice=10;
    memset(&fdinfo[destsocket].last_quota_bump_time, 0, sizeof(struct timeval));
    fdinfo[destsocket].rate=0;
    fdinfo[destsocket].total_read_last=0;
    
    printf("%s:%d -> ", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
    printf("%s:%d [%d->%d]\n",  inet_ntoa(da.sin_addr), ntohs(da.sin_port), client, destsocket); 

}
