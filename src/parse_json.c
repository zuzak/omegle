#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char *parse_json_keys(char *json, int want_result);
static char *parse_json_value(char *json, char *key);

/* TODO : */
/* key boundaries */
/* result bounadries */
static char *parse_json_keys(char *json, int want_result)
{
    int i;
    int json_start = 0; int json_started = 0;
    int json_end   = 0; int json_ended   = 0;
    int ticks = 0;
    /* char key[5000]; */

    char *key = (char *)malloc(sizeof(char)*1024);
    if(key == NULL) {
	return NULL;
    }

    char *result = (char *)malloc(sizeof(char)*1024);
    if(result == NULL) {
	return NULL;
    }

    /* Start searching for the beginning and end of our keys */
    /* or string values */
    for(i=0; i<strlen(json); i++) {
	if(json[i] == '"' && json_started == 0) {
	    json_start   = i;
	    json_started = 1;
	    json_ended   = 0;
	    ticks++;
	} else if(json[i] == '"' && json_started == 1) {
	    json_end     = i;
	    json_started = 0;
	    json_ended   = 1;
	    ticks++;
	}

	/* If we got keys copy them, move them to results */
	if(ticks == 2) {
	    int j=0,x=0;
	    for(j=json_start+1;j<json_end;j++) {
		key[x] = json[j];
		x++;
	    }
	    key[x] = 0x00;
	    ticks = 0;
	    /* if we request results otherwise we get keys */
	    /* only, good for checking with strstr for */
	    /* events */
	    if(want_result == 1) {
		if(parse_json_value(json, key) != NULL) {
		    strcat(result, key);
		    strcat(result, " ");
		}
	    } else if(want_result == 0) {
		strcat(result, key);
		strcat(result, " ");
	    }

	}
    }

    free(key);
    return result;
}

/* TODO: */
/* result boundaries */
static char *parse_json_value(char *json, char *key)
{
    int  i =0;
    char *key_ptr=NULL, *result=NULL;

    /* ptr to first occurance of key in json */
    key_ptr = strstr(json,key)+strlen(key)+1;
    if(key_ptr == NULL) {
    	printf("strstr in parse_json_value error\n");
    	exit(-1);
    }

    /* Allocate memory for results */
    if((result = (char *)malloc(sizeof(char)*200)) == NULL) {
    	perror("malloc()");
    	exit(-1);
    }
    bzero(result, 200);

    /* TODO: make sure we don't pass 200 of results */
    if(key_ptr[0] == ']') { 
    	return NULL;
    } else if(key_ptr[0] == ',') {
    	key_ptr++;
    	while(*key_ptr != ']') {
    	    if(*key_ptr != ' ') {
    		result[i] = *key_ptr;
    		i++;
    	    }
	    /* Add space if space is found */
	    if(*key_ptr == ' ') {
		result[i] = ' ';
		i++;
	    }
    	    key_ptr++;
    	}
    	return result;
    }

    return NULL;
}
