#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "parse_json.c"

#define BUF_SIZE 1024
#define WITH_RESULTS 1
#define WITHOUT_RESULTS 0

/* Proto types */
int create_tcp_socket();
char *get_ip(char *);
char *build_post(char *, char *, char *);
char *get_id();
char *get_event(char *);
void say_something(char *, char *);
void remchar(char *, char);
void reconnect();
struct sockaddr_in *create_remote();

struct omegle {
    char *id;        /* id */
    char *event;     /* current event from /events */
    char *ip;        /* omegle ip */
};

int sock = -1;
struct omegle user;

int main() {

    /* Get omegle.com IP */
    if((user.ip = get_ip("omegle.com")) == NULL)
	exit(-1);

    /* Get user stranger ID */
    user.id = get_id();
    remchar(user.id, '"');
    printf("[+] Stranger id %s\n", user.id);
    fflush(stdout);

    /***********************************/
    /*      checking events started    */
    /***********************************/
    char *message = NULL;
    char *count   = NULL;
    while(1) {
	user.event = get_event(user.id);
	/* printf("[+] Event %s\n", user.event); */

	/* Are we waiting */
	if(strstr(parse_json_keys(user.event, WITHOUT_RESULTS), "waiting") != NULL) {
	    printf("Waiting...\n");
	}

	/* Got connected */
	if(strstr(parse_json_keys(user.event, WITHOUT_RESULTS), "connected") != NULL) {
	    printf("We are connected to the chat\n");
	}
	
	/* Got count */
	if(strstr(parse_json_keys(user.event, WITH_RESULTS), "count") != NULL) {
	    count = parse_json_value(user.event, "count");
	    printf("Count : %s\n", count);
	    free(count);
	}

	/* Stranger typing */
	if(strstr(parse_json_keys(user.event, WITHOUT_RESULTS), "typing") != NULL) {
	    printf("Stranger typing...\n");
	}
	/* Stranger stopped typing */
	if(strstr(parse_json_keys(user.event, WITHOUT_RESULTS), "stoppedTyping") != NULL) {
	    printf("Stranger stopped typing...\n");
	}

	/* Got message */
	if(strstr(parse_json_keys(user.event, WITH_RESULTS), "gotMessage") != NULL) {
	    char *message = parse_json_value(user.event, "gotMessage");
	    printf("[>] Stranger : %s\n", message);
	    free(message);

	    /* Sleep for three seconds to send message */
	    sleep(3);

	    /* Send a message when ever we get one just for testing */
	    say_something("Hey what's up?", user.id);
	}

	/* Stranger disconnected */
	if(strstr(parse_json_keys(user.event, WITHOUT_RESULTS), "strangerDisconnected") != NULL) {
	    printf("Stranger disconnected\n");
	    printf("Because events are : %s\n", user.event);
	    /* Reconnect when stranger disconnects */
	    free(user.id);
	    reconnect();
	}

	free(user.event);
    }
    /***********************************/
    /*      checking events ended      */
    /***********************************/


    /* cleaning allocated memory */
    free(user.id);
    free(user.ip);
    return EXIT_SUCCESS;
}

/* Reconnect when stranger disconnects */
void reconnect()
{
    printf("Reconnecting()\n");
    user.id = get_id();
    remchar(user.id, '"');
    printf("[+] New Stranger id is %s\n", user.id);
    fflush(stdout);
    
}


/* Remove a specific char from string */
void remchar(char *string, char o)
{
    char *read, *write;
    for(read = write = string; *read != '\0'; ++read) {
        if(*read != o) {
            *(write++) = *read;
        }
    }
    *write = '\0';
}

/* A function to send message to omegle */
/* TODO: */
/* message bounadires */
void say_something(char *message, char *id)
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_request;

    /* Creating socket */
    if((sock = create_tcp_socket()) < 0) {
	printf("error while creating socket\n");
	exit(-1);
    }

    remote = create_remote();

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0) {
    	    perror("connect()");
    	    exit(-1);
    }

    /* Create the POST request to get stranger id */
    char message_command[2000];
    sprintf(message_command, "send=%s&id=%s", message, id);
    post_request = build_post(user.ip, "/send", message_command);

    /* Sending post request to server and get results */
    int sent = 0;
    while(sent < (int)strlen(post_request)) {
	tempres = send(sock, post_request+sent, strlen(post_request)-sent, 0);
	if(tempres == -1) {
	    perror("send()");
	    exit(-1);
	}
	sent += tempres;
    }

    printf("[+] You : %s\n", message);

    free(remote);
    free(post_request);
    close(sock);    
}


/* Create sockaddr_in struct */
struct sockaddr_in *create_remote()
{
    struct sockaddr_in *remote;
    remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    remote->sin_family = AF_INET;
    if((inet_pton(AF_INET, user.ip, (void *)&remote->sin_addr.s_addr)) < 0) {
	perror("inet_pton()");
	exit(-1);
    }
    remote->sin_port = htons(80);
    return remote;
}

/* Get Event */
char *get_event(char *id)
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_request;
    char buf[BUF_SIZE+1];
    char *result;

    /* Creating socket */
    if((sock = create_tcp_socket()) < 0) {
	printf("error while creating socket\n");
	exit(-1);
    }

    remote = create_remote(); 

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0) {
    	    perror("connect()");
    	    exit(-1);
    }

    /* Create the POST request to get stranger id */
    char event_command[2000];
    sprintf(event_command, "id=%s", id);
    post_request = build_post(user.ip, "/events", event_command);

    /* Sending post request to server and get results */
    int sent = 0;
    while(sent < (int)strlen(post_request)) {
	tempres = send(sock, post_request+sent, strlen(post_request)-sent, 0);
	if(tempres == -1) {
	    perror("send()");
	    exit(-1);
	}
	sent += tempres;
    }

    /* Reciving the data from server */
    memset(buf, 0x00, sizeof(buf));
    int htmlstart = 0;
    char *event;
    while((tempres = recv(sock, buf, BUF_SIZE, 0)) > 0) {
	if(htmlstart == 0) {
	    event = strstr(buf, "\r\n\r\n");
	    if(event != NULL) {
		htmlstart = 1;
		event += 2;
	    }
	} else {
	    event = buf;
	}

	/* If there's an event copy it for return */
	if(htmlstart) {
	    result = (char *)malloc(sizeof(char)*2500);
	    if(result == NULL) {
		perror("malloc()");
		exit(-1);
	    }
	    memcpy(result, event, 2500);
	}
	memset(buf, 0, tempres);
    }

    if(tempres < 0) {
	perror("Error reciving data");
	exit(-1);
    }

    free(remote);
    free(post_request);
    close(sock);

    return result;
}

/* Get Stranger ID */
char *get_id()
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_request;
    char buf[BUF_SIZE+1];
    char *result;

    /* Creating socket */
    if((sock = create_tcp_socket()) < 0)
	exit(-1);


    remote = create_remote();

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0) {
	    perror("connect()");
	    exit(-1);
    }

    /* Create the POST request to get stranger id */
    post_request = build_post(user.ip, "/start", "");

    /* Sending post request to server and get results */
    int sent = 0;
    while(sent < (int)strlen(post_request)) {
	tempres = send(sock, post_request+sent, strlen(post_request)-sent, 0);
	if(tempres == -1) {
	    perror("send()");
	    exit(-1);
	}
	sent += tempres;
    }

    /* Reciving the data from server */
    memset(buf, 0x00, sizeof(buf));
    int htmlstart = 0;
    char *id;
    while((tempres = recv(sock, buf, BUF_SIZE, 0)) > 0) {
	if(htmlstart == 0) {
	    id= strstr(buf, "\r\n\r\n");
	    if(id != NULL) {
		htmlstart = 1;
		id += 4;
	    }
	} else {
	    id = buf;
	}

	/* If we found a stranger id then copy it and
	 at the end of function */
	if(htmlstart) {
	    result = (char *)malloc(sizeof(char)*100);
	    if(result == NULL) {
		perror("malloc()");
		exit(-1);
	    }
	    memcpy(result, id, 100);
	}
	memset(buf, 0, tempres);
    }

    if(tempres < 0) {
	perror("Error reciving data");
	exit(-1);
    }

    free(remote);
    free(post_request);
    close(sock);

    return result;
}


/* Get ip of hostname */
char *get_ip(char *host)
{
    struct hostent *hent;
    int iplen = 15;  /* xxx.xxx.xxx.xxx */
    char *ip = (char *)malloc(sizeof(char)*(iplen+1));
    if(ip == NULL) {
	perror("malloc()");
	return NULL;
    }
    memset(ip, 0x00, iplen+1);

    /* Get gethostbyname */
    if((hent = gethostbyname(host)) == NULL) {
	perror("gethostbyname()");
	return NULL;
    }

    /* Resolve hostname */
    if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL) {
	perror("inet_ntop()");
	return NULL;
    }
    return ip;
}

/* Create a socket fd and return it's number */
int create_tcp_socket()
{
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("socket()");
	return -1;
    }
    return sock;
}


/* Send Post request */
char *build_post(char *host, char *page, char *data)
{
    char *query;
    char *command  = "POST /%s HTTP/1.0\r\n\
HOST: %s\r\n\
User-Agent: Omeglebot\r\n\
Accept: text/javascript, text/html, application/xml, text/xml, */*\r\n\
Accept-Language: en-gb,en;q=0.5\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n\
Connection: keep-alive\r\n\
Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n\
Referer: http://omegle.com/\r\n\
Pragma: no-cache\r\n\
Cache-Control: no-cache\r\n\
Content-Length: %d\r\n\r\n%s\r\n";
    char *fixedpage = page;

/* Keep-Alive: 300\r\n\ */

    if(fixedpage[0] == '/') {
	fixedpage = page + 1;
    }


    query = (char *)malloc(sizeof(char)*(strlen(host)+strlen(fixedpage)+strlen("Omeglebot")
					 +strlen(command)+5+strlen(data)));
    if(query == NULL) {
	perror("malloc()");
	exit(-1);
    }

    sprintf(query, command, fixedpage, host, strlen(data), data);
    /* printf(">>>>>>>>>>>> %s <<<<<<<<<<<<<<\n", query); */
    return query;
}
