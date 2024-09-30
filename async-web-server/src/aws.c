// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

static void connection_prepare_send_reply_header(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the reply header. */
	char s[BUFSIZ];

	sprintf(s, "HTTP/1.1 200 OK\r\n"
			 "Connection: close\r\n"
			 "Content-Length: %ld\r\n"
			 "\r\n", conn->file_size);

	strcpy(conn->send_buffer, s);
	conn->send_len = strlen(s);
	conn->state = STATE_SENDING_HEADER;

	dlog(LOG_INFO, "This is the reply: %s", conn->send_buffer);
}

static void connection_prepare_send_404(struct connection *conn)
{
	/* TODO: Prepare the connection buffer to send the 404 header. */
	char s[BUFSIZ] = "HTTP/1.1 404 Not Found\r\n"
			 "Connection: close\r\n"
			 "Content-Length: 0\r\n"
			 "\r\n";

	strcpy(conn->send_buffer, s);
	conn->send_len = strlen(s);
	conn->state = STATE_SENDING_404;
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	/* TODO: Get resource type depending on request path/filename. Filename should
	 * point to the static or dynamic folder.
	 */
	char s[10];
	char *aux = malloc(strlen(conn->request_path) + 10);

	strcpy(aux, AWS_DOCUMENT_ROOT);
	strcat(aux, conn->request_path + 1);
	strcpy(conn->request_path, aux);
	free(aux);

	strncpy(s, conn->request_path + strlen(AWS_DOCUMENT_ROOT), 9);
	s[9] = '\0';
	dlog(LOG_INFO, "This is the type: %s\n", s);

	if (strstr(s, "static")) {
		dlog(LOG_INFO, "static\n");
		strcpy(conn->filename, conn->request_path + 8);
		return RESOURCE_TYPE_STATIC;
	}

	if (strstr(s, "dynamic")) {
		dlog(LOG_INFO, "dynamic\n");
		strcpy(conn->filename, conn->request_path + 9);
		return RESOURCE_TYPE_DYNAMIC;
	}

	return RESOURCE_TYPE_NONE;
}


struct connection *connection_create(int sockfd)
{
	/* TODO: Initialize connection structure on given socket. */
	int rc = 0;
	struct connection *conn = malloc(sizeof(struct connection));

	DIE(conn == NULL, "malloc");

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	memset(conn->request_path, 0, BUFSIZ);

	conn->recv_len = 0;
	conn->send_len = 0;
	conn->fd = -1;
	conn->file_size = 0;
	conn->state = STATE_INITIAL;
	conn->async_read_len = 0;

	dlog(LOG_INFO, "Wow have created new socket and rc is: %d\n", rc);

	return conn;
}

void connection_start_async_io(struct connection *conn)
{
	/* TODO: Start asynchronous operation (read from file).
	 * Use io_submit(2) & friends for reading data asynchronously.
	 */
	int rc;

	memset(&(conn->iocb), 0, sizeof(conn->iocb));
	memset(conn->send_buffer, 0, BUFSIZ);
	dlog(LOG_INFO, "This is what fd is: %d\n", conn->fd);
	io_prep_pread(&(conn->iocb), conn->fd, conn->send_buffer, BUFSIZ, conn->async_read_len);
	conn->piocb[0] = &(conn->iocb);
	rc = io_submit(conn->ctx, 1, conn->piocb);
	dlog(LOG_INFO, "This is what rc is for read: %d\n", rc);
}

void connection_remove(struct connection *conn)
{
	/* TODO: Remove connection handler. */
	close(conn->sockfd);
	conn->state = STATE_CONNECTION_CLOSED;
	free(conn);
	dlog(LOG_INFO, "I GOT RID OF CONNECTION\n");
}

void handle_new_connection(void)
{
	/* TODO: Handle a new connection request on the server socket. */
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* TODO: Accept new connection. */
	sockfd = accept(listenfd, (SSA *) &addr, &addrlen);
	DIE(sockfd < 0, "accept");

	dlog(LOG_ERR, "Accepted connection from: %s:%d\n",
		inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	/* TODO: Set socket to be non-blocking. */
	int fileflags = fcntl(sockfd, F_GETFL);

	fcntl(sockfd, F_SETFL, fileflags | FNDELAY | FASYNC);


	/* TODO: Instantiate new connection handler. */
	conn = connection_create(sockfd);

	/* TODO: Add socket to epoll. */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);
	DIE(rc < 0, "w_epoll_add_in");

	/* TODO: Initialize HTTP_REQUEST parser. */
	http_parser_init(&(conn->request_parser), HTTP_REQUEST);
}

void receive_data(struct connection *conn)
{
	/* TODO: Receive message on socket.
	 * Store message in recv_buffer in struct connection.
	 */
	ssize_t bytes_recv;

	dlog(LOG_INFO, "Wow we are waiting to read something\n");

	conn->state = STATE_RECEIVING_DATA;

	bytes_recv = recv(conn->sockfd, conn->recv_buffer + conn->recv_len, BUFSIZ - conn->recv_len, 0);

	if (bytes_recv <= 0) {
		conn->state = STATE_CONNECTION_CLOSED;
		w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
		connection_remove(conn);
		return;
	}

	conn->recv_len += bytes_recv;
	dlog(LOG_INFO, "Wow we have read something: %s\n", conn->recv_buffer);

	if (!strcmp(conn->recv_buffer + conn->recv_len - 4, "\r\n\r\n")) {
		conn->request_parser.data = conn;
		conn->state = STATE_REQUEST_RECEIVED;
		parse_header(conn);
		conn->res_type = connection_get_resource_type(conn);
		connection_open_file(conn);
		w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
	}

	if (conn->state == STATE_REQUEST_RECEIVED) {
		if (conn->res_type == RESOURCE_TYPE_NONE)
			connection_prepare_send_404(conn);
		else
			connection_prepare_send_reply_header(conn);
	}
}

int connection_open_file(struct connection *conn)
{
	/* TODO: Open file and update connection fields. */
	conn->fd = open(conn->request_path, O_RDWR);

	dlog(LOG_INFO, "We have opened the file: %d\n", conn->fd);

	if (conn->fd == -1) {
		conn->res_type = RESOURCE_TYPE_NONE;
		conn->file_size = 0;
	}

	struct stat buffer;

	fstat(conn->fd, &buffer);
	conn->file_size = buffer.st_size;

	return -1;
}

void connection_complete_async_io(struct connection *conn)
{
	/* TODO: Complete asynchronous operation; operation returns successfully.
	 * Prepare socket for sending.
	 */
	int rc;

	memset(&(conn->iocb), 0, sizeof(conn->iocb));
	io_prep_pwrite(&(conn->iocb), conn->sockfd, conn->send_buffer, BUFSIZ, 0);
	conn->piocb[0] = &(conn->iocb);
	rc = io_submit(conn->ctx, 1, conn->piocb);
	dlog(LOG_INFO, "This is what rc is for write: %d\n", rc);
}

int parse_header(struct connection *conn)
{
	/* TODO: Parse the HTTP header and extract the file path. */
	/* Use mostly null settings except for on_path callback. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};

	dlog(LOG_INFO, "I am in parse header: %ld\n", strlen(conn->recv_buffer));
	http_parser_execute(&(conn->request_parser), &settings_on_path, conn->recv_buffer, strlen(conn->recv_buffer));
	dlog(LOG_INFO, "This is the file path: %s\n", conn->request_path);
	return 0;
}

enum connection_state connection_send_static(struct connection *conn)
{
	/* TODO: Send static data using sendfile(2). */
	//dlog(LOG_INFO, "This is the size of the file: %ld, and this is BUFSIZ: %ld\n", conn->file_size, BUFSIZ);
	ssize_t bytes_sent;

	bytes_sent = sendfile(conn->sockfd, conn->fd, NULL, BUFSIZ);
	conn->send_len += bytes_sent;

	if (conn->send_len >= conn->file_size) {
		close(conn->fd);
		conn->state = STATE_DATA_SENT;
		return STATE_DATA_SENT;
	}
	return STATE_SENDING_DATA;
}

int connection_send_data(struct connection *conn)
{
	/* May be used as a helper function. */
	/* TODO: Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */
	ssize_t bytes_sent;

	dlog(LOG_INFO, "Prepearing to send\n");

	bytes_sent = send(conn->sockfd, conn->send_buffer, conn->send_len, 0);
	if (bytes_sent == -1)
		return -1;

	if (bytes_sent >= conn->send_len) {
		dlog(LOG_INFO, "Data has been sent\n");
		conn->state = STATE_DATA_SENT;

		if (conn->res_type == RESOURCE_TYPE_NONE) {
			conn->state = STATE_DATA_SENT;
			return bytes_sent;
		}
		memset(&(conn->iocb), 0, sizeof(conn->iocb));
		conn->state = STATE_SENDING_DATA;
		if (conn->res_type != RESOURCE_TYPE_STATIC) {
			dlog(LOG_INFO, "Started reading from file\n");
			conn->async_read_len = 0;
			conn->send_len = 0;
			conn->eventfd = eventfd(0, 0);
			io_setup(128, &(conn->ctx));
			w_epoll_add_fd_inout(epollfd, conn->eventfd);
			io_set_eventfd(&(conn->iocb), conn->eventfd);
			connection_start_async_io(conn);
			w_epoll_update_ptr_inout(epollfd, conn->eventfd, conn);
		}
	} else {
		dlog(LOG_INFO, "Data sent is this: %ld\n", bytes_sent);
		conn->send_len -= bytes_sent;
		memmove(conn->send_buffer, conn->send_buffer + bytes_sent, conn->send_len);
		conn->send_buffer[conn->send_len] = '\0';
	}

	return bytes_sent;
}


int connection_send_dynamic(struct connection *conn)
{
	/* TODO: Read data asynchronously.
	 * Returns 0 on success and -1 on error.
	 */
	struct io_event events[1];
	ssize_t bytes_read;
	int rc;

	dlog(LOG_INFO, "Made the right decision %ld %ld\n", conn->async_read_len, conn->send_len);

	rc = io_getevents(conn->ctx, 1, 1, events, NULL);
	dlog(LOG_INFO, "This is what rc is: %d\n", rc);
	if (rc <= 0)
		return 0;
	bytes_read = events[0].res;

	if (conn->send_len == conn->async_read_len) {
		conn->async_read_len += bytes_read;
		dlog(LOG_INFO, "I have read this much from file: %ld\n", conn->async_read_len);
		connection_complete_async_io(conn);
	} else {
		conn->send_len += bytes_read;
		conn->async_read_len = conn->send_len;
		dlog(LOG_INFO, "I have sent this much from file: %ld\n", conn->async_read_len);
		if (conn->async_read_len >= conn->file_size) {
			conn->state = STATE_DATA_SENT;
			w_epoll_remove_ptr(epollfd, conn->eventfd, conn);
			io_destroy(conn->ctx);
			close(conn->eventfd);
		} else {
			connection_start_async_io(conn);
		}
	}

	dlog(LOG_INFO, "I WILL NOW EXIT CONNECTION_SEND_DYNAMIC WITH THE CODE: %d\n", conn->state);
	return 0;
}


void handle_input(struct connection *conn)
{
	/* TODO: Handle input information: may be a new message or notification of
	 * completion of an asynchronous I/O operation.
	 */
	dlog(LOG_INFO, "I AM IN HANDLE INPUT: %d\n", conn->state);

	switch (conn->state) {
	case STATE_INITIAL:
		receive_data(conn);
		break;
	case STATE_RECEIVING_DATA:
		receive_data(conn);
		break;
	case STATE_SENDING_DATA:
		connection_send_dynamic(conn);
		break;
	default:
		printf("shouldn't get here %d\n", conn->state);
	}
}

void handle_output(struct connection *conn)
{
	/* TODO: Handle output information: may be a new valid requests or notification of
	 * completion of an asynchronous I/O operation or invalid requests.
	 */
	dlog(LOG_INFO, "I AM IN HANDLE OUTPUT: %d\n", conn->state);

	switch (conn->state) {
	case STATE_SENDING_404:
		connection_send_data(conn);
		break;
	case STATE_SENDING_HEADER:
		connection_send_data(conn);
		break;
	case STATE_DATA_SENT:
		w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
		connection_remove(conn);
		break;
	case STATE_SENDING_DATA:
		if (conn->res_type == RESOURCE_TYPE_STATIC)
			connection_send_static(conn);
		else
			connection_send_dynamic(conn);
		break;
	default:
		ERR("Unexpected state\n");
		exit(1);
	}
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* TODO: Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
}

int main(void)
{
	int rc;

	/* TODO: Initialize asynchronous operations. */

	/* TODO: Initialize multiplexing. */
	epollfd = w_epoll_create();
	DIE(epollfd < 0, "w_epoll_create");

	/* TODO: Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT,
		DEFAULT_LISTEN_BACKLOG);
	DIE(listenfd < 0, "tcp_create_listener");

	/* TODO: Add server socket to epoll object*/
	rc = w_epoll_add_fd_in(epollfd, listenfd);
	DIE(rc < 0, "w_epoll_add_fd_in");

	/* Uncomment the following line for debugging. */
	dlog(LOG_INFO, "Server waiting for connections on port %d\n", AWS_LISTEN_PORT);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* TODO: Wait for events. */
		rc = w_epoll_wait_infinite(epollfd, &rev);
		DIE(rc < 0, "w_epoll_wait_infinite");

		/* TODO: Switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */
		if (rev.data.fd == listenfd) {
			if (rev.events & EPOLLIN)
				handle_new_connection();
		} else {
			if (rev.events & EPOLLIN)
				handle_input(rev.data.ptr);
			if (rev.events & EPOLLOUT)
				handle_output(rev.data.ptr);
		}
	}

	return 0;
}
