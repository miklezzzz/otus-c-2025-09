#include <pthread.h>

#define LOG_DEBUG(logger, message) log_debug(logger, message, __FILE__, __LINE__)
#define LOG_INFO(logger, message) log_info(logger, message, __FILE__, __LINE__)
#define LOG_WARN(logger, message) log_warn(logger, message, __FILE__, __LINE__)
#define LOG_ERR(logger, message) log_err(logger, message, __FILE__, __LINE__)

#define SIZE 100
#define DEFAULT_PATH "/tmp/logger.log"

typedef struct Logger Logger;

int log_info(Logger* self, char* str, char* file, int line);
int log_warn(Logger* self, char* str, char* file, int line);
int log_debug(Logger* self, char* str, char* file, int line);
int log_err(Logger* self, char* str, char* file, int line);

Logger* create_logger(char* path);
void terminate_logger(Logger* logger);
