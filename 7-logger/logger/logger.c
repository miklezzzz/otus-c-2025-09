#include <stdio.h>
#include "logger.h"
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>

struct Logger {
	pthread_mutex_t mutex;
	FILE* f_ptr;
};

int get_current_time(char** time_string) {
	time_t current_time;

	current_time = time(NULL);
	if (current_time == ((time_t)-1)) {
		return -1;
	}

	char* ct = ctime(&current_time);
	if (ct == NULL) {
		return -1;
	}

	if (strlen(ct) > 0 && ct[strlen(ct) -1 ] == '\n') {
		ct[strlen(ct) -1] = '\0';
	}

	*time_string = malloc(strlen(ct) + 1);
	strcpy(*time_string, ct);

	return 0;
}

int print_stack_trace(Logger* logger) {
	void* buffer[SIZE];
	int nptrs;
	char** strings;

	nptrs = backtrace(buffer, SIZE);

	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		perror("failed to convert current time to string\n");
		return -1;
	}

	int res = fprintf(logger->f_ptr, "%s", "stack trace:");
	if (res < 0) {
		perror("failed to write to the file\n");
		goto clean_up;
	}

	for (int j = 2; j < nptrs; j++) {
		res = fprintf(logger->f_ptr, " %s\n", strings[j]);
		if (res < 0) {
			perror("failed to write to the file\n");
			break;
		}
	}

	clean_up:
	free(strings);
	return res;
}

int log_debug(Logger* self, char* str, char* file, int line) {
	char* ts;
	pthread_mutex_lock(&(self->mutex));

	int res = get_current_time(&ts);
	if (res < 0) {
		perror("failed to get current time");
		goto clean_up;
	}

	res = fprintf(self->f_ptr, "{\"ts\": \"%s\", \"level\": \"debug\", \"pos\": \"%s:%d\", \"msg\": \"%s\"}\n", ts, file, line, str);
	if (res < 0) {
		perror("failed to write to the file");
	}

	clean_up:
	free(ts);
	pthread_mutex_unlock(&(self->mutex));

	return res;
}

int log_info(Logger* self, char* str, char* file, int line) {
	char* ts;
	pthread_mutex_lock(&(self->mutex));

	int res = get_current_time(&ts);
	if (res < 0) {
		perror("failed to get current time");
		goto clean_up;
	}

	res = fprintf(self->f_ptr, "{\"ts\": \"%s\", \"level\": \"info\", \"pos\": \"%s:%d\", \"msg\": \"%s\"}\n", ts, file, line, str);
	if (res < 0) {
		perror("failed to write to the file");
	}

	clean_up:
	free(ts);
	pthread_mutex_unlock(&(self->mutex));

	return res;
}

int log_warn(Logger* self, char* str, char* file, int line) {
	char* ts;
	pthread_mutex_lock(&(self->mutex));

	int res = get_current_time(&ts);
	if (res < 0) {
		perror("failed to get current time");
		goto clean_up;
	}

	res = fprintf(self->f_ptr, "{\"ts\": \"%s\", \"level\": \"warning\", \"pos\": \"%s:%d\", \"msg\": \"%s\"}\n", ts, file, line, str);
       	if (res < 0) {
		perror("failed to get current time");
	}

	clean_up:
	free(ts);
	pthread_mutex_unlock(&(self->mutex));

	return res;
}

int log_err(Logger* self, char* str, char* file, int line) {
	char* ts;
	pthread_mutex_lock(&(self->mutex));

	int res = get_current_time(&ts);
	if (res < 0) {
		perror("failed to get current time");
		goto clean_up;
	}

	res = fprintf(self->f_ptr, "{\"ts\": \"%s\", \"level\": \"error\", \"pos\": \"%s:%d\", \"msg\": \"%s\"}\n", ts, file, line, str);
	if (res < 0) {
		perror("failed to write to the file");
		goto clean_up;
	}

	if (print_stack_trace(self) < 0) {
		perror("failed to obtain the stack trace");
	}

	clean_up:
	free(ts);
	pthread_mutex_unlock(&(self->mutex));

	return res;
}

Logger* create_logger(char* path) {
	static Logger logger;
	if (pthread_mutex_init(&(logger.mutex), NULL) != 0) {
		perror("failed to init logger mutex");
		return NULL;
	}

	char* dst_path = DEFAULT_PATH;
	if (strlen(path) > 0) {
		dst_path = path;
	}

	logger.f_ptr = fopen(dst_path, "a");
	if (logger.f_ptr == NULL) {
		perror("failed to open the file for writing");
		return NULL;
	}

	return &logger;
}

void terminate_logger(Logger* logger) {
	pthread_mutex_destroy(&(logger->mutex));
	fclose(logger->f_ptr);
}
