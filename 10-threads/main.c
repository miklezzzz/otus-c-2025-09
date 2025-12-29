#define _DEFAULT_SOURCE
#define HASHMAP_IMPLEMENTATION

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include "hashmap.h"

struct file_to_scan {
	char* filename;
	struct file_to_scan* prev_element;
};

struct scan_report {
	char* dir;
	intmax_t total_served;

	pthread_mutex_t mutex;
	struct file_to_scan* files;
	struct hashmap_s* downloaded_per_url;
	struct hashmap_s* referer_count;
};

struct file_report {
	intmax_t bytes;
	struct hashmap_s* downloaded_per_url;
	struct hashmap_s* referer_count;
};

struct map_record {
	char* key;
	intmax_t value;
};

struct array_ctx {
	struct map_record* array;
	unsigned int index;
};

struct scan_report* init_scan_report(char* dir) {
	struct scan_report* report = (struct scan_report*)calloc(1, sizeof(struct scan_report));
	if (report == NULL) {
		printf("failed to allocate memory\n");
		return NULL;
	}

	struct file_to_scan* files_to_scan = NULL;
	report->dir = dir;
	report->files = files_to_scan;
	report->total_served = 0;

	if (pthread_mutex_init(&(report->mutex), NULL) != 0) {
		free(report);
		perror("failed to init mutex");
		return NULL;
	}

	report->downloaded_per_url = (struct hashmap_s*)malloc(sizeof(struct hashmap_s));
	if (report->downloaded_per_url == NULL) {
		printf("failed to allocate memory\n");
		return NULL;
	}

	if (hashmap_create(8192, report->downloaded_per_url) != 0) {
		free(report->downloaded_per_url);
		free(report);
		printf("failed to init a hashmap\n");
		return NULL;
	}

	report->referer_count = (struct hashmap_s*)malloc(sizeof(struct hashmap_s));
	if (report->referer_count == NULL) {
		printf("failed to allocate memory\n");
		hashmap_destroy(report->downloaded_per_url);
		free(report->downloaded_per_url);
		free(report);
		return NULL;
	}

	if (hashmap_create(8192, report->referer_count) != 0) {
		hashmap_destroy(report->downloaded_per_url);
		free(report->downloaded_per_url);
		free(report->referer_count);
		free(report);
		printf("failed to init a hashmap\n");
		return NULL;
	}

	return report;
}

int add_file_to_scan(struct scan_report* report, char* filename) {
	if (report == NULL) {
		return -1;
	}

	struct file_to_scan* new_element = calloc(1, sizeof(struct file_to_scan));
	if (new_element == NULL) {
		return -1;
	} else {
		new_element->filename = filename;
	}

	pthread_mutex_lock(&(report->mutex));
	if (report->files == NULL) {
		report->files = new_element;
	} else {
		new_element->prev_element = report->files;
		report->files = new_element;
	}

	pthread_mutex_unlock(&(report->mutex));

	return 0;
}

char* get_next_file_to_scan(struct scan_report* report) {
	if (report == NULL) {
		return NULL;
	}

	pthread_mutex_lock(&(report->mutex));

	if (report->files == NULL) {
		pthread_mutex_unlock(&(report->mutex));
		return NULL;
	}

	char* filename = malloc(sizeof(char) * strlen(report->files->filename) + 1);
	if (filename == NULL) {
		return NULL;
	}
	snprintf(filename, strlen(report->files->filename) + 1, "%s", report->files->filename);

	struct file_to_scan* current = report->files;
	report->files = report->files->prev_element;
	free(current->filename);
	free(current);
	pthread_mutex_unlock(&(report->mutex));

	return filename;
}

int add_files_to_scan(struct scan_report* report) {
	if (report == NULL) {
		return -1;
	}

	DIR* dir;
	struct dirent* entry;

	int exit_code = 0;
        dir = opendir(report->dir);
	if (dir == NULL) {
		perror("failed to open the directory");
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		struct stat file_stat;
		char full_path[PATH_MAX];
		sprintf(full_path, "%s/%s", report->dir, entry->d_name);

		if (stat(full_path, &file_stat) == 0) {
			if (S_ISREG(file_stat.st_mode)) {
				char* filename = malloc((sizeof(char) * strlen(full_path)) + 1);
				if (filename == NULL) {
					exit_code = -1;
					goto clean_up;
				}
				snprintf(filename, strlen(full_path)+1, "%s", full_path);

				if (add_file_to_scan(report, filename) != 0) {
					free(filename);
					exit_code = -1;
					goto clean_up;
				}
			}
		} else {
			perror("failed to stat the entry");
			exit_code = -1;
			goto clean_up;
		}
	}

	clean_up:
	closedir(dir);
	return exit_code;
}

int scan_file(struct file_report* report, char* filename) {
	FILE *file_ptr;
	file_ptr = fopen(filename, "r");
	if (file_ptr == NULL) {
		perror("failed to open file");
		return -1;
	}

	char ip[64], ident[64], authuser[64], date[32], timezone[10], method[8], url[4096], http_version[16], referer[4096], user_agent[256];
	intmax_t status = 0, size = 0;
	const char *format_string = "%63s %63s %63s [%63[^ ] %63[^]]] \"%15s %4095s %15[^\"]\" %d %d \"%4095[^\"]\" \"%255[^\"]\"";
	intmax_t resulting_bytes = 0;

	int exit_code = 0, cont = 1, c = 0, parsed = 0;

	while (cont) {
		parsed = fscanf(file_ptr, format_string, ip, ident, authuser, date, timezone, method, url, http_version, &status, &size, referer, user_agent);
		if (parsed == 12) {
			resulting_bytes += size;
			char* key;

			intmax_t* stored_value_ptr = (intmax_t*)hashmap_get(report->downloaded_per_url, url, strlen(url));
			if (stored_value_ptr == NULL) {
				intmax_t* value_to_store = malloc(sizeof(intmax_t));
				if (value_to_store == NULL) {
					printf("failed to allocate memory\n");
					exit_code = -1;
					goto clean_up;
				}

				key = strdup(url);
				if (key == NULL) {
					printf("failed to dupe string\n");
					exit_code = -1;
					goto clean_up;
				}

				*value_to_store = size;
				if (hashmap_put(report->downloaded_per_url, key, strlen(key), value_to_store) != 0) {
					printf("failed to put data into the hashmap\n");
					exit_code = -1;
					goto clean_up;
				}
			} else {
				*stored_value_ptr += size;
			}

			stored_value_ptr = (intmax_t*)hashmap_get(report->referer_count, referer, strlen(referer));
			if (stored_value_ptr == NULL) {
				intmax_t* value_to_store = malloc(sizeof(intmax_t));
				if (value_to_store == NULL) {
					printf("failed to allocate memory\n");
					exit_code = -1;
					goto clean_up;
				}

				key = strdup(referer);
				if (key == NULL) {
					printf("failed to dupe string\n");
					exit_code = -1;
					goto clean_up;
				}

				*value_to_store = 1;
				if (hashmap_put(report->referer_count, key, strlen(key), value_to_store) != 0) {
					printf("failed to put data into the hashmap\n");
					exit_code = -1;
					goto clean_up;
				}
			} else {
				*stored_value_ptr += 1;
			}

			continue;
		}

		while ((c = getc(file_ptr)) != '\n' && c != EOF);
		if (c == EOF) {
			cont = 0;
		}
	}

	report->bytes += resulting_bytes;

	clean_up:
	fclose(file_ptr);
	return exit_code;
}

static int merge_maps_call(void* const context, struct hashmap_element_s* const element) {
	struct hashmap_s* dest_map = (struct hashmap_s*) context;
	intmax_t* stored_value_ptr = (intmax_t*)hashmap_get(dest_map, element->key, element->key_len);
	if (stored_value_ptr == NULL) {
		intmax_t* value_to_store = malloc(sizeof(intmax_t));
		if (value_to_store == NULL) {
			return -1;
		}
		*value_to_store = *(intmax_t*)element->data;

		char* key = strdup(element->key);
		if (key == NULL) {
			free(value_to_store);
			printf("failed to dupe string\n");
			return -1;
		}

		if (hashmap_put(dest_map, key, strlen(key), value_to_store) != 0) {
			free(value_to_store);
			free(key);
			printf("failed to put data into the hashmap\n");
			return -1;
		}
	} else {
		*stored_value_ptr += *(intmax_t*)element->data;
	}

	return 0;
}

static int merge_maps(struct hashmap_s* src, struct hashmap_s* dest) {
	if (hashmap_iterate_pairs(src, merge_maps_call, (void *)dest) != 0) {
		return -1;
	}

	return 0;
}

int update_scan_report(struct scan_report* report, intmax_t served, struct hashmap_s* dpu_map, struct hashmap_s* rc_map) {
	if (report == NULL) {
		return -1;
	}

	pthread_mutex_lock(&(report->mutex));

	report->total_served += served;
	if (merge_maps(dpu_map, report->downloaded_per_url) != 0) {
		return -1;
	}
	if (merge_maps(rc_map, report->referer_count) != 0) {
		return -1;
	}

	pthread_mutex_unlock(&(report->mutex));
	return 0;
}

static int purge_map_call(void* const context __attribute__((unused)), struct hashmap_element_s* const element) {
    free(element->data);
    free((void*)element->key);

    return -1;
}

void* thread_func(void* arg) {
	struct scan_report* report = (struct scan_report*)(arg);
	int64_t exit_code = 0;
	char* filename = get_next_file_to_scan(report);
	if (filename != NULL) {
		struct file_report f_report = {0};
		struct hashmap_s dpu_map;
		if (hashmap_create(16384, &dpu_map) != 0) {
			perror("failed to create a hashmap\n");
			return ((void*)-1);
		}
		f_report.downloaded_per_url = &dpu_map;

		struct hashmap_s rc_map;
		if (hashmap_create(8192, &rc_map) != 0) {
			hashmap_destroy(&dpu_map);
			perror("failed to create a hashmap\n");
			return ((void*)-1);
		}
		f_report.referer_count = &rc_map;

		while (filename != NULL) {
			if (scan_file(&f_report, filename) != 0) {
				printf("failed to scan the %s file\n", filename);
				filename = get_next_file_to_scan(report);
				continue;
			}

			free(filename);
			filename = get_next_file_to_scan(report);
		}

		if (update_scan_report(report, f_report.bytes, f_report.downloaded_per_url, f_report.referer_count) != 0) {
			printf("failed to update the scan report\n");
			exit_code = 1;
		}

		hashmap_iterate_pairs(f_report.downloaded_per_url, purge_map_call, NULL);
		hashmap_iterate_pairs(f_report.referer_count, purge_map_call, NULL);
		hashmap_destroy(f_report.downloaded_per_url);
		hashmap_destroy(f_report.referer_count);
	}

	return ((void*)exit_code);
}

int compare_map_records(const void* a, const void* b) {
	struct map_record* elem_a = (struct map_record*)a;
	struct map_record* elem_b = (struct map_record*)b;

	if (elem_a->value < elem_b->value) {
	       	return 1;
	}

	if (elem_a->value > elem_b->value) {
	       	return -1;
	}

       	return 0;
}

int to_array_call(void* const context, struct hashmap_element_s* const element) {
	struct array_ctx* ctx = (struct array_ctx*)context;

	ctx->array[ctx->index].key = (char*)element->key;
	ctx->array[ctx->index].value = *(intmax_t*)element->data;

	ctx->index++;
	return 0;
}

int process_scan_report(struct scan_report* report) {
	unsigned int total_elements = hashmap_num_entries(report->downloaded_per_url);
	struct map_record* array = malloc(sizeof(struct map_record)* total_elements);
	struct array_ctx ctx = { .array = array, .index = 0 };

    	hashmap_iterate_pairs(report->downloaded_per_url, to_array_call, &ctx);
    	qsort(array, total_elements, sizeof(struct map_record), compare_map_records);

	printf("\nTop 10 URLs:\n");
    	for (unsigned int i = 0; i < 10; i++) {
        	printf("  \"%s\": %ld\n", array[i].key, array[i].value);
    	}

        free(array);

	total_elements = hashmap_num_entries(report->referer_count);
	array = malloc(sizeof(struct map_record)* total_elements);
	ctx.array = array;
	ctx.index = 0;

    	hashmap_iterate_pairs(report->referer_count, to_array_call, &ctx);
    	qsort(array, total_elements, sizeof(struct map_record), compare_map_records);

	printf("\nTop 10 Referers:\n");
    	for (unsigned int i = 0; i < 10; i++) {
        	printf("  \"%s\": %ld\n", array[i].key, array[i].value);
    	}

        free(array);

	printf("\nServed in total: %jd MB\n", report->total_served/(1024*1024));
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s <log directory> <number of threads>\n", argv[0]);
		exit(1);
	}

	int n;

        if (sscanf(argv[2], "%d", &n) != 1) {
		printf("failed to convert the \"%s\" argument to int\n", argv[2]);
		return 1;
	}

	if (n <= 0) {
		printf("wrong number of threads, will use a single thread for calculations\n");
		n = 1;
	}

	int exit_code = 0;
	struct scan_report* report = init_scan_report(argv[1]);
	if (report == NULL) {
		printf("failed to init scan report\n");
		return 1;
	}

	if (add_files_to_scan(report) != 0) {
		exit_code = 1;
		goto clean_up;
	}

	{
		pthread_t threads[n];
		for (int i = 0; i < n; i++) {
			if (pthread_create(&threads[i], NULL, thread_func, report) != 0) {
				perror("failed to create a thread");
				exit_code = 1;
				goto clean_up;
			}
		}

		for (int i = 0; i < n; i++) {
			if (pthread_join(threads[i], NULL) != 0) {
				perror("failed to join a thread");
				exit_code = 1;
				goto clean_up;
			}
		}
	}

	if (report->total_served != 0) {
		if (process_scan_report(report) != 0) {
			printf("failed to process the scan report\n");
		}
	} else {
		printf("no info found - check the input directory\n");
	}

	clean_up:
	hashmap_iterate_pairs(report->downloaded_per_url, purge_map_call, NULL);
	hashmap_iterate_pairs(report->referer_count, purge_map_call, NULL);
	hashmap_destroy(report->downloaded_per_url);
	hashmap_destroy(report->referer_count);
	pthread_mutex_destroy(&(report->mutex));
	free(report->referer_count);
	free(report->downloaded_per_url);
	free(report);

	return exit_code;
}
