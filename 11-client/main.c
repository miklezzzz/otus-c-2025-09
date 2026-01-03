#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>


#define PORT 23
#define PORT_STR "23"
#define BUF_SIZE 16000

const char* command = "figlet";
static char buffer[BUF_SIZE];
const char* str_to_find = "Press control-C to interrupt any command.";
const char str_to_find2[3] = "\n.";

int string_has_suffix(char* str, int offset, int length, const char* suffix) {
	int j = 0;
	for (int i = offset; i < offset + length; i++) {
		if ((unsigned char)str[i] == 255) {
			i += 2;
			continue;
		}

		if ((strlen(str)-1-i) < (strlen(suffix)-1-j)) {
			return 0;
		}

		if (str[i] == suffix[j]) {
			j += 1;
		} else {
			j = 0;
		}

		if ((j == (int)strlen(suffix)-1)) {
			return 1;
		}
	}

	return 0;
}

char* get_ip_addr_by_name(char* domain_name) {
	struct addrinfo hints = {0};
	struct addrinfo* res = NULL;
	struct addrinfo* p = NULL;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int status = getaddrinfo(domain_name, PORT_STR, &hints, &res);
	if (status !=0) {
		perror("getaddrinfo failed");
		return NULL;
	}

	char ipstr[INET_ADDRSTRLEN];
	void *addr = NULL;
	for(p = res; p != NULL; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);

			if (addr != NULL) {
   		     		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
				if (ipstr == NULL) {
					perror("failed to convert ip to string");
					return NULL;
				}

				char* ip_str = strdup(ipstr);
				freeaddrinfo(res);

				return ip_str;
			}
		}
	}

	printf("failed to find a valid IPv4 address\n");
	return NULL;
}

int main(int argc, char** argv) {
	struct sockaddr_in sock_addr = {0};
	int sock_fd, r, exit_code = EXIT_SUCCESS;
	if (argc != 4) {
		printf("Usage: %s <host> <font> <message>\n", argv[0]);
		return EXIT_FAILURE;
	}

	sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_fd == -1) {
		perror("failed to open a socket");
		return EXIT_FAILURE;
	}

	struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
	if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
		perror("failed to set socket send timeout option");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(PORT);

	char* dst_ip = get_ip_addr_by_name(argv[1]);
	if (dst_ip == NULL) {
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	r = inet_pton(PF_INET, dst_ip, &sock_addr.sin_addr);
	if (r <= 0) {
		perror("failed to set dst address");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	printf("connecting to %s ..\n", dst_ip);
	free(dst_ip);

	if (connect(sock_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) == -1) {
		perror("failed to connect to host");
		exit_code = EXIT_FAILURE;
	}

	while ((r = recv(sock_fd, buffer, BUF_SIZE - 1, 0)) > 0) {
		if (string_has_suffix((char*)buffer, 0, r, str_to_find)) {
			break;
		}
	}

	if (r < 0) {
		perror("failed to receive a message from the host");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	char full_command[4096];
	int res = snprintf(full_command, sizeof(full_command), "%s /%s %s\n\r", command, argv[2], argv[3]);
	if (res < 0) {
		printf("failed to concatenate strings");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	if (send(sock_fd, full_command, strlen(full_command), 0) < 0) {
		perror("failed to send the command to the host");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	memset(buffer, 0, sizeof(buffer));

	int len = 0;
	while ((r = recv(sock_fd, &buffer[len], BUF_SIZE - len, 0)) > 0) {
		if (string_has_suffix((char*)buffer, len, r, str_to_find2)) {
			break;
		}
		len += r;
	}

	int found_new_line = 0;
	for (int i = 0; i < len + r; i++) {
		if (buffer[i] == '\n') {
			found_new_line = 1;
		}

		if (i == len + r - 1) {
			continue;
		}

		if (found_new_line) {
			putchar(buffer[i]);
			fflush(stdout);
		}
	}

	if (r < 0) {
		perror("failed to receive a message from the host");
		exit_code = EXIT_FAILURE;
		goto clean_up;
	}

	clean_up:
	close(sock_fd);
	return exit_code;
}
