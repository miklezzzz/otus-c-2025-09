#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define JPEG_MAGIC_NUMBER "0xFFD8FFE0"
#define EOCDR_SIGNATURE 0x06054b50
#define CDR_FILE_HEADER_SIGNATURE 0x02014b50

const unsigned char JPEG_SIGNATURE[4] = {0xFF, 0xD8, 0xFF, 0xE0};

#pragma pack(1)
struct headerStruct {
	uint16_t fileNameLength;
	uint16_t extraFieldLength;
	uint16_t fileLength;
};
#pragma pack(0)

typedef enum {
	JPEG    = (1 << 0),
	ZIP     = (1 << 1),
	ERR     = (1 << 2)
} result;

void printAllFilenames(FILE *filePointer, long crdOffset)
{
	long fileHeaderOffset = crdOffset;
	printf("The following files are found in the archive:\n");
	while (1) {
		// get to the next file header
		fseek(filePointer, fileHeaderOffset, SEEK_SET);
		uint32_t fileHeaderSignature;
		// check if there's a file header signature
		fread(&fileHeaderSignature, sizeof(fileHeaderSignature), 1, filePointer);
		if (fileHeaderSignature != CDR_FILE_HEADER_SIGNATURE) {
			// printf("file header not found, stop litsing\n");
			// exit if a file header signature wasn't found where it was supposed to be
			return;
		}

		// get to the file name len field
		fseek(filePointer, fileHeaderOffset + 28, SEEK_SET);
		struct headerStruct headerFields;
		// read the file name len, extra field len, file comm len fields
		fread(&headerFields, sizeof(headerFields), 1, filePointer);
		// get to the file name field
		fseek(filePointer, fileHeaderOffset + 46, SEEK_SET);
		char fName[headerFields.fileNameLength];
		fread(&fName, sizeof(char), headerFields.fileNameLength, filePointer);
		// set a null terminator
		fName[headerFields.fileNameLength] = '\0';
		printf("%s\n", fName);

		// update the pointer to point to the next feasible file header
		fileHeaderOffset = ftell(filePointer) + headerFields.extraFieldLength + headerFields.fileLength;
	}
}

int checkIfRarJpg(const char *fileName)
{
	FILE *filePointer;
	unsigned char buffer[4];
	int result = 0;

	filePointer = fopen(fileName, "rb");
	if (filePointer == NULL) {
		printf("Could not open the \"%s\" file.\n", fileName);
		return ERR;
	}

	// read the magic number to check if it's a jpeg file
	size_t bytesRead = fread(buffer, sizeof(char), 4, filePointer);
	if (bytesRead == 4 && memcmp(buffer, JPEG_SIGNATURE, 4) == 0) {
		result ^= JPEG;
	}

	fseek(filePointer, 0, SEEK_END);
	long fileSize = ftell(filePointer);
	// printf("file size is %ld\n", fileSize);
	long eocdrOffset = -1;

	// search for the end of central directory record by its signature
	for (long i = fileSize - 22; i >= 0 && i >= fileSize - (65536 + 22); --i) {
		fseek(filePointer, i, SEEK_SET);
		uint32_t eocdrSignature;
		fread(&eocdrSignature, sizeof(eocdrSignature), 1, filePointer);
		if (eocdrSignature == EOCDR_SIGNATURE) {
			eocdrOffset = i;
			break;
		}
	}

	if (eocdrOffset == -1) {
		// printf("Could not find end of central record offset\n");
		return result;
	}

	result ^= ZIP;

	// get to the offset of central directory field
	fseek(filePointer, eocdrOffset + 12, SEEK_SET);
	uint32_t cdrOffset;
	fread(&cdrOffset, sizeof(cdrOffset), 1, filePointer);
	// calculate the offset of the central directory using known offsets
	cdrOffset = fileSize-cdrOffset-(fileSize-eocdrOffset);
	// printf("cdr offset %d\n", cdrOffset);

	printAllFilenames(filePointer, cdrOffset);
	fclose(filePointer);
	return result;
}

int main(int argc, char *argv[])
{
	switch (argc) {
		case 1:
			printf("Please provide the filename to check. For example: \"rarjpg myfile\"\n");
			break;
		case 2:
			printf("Checking the \"%s\" file...\n", argv[1]);
			int res = checkIfRarJpg(argv[1]);
			if (res == ERR) {
				return 1;
			}

			printf("\nSummary: ");
			switch (res) {
				case (ZIP):
					printf("the \"%s\" file is a zip archive.\n", argv[1]);
					break;
				case (JPEG):
					printf("the \"%s\" file is an image.\n", argv[1]);
					break;
				case (ZIP | JPEG):
					printf("the \"%s\" file is a rarjpeg (zipjpeg, to be honset).\n", argv[1]);
					break;
				default:
					printf("the \"%s\" file has nothing to do neigher with jpeg nor zip.\n", argv[1]);
			}

			return 0;
		default:
			printf("More than one argument is provided. Please provide the filename as a single argument.\n");
			break;
	}

	return 1;
}
