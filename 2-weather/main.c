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

int print_current_weather_from_json(char *json_string){
	struct json_object *parsed_json = json_tokener_parse(json_string);
	if (parsed_json == NULL) {
		printf("Error parsing JSON\n");
		return 1;
	}

	struct json_object *weather_obj;
	json_object_object_get_ex(parsed_json, "weather", &weather_obj);
	if (json_object_is_type(weather_obj, json_type_array)) {
		int array_length = json_object_array_length(weather_obj);
		if (array_length > 0) {
			struct json_object *todays_weather_obj = json_object_array_get_idx(weather_obj, 0);
			struct json_object *hourly_weather_obj;
			json_object_object_get_ex(todays_weather_obj, "hourly", &hourly_weather_obj);
			if (json_object_is_type(hourly_weather_obj, json_type_array)) {
				array_length = json_object_array_length(hourly_weather_obj);
				for (int i = 0; i < array_length; i++) {
					struct json_object *hourly_item_obj = json_object_array_get_idx(hourly_weather_obj, i);
					if (json_object_is_type(hourly_item_obj, json_type_object)) {
						struct json_object *temp_obj, *time_obj, *wind_dir_obj, *wind_speed_obj, *weather_desc_obj, *weather_val_obj;

						json_object_object_get_ex(hourly_item_obj, "time", &time_obj);
						int time = atoi(json_object_get_string(time_obj));
						time = time/100;
						printf("time: ");
						switch (time) {
							case 0:
								printf("00:00\n");
								break;
							default:
								printf("%d:00\n", time);
						}

						json_object_object_get_ex(hourly_item_obj, "weatherDesc", &weather_desc_obj);
						if (json_object_is_type(weather_desc_obj, json_type_array)) {
							int desc_array_length = json_object_array_length(weather_desc_obj);
							if (desc_array_length > 0) {
								json_object_object_get_ex(json_object_array_get_idx(weather_desc_obj, 0), "value", &weather_val_obj);
								if (json_object_is_type(weather_val_obj, json_type_string)) {
									printf("  weather description: %s\n", json_object_get_string(weather_val_obj));
								}
							}
						}

						json_object_object_get_ex(hourly_item_obj, "tempC", &temp_obj);
						if (json_object_is_type(temp_obj, json_type_string)) {
							printf("  temp: %s °C\n", json_object_get_string(temp_obj));
						}

						json_object_object_get_ex(hourly_item_obj, "winddir16Point", &wind_dir_obj);
						if (json_object_is_type(wind_dir_obj, json_type_string)) {
							printf("  wind direction: %s\n", json_object_get_string(wind_dir_obj));
						}

						json_object_object_get_ex(hourly_item_obj, "windspeedKmph", &wind_speed_obj);
						if (json_object_is_type(wind_speed_obj, json_type_string)) {
							printf("  wind speed: %s Kmph\n", json_object_get_string(wind_speed_obj));
						}
					}
				}
			}
		} else {
			printf("Could not find today's weather information in the output");
			return 1;
		}
	}

	return 0;
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
		// set 30s timeout
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

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
				return 1;
			}
		} else {
			printf("Failed to perform a request: %s\n", curl_easy_strerror(res));
			return 1;
		}

		res = print_current_weather_from_json(chunk.response);
		free(chunk.response);

		return res;
	}

	printf("Failed to init libcurl\n");
	return 1;
}

int main(int argc, char *argv[]) {
	switch (argc) {
		case 2:
			printf("Getting today's weather in %s\n", argv[1]);

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
