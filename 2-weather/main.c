#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define NOT_FOUND 404

char *URL_PATTERN = "https://wttr.in/%s?format=j1";

struct memory {
	char *response;
	size_t size;
};

// example cb function from https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
static size_t cb(char *data, size_t size, size_t nmemb, void *clientp)
{
	size_t realsize = nmemb;
	struct memory *mem = (struct memory *)clientp;

	char *ptr = realloc(mem->response, mem->size + realsize + size);
	if(!ptr) {
		return 0;
	}

	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;

	return realsize;
}

int get_weather(const char *cityname) {
	// init curl handle
	CURL *curl = curl_easy_init();
	if (curl) {
		char url[100];
		// format the url
		int res = snprintf(url, sizeof(url), URL_PATTERN, cityname);
	 	if (res < 0) {
			printf("Failed to format the url\n");
			return 1;
		}

		// set the url
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);

		// init with zeroes
		struct memory chunk = {0};
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

		// perform the request
		res = curl_easy_perform(curl);
		// cleanup 
		curl_easy_cleanup(curl);
		
		if (res == CURLE_OK) {
			long status_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
			if (status_code == NOT_FOUND) {
				printf("Location %s not found\n", cityname);
				res = 1;
			}
		} else {
			printf("Failed to perform a request: %s\n", curl_easy_strerror(res));
		}

		printf("%s", chunk.response);

		free(chunk.response);

		return res;
	}

	printf("Failed to init libcurl\n");
	return 1;
}

int main(int argc, char *argv[]) {
	switch (argc) {
		case 2:
			printf("Getting current weather for %s\n", argv[1]);

			// init curl global environment
			CURLcode global_init_res = curl_global_init(CURL_GLOBAL_ALL);
			if (global_init_res != CURLE_OK) {
				printf("Failed to init curl global environment: %s\n", curl_easy_strerror(global_init_res));
				return 1;
			
			}
			int res = get_weather(argv[1]);
			curl_global_cleanup();

			return res;

		default:
			printf("Example usage: solution Moscow...\n");
	}

	return 1;
}
