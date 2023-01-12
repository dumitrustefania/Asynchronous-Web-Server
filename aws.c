#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include "aws.h"
#include "utils/debug.h"
#include "http-parser/http_parser.h"
#include "utils/sock_util.h"
#include "utils/util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

/* storage for request_path */
static char request_path[BUFSIZ];

enum connection_state
{
	STATE_DATA_RECEIVED,
	STATE_DATA_SENT,
	STATE_CONNECTION_CLOSED
};

enum file_type
{
	STATIC,
	NONE
};

/* structure acting as a connection handler */
struct connection
{
	int sockfd;
	int fd;
	size_t size;

	/* buffers used for receiving messages and then echoing them back */
	char recv_buffer[BUFSIZ];
	size_t recv_len;
	char send_buffer[BUFSIZ];
	size_t send_len;

	enum connection_state state;
	enum file_type type;
};

/*
 * Callback is invoked by HTTP request parser when parsing request path.
 * Request path is stored in global request_path variable.
 */
static int on_path_cb(http_parser *p, const char *buf, size_t len)
{
	char path[BUFSIZ];
	strncpy(path, buf, len);
	sprintf(request_path, "%s%s", ".", path);
	return 0;
}

/* Use mostly null settings except for on_path callback. */
static http_parser_settings settings_on_path = {
	/*on_message_begin*/ 0,
	/*on_path*/ on_path_cb,
	/*on_query_string*/ 0,
	/*on url*/ 0,
	/*on fragment*/ 0,
	/*on_header_field*/ 0,
	/*on_header_value*/ 0,
	/*on_headers_complete*/ 0,
	/*on_body*/ 0,
	/*on_message_complete*/ 0};

/*
 * Initialize connection structure on given socket.
 */

static struct connection *connection_create(int sockfd)
{
	struct connection *conn = malloc(sizeof(*conn));

	DIE(conn == NULL, "malloc");

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);

	return conn;
}

/*
 * Remove connection handler.
 */
static enum connection_state connection_remove(struct connection *conn)
{
	int rc = w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "w_epoll_remove_ptr");

	/* remove current connection */
	close(conn->sockfd);
	conn->state = STATE_CONNECTION_CLOSED;
	free(conn);

	return STATE_CONNECTION_CLOSED;
}

/*
 * Handle a new connection request on the server socket.
 */
static void handle_new_connection(void)
{
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* accept new connection */
	sockfd = accept(listenfd, (SSA *)&addr, &addrlen);
	DIE(sockfd < 0, "accept");

	dlog(LOG_ERR, "Accepted connection from: %s:%d\n",
		 inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	/* make the socket non-blocking by setting the
	corresponding flag (O_NONBLOCK) */
	int curr_flag = fcntl(sockfd, F_GETFL);
	int non_block_flag = curr_flag | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, non_block_flag);

	/* instantiate new connection handler */
	conn = connection_create(sockfd);

	/* add socket to epoll */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);
	DIE(rc < 0, "w_epoll_add_in");
}

/*
 * Receive message on socket.
 * Store message in recv_buffer in struct connection.
 */
static enum connection_state receive_message(struct connection *conn)
{
	ssize_t bytes_recv;
	int rc;
	char abuffer[64];

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0)
	{
		ERR("get_peer_address");
		return connection_remove(conn);
	}

	bytes_recv = recv(conn->sockfd, conn->recv_buffer + conn->recv_len,
					  BUFSIZ, 0);

	if (bytes_recv < 0)
	{ /* error in communication */
		dlog(LOG_ERR, "Error in communication from: %s\n", abuffer);
		return connection_remove(conn);
	}
	if (bytes_recv == 0)
	{ /* connection closed */
		dlog(LOG_INFO, "Connection closed from: %s\n", abuffer);
		return connection_remove(conn);
	}

	dlog(LOG_DEBUG, "Received message from: %s\n", abuffer);

	printf("--\n%s--\n", conn->recv_buffer);

	conn->recv_len += bytes_recv;
	conn->state = STATE_DATA_RECEIVED;

	return STATE_DATA_RECEIVED;
}

static void create_error_output(struct connection *conn)
{
	conn->type = NONE;
	strncat(conn->send_buffer, ERROR_MSG, BUFSIZ);
	conn->send_len = sizeof(ERROR_MSG);
	conn->fd = -1;
}

int file_size(int fd)
{
	struct stat buf;
	fstat(fd, &buf);
	return buf.st_size;
}

static void create_ok_output(struct connection *conn)
{
	strcpy(conn->send_buffer, OK_MSG);

	/*add file size to OK message*/
	char buffer[20];
	sprintf(buffer, "%d", conn->size);
	strcat(conn->send_buffer, buffer);
	strcat(conn->send_buffer, "\r\n\r\n");
	fprintf(stderr, "%s", conn->send_buffer);

	conn->send_len = strlen(conn->send_buffer);
}
/*
 * Call http_parser to parse the request. Determine request_path
 * as filled by callback.
 * Callback is on_path_cb as setup in settings_on_path.
 */
void parse_http_request(struct connection *conn)
{
	/* init HTTP_REQUEST parser */
	http_parser h_parser;
	http_parser_init(&h_parser, HTTP_REQUEST);

	size_t bytes_parsed = http_parser_execute(
		&h_parser, &settings_on_path, conn->recv_buffer, conn->recv_len);
	printf("Parsed simple HTTP request (bytes: %lu), path: %s\n", bytes_parsed, request_path);

	/* determine whether the request path refers to a
	static file*/
	if (strstr(request_path, AWS_REL_STATIC_FOLDER))
		conn->type = STATIC;
}
/*
 * Handle a client request on a client connection.
 */
static void handle_client_request(struct connection *conn)
{
	int rc;
	enum connection_state ret_state;

	ret_state = receive_message(conn);
	if (ret_state == STATE_CONNECTION_CLOSED)
		return;

	if (strstr(conn->recv_buffer, "\r\n\r\n") == NULL)
		return;

	/* add socket to epoll for out events */
	rc = w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "w_epoll_update_ptr_out");

	parse_http_request(conn);

	conn->fd = open(request_path, O_RDONLY);
	if (conn->fd == -1)
	{
		create_error_output(conn);
		return;
	}

	conn->size = file_size(conn->fd);
	create_ok_output(conn);
}

/*
 * Send message on socket.
 * Store message in send_buffer in struct connection.
 */
static enum connection_state send_message(struct connection *conn)
{
	ssize_t bytes_sent;
	int rc;
	char abuffer[64];

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0)
	{
		ERR("get_peer_address");
		return connection_remove(conn);
	}

	bytes_sent = send(conn->sockfd, conn->send_buffer, conn->send_len, 0);
	if (bytes_sent < 0)
	{ /* error in communication */
		dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
		return connection_remove(conn);
	}
	if (bytes_sent == 0)
	{ /* connection closed */
		dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
		return connection_remove(conn);
	}

	strcpy(conn->send_buffer, conn->send_buffer + bytes_sent);
	conn->send_len -= bytes_sent;

	if (conn->send_len != 0)
		return -1;

	dlog(LOG_DEBUG, "Sending message to %s\n", abuffer);

	printf("--\n%s--\n", conn->send_buffer);

	conn->state = STATE_DATA_SENT;

	return STATE_DATA_SENT;
}

/*Use sendfile because it does not pass the data to user-space,
making the copying in kernel-space.*/
static void send_file(struct connection *conn)
{
	ssize_t bytes_sent;
	char abuffer[64];
	int rc;

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0)
	{
		ERR("get_peer_address");
		connection_remove(conn);
		return;
	}

	bytes_sent = sendfile(conn->sockfd, conn->fd, NULL,
						  conn->size);
	if (bytes_sent < 0)
	{ /* error in communication */
		dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
		connection_remove(conn);
		return;
	}
	if (bytes_sent == 0)
	{ /* connection closed */
		dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
		connection_remove(conn);
		return;
	}

	dlog(LOG_INFO, "Sent message to %s (bytes: %ld)\n", abuffer,
		 bytes_sent);
}

/*
 * Handle a client reply.
 */
static void handle_client_reply(struct connection *conn)
{
	if (conn->state == STATE_DATA_RECEIVED)
	{
		enum connection_state ret_state = send_message(conn);
		if (ret_state != STATE_DATA_SENT)
			return;
	}

	if (conn->type == STATIC)
		send_file(conn);
	else if (conn->type == NONE)
	{
		int rc = w_epoll_update_ptr_in(epollfd, conn->sockfd, conn);
		DIE(rc < 0, "w_epoll_update_ptr_in");
		return;
	}
}

int main(void)
{
	int rc;

	/* init multiplexing */
	epollfd = w_epoll_create();
	DIE(epollfd < 0, "w_epoll_create");

	/* create server socket */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT, DEFAULT_LISTEN_BACKLOG);
	DIE(listenfd < 0, "tcp_create_listener");

	rc = w_epoll_add_fd_in(epollfd, listenfd);
	DIE(rc < 0, "w_epoll_add_fd_in");

	dlog(LOG_INFO, "Server waiting for connections on port %d\n",
		 AWS_LISTEN_PORT);

	/* server main loop */
	while (1)
	{
		struct epoll_event rev;

		/* wait for events */
		rc = w_epoll_wait_infinite(epollfd, &rev);
		DIE(rc < 0, "w_epoll_wait_infinite");

		/*
		 * switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */

		if (rev.data.fd == listenfd)
		{
			dlog(LOG_DEBUG, "New connection\n");
			if (rev.events & EPOLLIN)
				handle_new_connection();
		}
		else
		{
			if (rev.events & EPOLLIN)
			{
				dlog(LOG_DEBUG, "New message\n");
				handle_client_request(rev.data.ptr);
			}
			else if (rev.events & EPOLLOUT)
			{
				dlog(LOG_DEBUG, "Ready to send message\n");
				handle_client_reply(rev.data.ptr);
			}
		}
	}

	return 0;
}
