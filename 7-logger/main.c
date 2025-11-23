#include "logger/logger.h"
#include "stdio.h"

void print_error(Logger* logger, char* msg) {
	LOG_ERR(logger, msg);
}

int main() {
	Logger* logger = create_logger("/tmp/logger_test123.log");
	if (logger == NULL) {
		return 1;
	}

	LOG_INFO(logger, "test message");
	LOG_DEBUG(logger, "debug message");
	print_error(logger, "error message");

	terminate_logger(logger);
	return 0;
}
