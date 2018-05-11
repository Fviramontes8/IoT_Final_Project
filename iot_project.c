#include <stdio.h>
#include <syslog.h> //For syslog()
#include <signal.h> //For SIGHUP, SIGTERM
#include <stdbool.h> //For true
#include <unistd.h> //For sleep()
#include <errno.h> //For errno
#include <time.h>
#include <stdlib.h> //strtol()
#include <unistd.h> //getopt()
#include <curl/curl.h> //Everything in curl_get()
#include <string.h> //For strerror()
#include <sys/stat.h> //For S_IRUSR, S_IWUSR, S_IRGRP, S_IROTH
#include "cJSON.h"

#define URL "18.188.242.239:8000"

#define OK 0
#define ERR_FORK 1
#define ERR_SETSID 2
#define ERR_CHDIR 3
#define ERR_WTF 4

#define DAEMON_NAME "iot_projd"

typedef struct {
	int hr;
	int min;
}time_boi;

typedef struct {
	time_boi times[3];
	time_boi curr_time;
	int temp[3];
}config_time;

config_time configs;

void string_to_time(char *time_bois, time_boi *t) {
	int hr, min;
	sscanf(time_bois, "%d:%d", &hr, &min);
	t->hr = hr;
	t->min = min;
}


//Parses JSON content from cloud and updates config file
void parse_JSON(char *strJson, size_t nmemb) {
	cJSON *root = cJSON_Parse(strJson);

	//Updates
	char *day = cJSON_GetObjectItem(root, "day")->valuestring;
	char *morn = cJSON_GetObjectItem(root, "morning")->valuestring;
	char *night = cJSON_GetObjectItem(root, "night")->valuestring;

	//string_to_time(day, &configs.times[0]);
	//string_to_time(morn, &configs.times[1]);
	//string_to_time(night, &configs.times[2]);
	printf("Day is: %s\n", day);
	printf("Morning is: %s\n", morn);
	printf("Night is: %s\n", night);

	cJSON_Delete(root);

}

size_t get_call(void *ptr, size_t size, size_t nmemb, void *stream) {
	printf("INput: %s\n", (char *) ptr);
	parse_JSON(ptr, nmemb);

	return size * nmemb;
}



//No input, output is value from /tmp/temp
int read_temp() {
	FILE* fp = fopen("/tmp/temp", "rb");
	if(fp == NULL) {
		printf("Error opening file\n");
		return 1;
	}
	char temp_str[50];
	fgets(temp_str, sizeof(temp_str), fp);
	fclose(fp);
	char *p;
	int temp = strtol(temp_str, &p, 10);

	//printf("The temp is: %d\n", temp);
	return temp;
}

//No input, returns current hour in 24-hr format
int read_hour() {
	time_t current_time;
	struct tm *tm;
	int hour;

	current_time = time(NULL);
	tm = localtime(&current_time);
	hour = tm->tm_hour;

	return hour;
	//printf("Time: %d:%d\n", hour, minute);
}

//No input, returns current minute
int read_min() {
	time_t current_time;
	struct tm *tm;
	int min;

	current_time = time(NULL);
	tm = localtime(&current_time);
	min = tm->tm_min;

	return min;
	//printf("Time: %d:%d\n", hour, minute);
}

/*
struct json_string{
	char obj_ID[100];
	char morn[100];
	char day[100];
	char night[100];
}json_data;

void string_parser(char *string) {
	char body[400] = {"\0"};
	strcpy(body, string);
	memset(json_data.obj_ID, 0 100);
	memset(json_data.morn, 0, 100);
	memset(json_data.day, 0, 100);
	memset(json_data.night, 0, 100);

	char *ptr;
	ptr = strtok(body, ",");
	while(ptr != NULL) {
		switch(count) {
			case 0:
				strcpy(json_data.obj_ID, ptr);
				break;
			case 1:
				strcpy(json_data.morn, ptr);
				break;
			case 2:
				strcpy(json_data.day, ptr);
				break;
			case 3:
				strcpy(json_data.night, ptr);
				break;
			default:
				printf("Error parsing json data\n");
				break;
		}
		count++;
		ptr = strtok(NULL, ",");
	}
}
*/


//Struct to get a string from curl get function
struct curl_string {
	char *ptr;
	size_t len;
};

//Initializing the string for curl function
void curl_init_string(struct curl_string *s) {
	s->len = 0;
	s->ptr = malloc(s->len+1);
	if(s->ptr == NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

size_t write_fn(void *ptr, size_t size, size_t nmember, struct curl_string *s) {
	size_t new_len = s->len + size*nmember;
	s->ptr = realloc(s->ptr, new_len+1);
	if(s->ptr == NULL) {
		fprintf(stderr, "realloc failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr+s->len, ptr, size*nmember);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmember;
}


struct curl_string curl_get(struct curl_string s) {
	curl_init_string(&s);

	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	long http_code = 0;
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, URL);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_fn);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_call);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl could not execute --get: %s\n", curl_easy_strerror(res));
		}
		char *ct = NULL;
		curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
		printf("Content-Type: %s\n", ct);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		printf("\nHTTP CODE: %ld\n", http_code);
		curl_easy_cleanup(curl);
	}
	printf("Henlo, get: %s pls.\n", URL);
	return s;
}

static void _signal_handler(const int signal) {
	switch(signal) {
		case SIGHUP:
			break;
		case SIGTERM:
			syslog(LOG_INFO, "Received SIGTERM,\
			exiting program.\n");

			closelog();
			exit(OK);
			break;
		default:
			syslog(LOG_INFO, "Received unhandled signal.\n");
	}
}

void _main_func() {

	syslog(LOG_INFO, "Starting main function\n");

	while(1){
		int temp = read_temp();
	//printf("Tempurature is: %d\n", temp);

		if(temp > 70) {
			FILE *filep;
			filep = fopen("/tmp/status", "wb");
			char *status = "OFF";
			fprintf(filep, "%s", status);
			fclose(filep);
			syslog(LOG_INFO, "OFF\n");

		}
		else if (temp < 60) {
			FILE *filep;
			filep = fopen("/tmp/status", "wb");
			char *status = "ON";
			fprintf(filep, "%s", status);
			fclose(filep);
			syslog(LOG_INFO, "ON\n");
		}
		sleep(5);
	}

	/*
	int min = read_min();
	int hr = read_hour();
	printf("The time(24 hr) is: %d%d\n", hr, min);

	struct curl_string s = curl_get(s);
	printf("String: %s\n", s.ptr);
	//printf("Cut up string: %zu\n", s.len);
	*/

	/*
	FILE *filep;
	filep = fopen("/tmp/get_response.txt", "wb");
	if(filep == NULL) {
		printf("Error opening file\n");
		return 1;
	}

	fprintf(filep, s.ptr, s.len);
	fclose(filep);

	filep = fopen("/tmp/get_response.txt", "rb");
	if(filep == NULL) {
		printf("Error opening file\n");
		return 1;
	}
	char get_r_str[50];
	fgets(get_r_str, sizeof(get_r_str), filep);
	fclose(filep);

	int i;
	for(i=0; i < 40; i++) {
		printf("Piece '%s'\n", &get_r_str[i]);
	}
	*/

	//free(s.ptr);
}

int main(void) {
	openlog(DAEMON_NAME, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_DAEMON);
	syslog(LOG_INFO, "Starting sampled\n");
	
	//Creating child process
	pid_t pid = fork();
	printf("Child process is made\n");

	if(pid < 0) {
		syslog(LOG_ERR, "Help: %s\n", strerror(errno));
		return ERR_FORK;
	}

	//Getting rid of parent process
	if(pid > 0) {
		return OK;
	}
	
	//Creating session id
	if(setsid() < -1) {
		syslog(LOG_ERR, "Help: %s\n", strerror(errno));
		return ERR_SETSID;
	}
	printf("Session ID was made\n");

	//Closing file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	//Set umask
	umask(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	//Setting up working directory
	//Should be changed from root to /log/syslog
	if(chdir("/") < 0) {
		syslog(LOG_ERR, "Help: %s\n", strerror(errno));
		return ERR_CHDIR;
	}

	//Setting signal handler [_signal_handler is a custom fn]
	signal(SIGTERM, _signal_handler);
	signal(SIGHUP, _signal_handler);

	_main_func();

	return 0;
}
