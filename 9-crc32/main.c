#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define CHUNK_SIZE (1024 * 1024 * 1000)

uint32_t crc32_bitwise(uint32_t crc, char ch) {

	for (size_t j = 0; j < 8; j++) {
		uint32_t b = (ch ^ crc) & 1;
		crc >>= 1;
		if (b) {
			crc = crc ^ 0xEDB88320;
		}
		ch >>= 1;
	}

	return crc;
}

int main(int argc, char *argv[]) {
	int fd;
	struct stat file_stat;
	off_t filesize;
	off_t chunk_size = CHUNK_SIZE;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		exit(1);
	}

	if ((fd = open(argv[1], O_RDONLY)) == -1) {
		perror("failed to open the file");
		exit(1);
	}

	if (fstat(fd, &file_stat) == -1) {
		perror("failed to stat the file");
		close(fd);
		exit(1);
	}

	size_t page_size = sysconf(_SC_PAGE_SIZE);
	// if chunk_size smaller than the page_size, we increase up to the page size
	if (chunk_size < (long int)page_size) {
		chunk_size = page_size;
	} else {
		chunk_size = (chunk_size / page_size) * page_size;
	}

	filesize = file_stat.st_size;
	uint32_t crc = 0xFFFFFFFF;

	for (off_t current_offset = 0; current_offset < filesize; current_offset += chunk_size) {
		// length of the mmap call is up to the chunk_size
		size_t map_len = chunk_size;
		// or the remainder of the file
		if ((long int)(current_offset + map_len) > filesize) {
			map_len = filesize - current_offset;
		}

		char *data = mmap(NULL, map_len, PROT_READ, MAP_PRIVATE, fd, current_offset);
		if (data == MAP_FAILED) {
			perror("failed to mmap the file");
			close(fd);
			exit(1);
		}

		printf("\n--- Processing chunk starting at file offset %ld, (mapped length %zu) ---\n", current_offset, map_len);
		for (size_t i = 0; i < map_len; i++) {
			crc = crc32_bitwise(crc, data[i]);
		}

		if (munmap(data, map_len) == -1) {
			perror("failed to munmap the file");
		}
	}

	printf("Crc32: %" PRIx32 "\n", ~crc);
	close(fd);
	return 0;
}
