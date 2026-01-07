#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_EVENTS 1024

struct worker_data {
	int id;
	char* dir;
	char* ip_addr;
	unsigned int port;
};

int pipe_fds[2];
long cores = 0;

const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 16\r\nConnection: close\r\n\r\nFile not found\r\n";
const char* bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\nBad request\r\n";
const char* denied_request = "HTTP/1.1 403 Forbidden\r\nContent-Length: 15\r\nConnection: close\r\n\r\nAccess denied\r\n";

void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void* worker_thread(void* arg) {
	struct worker_data* data = (struct worker_data*)arg;
	int listen_fd, epoll_fd;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("failed to create a socket");
		return NULL;
	}

	int opt = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
		perror("failed to set SO_REUSEPORT socket option");
		return NULL;
	}

	struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(data->port)};
	int res = inet_pton(AF_INET, data->ip_addr, &(addr.sin_addr));
	if (res != 1) {
		perror("failed to convert ip addr\n");
		return NULL;
	}

	if (bind(listen_fd, (struct sockaddr* )&addr, sizeof(addr)) < 0) {
		perror("failed to bind to the socket");
		return NULL;
	}

	set_nonblocking(listen_fd);
	listen(listen_fd, SOMAXCONN);

	struct epoll_event ev, pipe_ev, events[MAX_EVENTS];
	epoll_fd = epoll_create1(0);
	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);


	pipe_ev.events = EPOLLIN|EPOLLONESHOT;
	pipe_ev.data.fd = pipe_fds[0];
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_fds[0], &pipe_ev);
	printf("worker %d is listening on %s:%d...\n", data->id, data->ip_addr, data->port);

	while (1) {
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		for (int i = 0; i < nfds; i++) {
			if (events[i].data.fd == pipe_fds[0]) {
				int i;
				read(pipe_fds[0], &i, 1);
				printf("worker %d got the shudown signal, shutting down...\n", data->id);
				goto exit;
			} else if (events[i].data.fd == listen_fd) {
				int client_fd = accept(listen_fd, NULL, NULL);
				if (client_fd >= 0) {
					set_nonblocking(client_fd);
					struct epoll_event client_ev = {.events = EPOLLIN | EPOLLET, .data.fd = client_fd};
					epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
				}
			} else {
				char buffer[2048];
				ssize_t bytes_read = recv(events[i].data.fd, buffer, sizeof(buffer) - 1, 0);
				if (bytes_read <= 0) {
					goto clean_up;
				}

    				buffer[bytes_read] = '\0';

				char method[10], uri[1024];
				int scan_res = sscanf(buffer, "%s %s", method, uri);
				if (scan_res != 2) {
					write(events[i].data.fd, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 13\r\nConnection: close\r\n\r\nCould not parse argyments", 25);
					goto clean_up;
				}

    				char* parsed_uri = (uri[0] == '/') ? &uri[1] : uri;
				if (strncmp(method, "GET", 3) != 0) {
					write(events[i].data.fd, denied_request, strlen(denied_request));
					goto clean_up;
				}

				if (strncmp(uri, "/files?", 7) != 0) {
					write(events[i].data.fd, not_found_response, strlen(not_found_response));
					goto clean_up;
    				}

				char filename[256] = {0};
				char* query_start = strchr(parsed_uri, '?');
				if (query_start) {
					char* name_key = strstr(query_start, "name=");
					if (name_key) {
						name_key += 5;
						sscanf(name_key, "%255[^&]", filename);

					}
				} else {
					write(events[i].data.fd, bad_request, strlen(bad_request));
					goto clean_up;
				}

				if (strlen(filename) == 0 || strstr(filename, "..") || strchr(filename, '/') || strchr(filename, '\\')) {
					write(events[i].data.fd, bad_request, strlen(bad_request));
					goto clean_up;
				}

				char full_path[PATH_MAX];
				sprintf(full_path, "%s/%s", data->dir, filename);

				int file_fd = open(full_path, O_RDONLY);
				if (file_fd >= 0) {
					struct stat st;
					fstat(file_fd, &st);
					off_t file_size = st.st_size;
					char headers[256];
					int header_len = sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", file_size);
					write(events[i].data.fd, headers, header_len);
					off_t offset = 0;
					sendfile(events[i].data.fd, file_fd, &offset, file_size);
					close(file_fd);
				} else {
					const char *error_response;
   					if (errno == ENOENT) {
						error_response = not_found_response;
					} else if (errno == EACCES) {
						error_response = denied_request;
					} else {
        					error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 21\r\nConnection: close\r\n\r\nInternal server error\r\n";
    					}
    					write(events[i].data.fd, error_response, strlen(error_response));
				}

				clean_up:
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
				close(events[i].data.fd);
			}
		}
	}

	exit:
	close(listen_fd);
	close(epoll_fd);

	return NULL;
}

void signal_handler(__attribute__((unused)) int i) {
	const char* msg = "\nSignal received, shutting down...\n";
	write(STDOUT_FILENO, msg, 35);
	for (int i = 0; i < cores; i++) {
		write(pipe_fds[1], &i, sizeof(int));
	}
}

int main(int argc, char** argv) {
	if (argc != 3) {
		printf("Usage: %s <host>:<port> <directory>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char ip_addr[63];
	unsigned int port;
	if (sscanf(argv[1], "%63[^:]:%u", ip_addr, &port) != 2) {
		printf("failed to parse <ip>:<port> argument\n");
		return EXIT_FAILURE;
	}

	if (port > 65535) {
		printf("max port number is 65535\n");
		return EXIT_FAILURE;
	}

	cores = sysconf(_SC_NPROCESSORS_ONLN);
		if (cores == -1) {
		perror("could not detect the number of cores");
		return EXIT_FAILURE;
	}

	pthread_t threads[(int)cores];

	struct worker_data data[8];

	if (pipe(pipe_fds) == -1) {
		perror("failed to init the pipe\n");
		return EXIT_FAILURE;
	}

	for (long i = 0; i < cores; i++) {
		data[i].id = i;
		data[i].ip_addr = ip_addr;
		data[i].port = (int)port;
		data[i].dir = argv[2];

		if (pthread_create(&threads[i], NULL, worker_thread, (void*)&data[i]) != 0) {
			perror("failed to create a thread");
			return EXIT_FAILURE;
		}
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &signal_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	for (int i = 0; i < cores; i++) {
		pthread_join(threads[i], NULL);
	}

	return EXIT_SUCCESS;
}
