#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <yaml.h>

#define CONFIG_SOCKET_PATH "socketPath"
#define CONFIG_FILE_PATH "filePath"
#define CONFIG_PATH "./config.yaml"

void getFileSize(char** response_msg, char* file_path) {
	struct stat file_stats;
	if (stat(file_path, &file_stats) < 0) {
		sprintf(*response_msg, "failed to get file stats: %s\n", strerror(errno));
		return;
	}

	sprintf(*response_msg, "the size of the %s file is %ld bytes\n", file_path, file_stats.st_size);
}

struct Config {
	char* socket_path;
	char* file_path;
};

int listenToUnixSocket(char* socket_path, char* file_path) {
	if (unlink(socket_path) < 0) {
		if (errno != ENOENT) {
			perror("failed to unlink the socket");
			return 1;
		}
	}

	int unix_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (unix_sock == -1) {
		perror("failed to creat a unix socket");
		return 1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, socket_path);
	if (bind(unix_sock, (struct sockaddr*) &addr, sizeof(struct sockaddr_un))) {
		perror("failed to bind the socket");
		return 1;
	}

	if (listen(unix_sock, 20) == 1) {
		perror("failed to adjust the socket's backlog");
		return 1;
	}

	while (1) {
		int client_fd = accept(unix_sock, 0, 0);
        	if (client_fd == -1) {
			perror("accept");
		}

		char* response_msg;
		response_msg = (char *)malloc(100 * sizeof(char));
		getFileSize(&response_msg, file_path);

		if (write(client_fd, response_msg, strlen(response_msg)) == -1) {
     			perror("failed to write the response");
		}

		free(response_msg);
		close(client_fd);
	}

	close(unix_sock);
	return 0;
}

typedef enum {
	EXPECT_KEY,
	EXPECT_UNKNOWN_VALUE,
	EXPECT_SOCKET_PATH_VALUE,
	EXPECT_FILE_PATH_VALUE
} parse_state_t;

struct Config* parseConfig() {
	yaml_parser_t parser;
	yaml_event_t event;
	parse_state_t state = EXPECT_KEY;
	FILE *config_file = fopen(CONFIG_PATH, "rb");
	if (!config_file) {
		perror("failed to open the config");
		return NULL;
	}

	if (!yaml_parser_initialize(&parser)) {
		perror("failed to init the yaml parse");
		return NULL;
	}

	struct Config* config = malloc(sizeof(struct Config));

	yaml_parser_set_input_file(&parser, config_file);
	do {
		if (!yaml_parser_parse(&parser, &event)) {
			perror("failed to parse");
			return NULL;
			break;
		}

		switch (event.type) {
			case YAML_MAPPING_START_EVENT:
				state = EXPECT_KEY;
				break;

			case YAML_SCALAR_EVENT:
				switch (state) {
					case EXPECT_KEY: ;
						char* key = (char*) event.data.scalar.value;
						if (strcmp(key, CONFIG_FILE_PATH) == 0) {
							state = EXPECT_FILE_PATH_VALUE;
						} else if (strcmp(key, CONFIG_SOCKET_PATH) == 0) {
							state = EXPECT_SOCKET_PATH_VALUE;
						} else {
							state = EXPECT_UNKNOWN_VALUE;
						}
						break;

					case EXPECT_UNKNOWN_VALUE:
						state = EXPECT_KEY;
						break;

					case EXPECT_SOCKET_PATH_VALUE:
						config->socket_path = malloc((strlen((char*)event.data.scalar.value)+1)*sizeof(char));
						strcpy(config->socket_path, (char*)event.data.scalar.value);
						state = EXPECT_KEY;
						break;

					case EXPECT_FILE_PATH_VALUE:
						config->file_path = malloc((strlen((char*)event.data.scalar.value)+1)*sizeof(char));
						strcpy(config->file_path, (char*)event.data.scalar.value);
						state = EXPECT_KEY;
						break;
				}
				break;
			default:
				break;
		}

		if(event.type != YAML_STREAM_END_EVENT) {
      			yaml_event_delete(&event);
		}
	} while (event.type != YAML_STREAM_END_EVENT);
	yaml_event_delete(&event);

	yaml_parser_delete(&parser);

	fclose(config_file);
	if (config->socket_path == NULL) {
		printf("failed to get the socket path from the config file\n");
		return NULL;
	}

	if (config->file_path == NULL) {
		printf("failed to get the file path from the config file\n");
		return NULL;
	}

	return config;
}

int startServer(pid_t pid) {
	struct Config* config = parseConfig();
	if (config == NULL) {
		return -1;
	}

	if (pid == 0) {
		printf("listen to the %s socket from the daemon\n", config->socket_path);
	}

	char socket[100];
	char file[100];
	strcpy(socket, config->socket_path);
	strcpy(file, config->file_path);
	free(config->socket_path);
	free(config->file_path);
	free(config);
	int res = listenToUnixSocket(socket, file);

	return res;
}

int main(int argc, char *argv[]) {
	pid_t pid = -1;
	switch (argc) {
		case 2:
			if (strcmp(argv[1], "-d") == 0) {
				pid = fork();
				if (pid < 0) {
					perror("failed to fork");
					return 1;
				}

				if (pid == 0) {
					return startServer(pid);
				}

				if (pid > 0) {
					printf("successufully spawned the daemon process with the %d pid\n", pid);
					return 0;
				}

				break;

			} else {
				printf("wrong argument, use \"-d\" do daemonize\n");
				return 1;
			}
	}

	return startServer(pid);
}
