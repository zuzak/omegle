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
#include <errno.h>

#include <sys/shm.h>
#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>

#define BUF_SIZE 1024
#define WITH_RESULTS 1
#define WITHOUT_RESULTS 0

typedef struct {
    char *id;        /* id */
    char *event;     /* current event from /events */
    char *ip;        /* omegle ip */
} omegle;

typedef struct {
    GtkEntry *textbox;
    GtkTextView *chatbox;

    GtkTextBuffer *buffer;	/* for chatbox */
    GtkTextMark *mark;		/* for chatbox */
    GtkTextIter iter;		/* for chatbox */
    const gchar   *text;	/* for chatbox */

    omegle *gtk_user;
} Widgets;


/* Globals */
int sock = -1;

/* Proto types */
int create_tcp_socket();
char *get_ip(char *host);
char *build_post(char *host, char *page, char *data);
void get_id(omegle *user);
void remchar(char *string, char o);
void reconnect(omegle *user);
void say_something(omegle *user, char *message);
int check_event(omegle *user, char *event, int with_result);
void parse_events(omegle *user, Widgets *w);
void get_event(omegle *user);
void chatBox_write(char *message, Widgets *w);
G_MODULE_EXPORT void next_clicked_cb (GtkButton *button, Widgets *w);
void *gtk_event_update(void *args);
gboolean on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data);

int main(int argc, char **argv) {
    
    omegle *user;
    GtkBuilder *builder;
    GtkWidget  *window;
    Widgets    *w;

    /* user */
    int shk_omegle = 0xd34d0000;
    int shi_omegle = 0;
    void *shm_omegle = (void *)0;
    /* user->events */
    int shk_events = 0xd341111;
    int shi_events = 0;
    char *shm_events = (char *)0;
    /* user->id */
    int shk_id = 0xd34d2222;
    int shi_id = 0;
    char *shm_id = (char *)0;
    /* user->ip */
    int shk_ip = 0xd34d3333;
    int shi_ip = 0;
    char *shm_ip = (char *)0;
    /* w */
    int shk_w = 0xd34d4444;
    int shi_w = 0;
    void *shm_w = (void *)0;

    int pid;

    pthread_t gtk_event_upd;
    
    /**********************************************/
    /* Setting up shared memory for our structure */
    /* and for the events			  */
    /**********************************************/
    /* Creating the shared memory for the struct */
    if((shi_omegle = shmget(shk_omegle, 1024, 0644|IPC_CREAT)) == -1)
    {
	perror("shmget()");
	return -1;
    }
    shm_omegle = shmat(shi_omegle, (void *)0, 0);
    if(shm_omegle == (void *)-1)
    {
	perror("shmat()");
	return -1;
    }
    user = (omegle *)shm_omegle;
    /* Creating the shared memory for events */
    if((shi_events = shmget(shk_events, 1024, 0644|IPC_CREAT)) == -1)
    {
	perror("shmget()");
	return -1;
    }   
    if((shm_events= shmat(shi_events, (void *)0, 0)) == (char *)-1)
    {
	perror("shmat()");
	return -1;
    }
    user->event = (char *)shm_events;
    /* Creating the shared memory for id */
    if ((shi_id = shmget(shk_id, 1024, 0644|IPC_CREAT)) == -1)
    {
	perror("shmget()");
	return -1;
    }
    if ((shm_id = shmat(shi_id, (void *)0, 0)) == (char *)-1)
    {
	perror("shmat()");
	return -1;
    }
    user->id = (char *)shm_id;
    /* Creating shared memory for ip */
    if ((shi_ip = shmget(shi_ip, 1024, 0644|IPC_CREAT)) == -1)
    {
	perror("shmget()");
	return -1;
    }
    if ((shm_ip = shmat(shi_ip, (void *)0, 0)) == (char *)-1)
    {
	perror("shmat()");
	return -1;
    }
    user->ip = (char *)shm_ip;
    /* Creating shared memory for Widget w */
    if ((shi_w = shmget(shi_w, 1024, 0644|IPC_CREAT)) == -1)
    {
	perror("shmget()");
	return -1;
    }
    if ((shm_w = shmat(shi_w, (void *)0, 0)) == (void *)-1)
    {
	perror("shmat()");
	return -1;
    }
    w = (Widgets *)shm_w;
    w->gtk_user = user;

    /*********************************/
    /* Init. widget and link objects */
    /*********************************/
    g_thread_init(NULL);	/* init threads */
    gdk_threads_init();		/* init threads */
    gdk_threads_enter();	/* init threads */
    gtk_init(&argc, &argv);
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "src/widget.xml", NULL);
    /* Linking with the code */
    window     = GTK_WIDGET( gtk_builder_get_object(builder, "window") );
    g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (on_key_press), w); /* on key press all */
    w->textbox = GTK_ENTRY ( gtk_builder_get_object(builder, "textbox") );
    w->chatbox = GTK_TEXT_VIEW ( gtk_builder_get_object(builder, "chatbox") );
    gtk_text_view_set_editable(w->chatbox, 0); /* w->chatbox not editable */
    /* Connecting signals */
    gtk_builder_connect_signals (builder, w);
    g_object_unref (G_OBJECT (builder));
    /* Get omegle.com IP */
    if((user->ip = get_ip("omegle.com")) == NULL)
    	exit(-1);
    /* Get user stranger ID */
    get_id(user); remchar(user->id, '"');
    chatBox_write("ID : ", w);
    chatBox_write(user->id, w);
    chatBox_write("\n", w);

    /***********/
    /* Start.. */
    /***********/
    if ((pid = fork()) < 0)
    {
    	perror("fork()");
    	return -1;
    } else if (pid == 0)	/* Child */
    {
	while(1)
    	    get_event(user);	/* Get events */
    } else {			/* Parent */
	pthread_create(&gtk_event_upd, NULL,
		       gtk_event_update, w); /* Event updater */
	gtk_widget_show_all(window);
	gtk_main();
    }

    return EXIT_SUCCESS;
}

/********************************************/
/* To handle key presses		    */
/* currenty handles : enter -> send message */
/********************************************/
gboolean
on_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    Widgets *w = (Widgets*)user_data; /* Yep it worked */
    
    switch (event->keyval)
    {
    case 65293:			/* Enter */
	/* Insert what ever you typed in the chatbox */
	w->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w->chatbox));
	w->text   = gtk_entry_get_text (GTK_ENTRY (w->textbox));
	w->mark   = gtk_text_buffer_get_insert (w->buffer);
	gtk_text_buffer_get_iter_at_mark (w->buffer, &w->iter, w->mark);
	gtk_text_buffer_insert (w->buffer, &w->iter, "You : ", -1);
	gtk_text_buffer_insert (w->buffer, &w->iter, w->text, -1);
	gtk_text_buffer_insert (w->buffer, &w->iter, "\n", -1);
	say_something(w->gtk_user, (char *)w->text); /* Send the text to the user */
	gtk_entry_set_text(w->textbox, "");	/* Clear the textbox */
	/* Scroll down to the end */
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(w->chatbox),
				     gtk_text_buffer_get_insert(w->buffer), 0.0, TRUE, 0.5, 0.5);
	break;
    default:
	return FALSE; 
    }

    return FALSE; 
}

/************************************/
/* A thread to check for events and */
/* print them to the chatbox	    */
/* @param events                    */
/************************************/
void *gtk_event_update(void *args)
{
    Widgets *w_cpy = (Widgets *)args;
    char previous_event[1024], message[1024];

    for(;;) {
	usleep(25);		/* a small delay */
	if(strcmp(w_cpy->gtk_user->event, previous_event)
	   && strcmp(w_cpy->gtk_user->event, "null\r\n")
	   && strcmp(w_cpy->gtk_user->event, "\r\n")
	   && strcmp(w_cpy->gtk_user->event, "")
	    ) {
	    gdk_threads_enter(); /* enter thread */
	    memset(message, 0x00, 1024);
	    strncpy(message, w_cpy->gtk_user->event, 1024);
	    parse_events(w_cpy->gtk_user, w_cpy); /* Parse events */
	    gdk_threads_leave(); /* leave thread */
	 }

	memset(previous_event, 0x00, 1024);
	strncpy(previous_event, w_cpy->gtk_user->event, 1024);
    }
    return NULL;
}

/*****************************/
/* Find another conversation */
/* @param button, widgets    */
/*****************************/
G_MODULE_EXPORT void
next_clicked_cb (GtkButton *button, Widgets *w)
{
    chatBox_write("ID : ", w);
    reconnect(w->gtk_user);
    chatBox_write(w->gtk_user->id, w);
    chatBox_write("\n", w);
    return;
}

/***************************/
/* Write to chatbox	   */
/* @param message, Widgets */
/***************************/
void chatBox_write(char *message, Widgets *w)
{
    w->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w->chatbox));
    w->mark    = gtk_text_buffer_get_insert (w->buffer);
    gtk_text_buffer_get_iter_at_mark (w->buffer, &w->iter, w->mark);
    gtk_text_buffer_insert (w->buffer, &w->iter, message, -1);
    return;
}

/********************/
/* Parse user event */
/********************/
void parse_events(omegle *user, Widgets *w)
{
    char *count;
    /* Are we waiting */
    if(check_event(user, "waiting", WITHOUT_RESULTS))
	chatBox_write("Waiting....\n", w);
    /* Got connected */
    if(check_event(user, "connected", WITHOUT_RESULTS))
	chatBox_write("We are connected to chat\n", w);
    /* Got count */
    if(check_event(user, "count", WITH_RESULTS)) {
	count = parse_json_value(user->event, "count");
	chatBox_write("Count : ", w);
	chatBox_write(count, w);
	chatBox_write("\n", w);
	free(count);
    }
    /* Stranger typing */
    if(check_event(user, "typing", WITHOUT_RESULTS))
	chatBox_write("Stranger typing...\n", w);
    /* Stranger stopped typing */
    if(check_event(user, "stoppedTyping", WITHOUT_RESULTS))
	chatBox_write("Stranger stopped typing...\n", w);
    /* Got message */
    if(check_event(user, "gotMessage", WITH_RESULTS)) {
	char *message = parse_json_value(user->event, "gotMessage");
	chatBox_write("Stranger : ", w);
	chatBox_write(message, w);
	chatBox_write("\n", w);
	free(message);
    }
    /* Stranger disconnected */
    if(check_event(user, "strangerDisconnected", WITHOUT_RESULTS)) {
	chatBox_write("Stranger Disconnected\n", w);
	chatBox_write("Reconnecting...\n", w);
	reconnect(user);
    }
    return;
}
 
/*****************************************/
/* Check if event exists in user->events */
/* @param  user, event, with_results     */
/* @return 1-available, 0-not-available	 */
/*****************************************/
int check_event(omegle *user,
		char *event, int with_result) {
    if(strstr(parse_json_keys(user->event, with_result), event) != NULL)
	return 1;
    return 0;
}

/*****************************************/
/* Reconnect when stranger disconnects	 */
/* TODO : send disconnect before getting */
/* a new stranger id			 */
/*****************************************/
void reconnect(omegle *user)
{
    user->ip = NULL;
    /* Get omegle.com IP */
    if((user->ip = get_ip("omegle.com")) == NULL)
    	exit(-1);
    /* Get user stranger ID */
    get_id(user);
    remchar(user->id, '"');
    return;
}

/**************************************/
/* Remove a specific char from string */
/**************************************/
void remchar(char *string, char o)
{
    char *read, *write;
    for(read = write = string; *read != '\0'; ++read) {
        if(*read != o) {
            *(write++) = *read;
        }
    }
    *write = '\0';
    return;
}

/*******************************/
/* Send message to omegle user */
/* @param user, message        */
/* TODO: boundries checking    */
/*******************************/
void say_something(omegle *user, char *message)
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_request;

    /* Creating socket */
    if((sock = create_tcp_socket()) < 0) {
	fprintf(stderr,"error while creating socket\n");
	exit(-1);
    }

    remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (remote == NULL)
    {
	perror("malloc()");
	exit(-1);
    }
    remote->sin_family = AF_INET;
    if((inet_pton(AF_INET, user->ip, (void *)&remote->sin_addr.s_addr)) < 0) {
	perror("inet_pton()");
	exit(-1);
    }
    remote->sin_port = htons(80);

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0) {
    	    perror("connect()");
    	    exit(-1);
    }

    /* Create the POST request to get stranger id */
    char message_command[2000];
    sprintf(message_command, "msg=%s&id=%s", message, user->id);
    post_request = build_post(user->ip, "/send", message_command);

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

    free(remote);
    free(post_request);
    close(sock);
    return;
}


/**********************/
/* Get event for user */
/* @set user->event   */
/**********************/
void get_event(omegle *user)
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_req = NULL;
    char post_data[64];
    int sent=0;
    char buf[BUF_SIZE+1];
    int htmlstart =0;
    char *event;

    /* Create the POST request to get stranger id */
    sprintf(post_data, "id=%.64s", user->id);
    post_req = build_post(user->ip, "/events", post_data);

    /* Creating a socket */
    if((sock = create_tcp_socket()) < 0)
    {
	fprintf(stderr,"Child:Error while creating socket\n");
	exit(-1);
    }
    remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (remote == NULL)
    {
	perror("malloc()");
	exit(-1);
    }
    remote->sin_family = AF_INET;
    if((inet_pton(AF_INET, user->ip, (void *)&remote->sin_addr.s_addr)) < 0) {
	perror("inet_pton()");
	exit(-1);
    }
    remote->sin_port = htons(80);
    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0)
    {
	perror("connect()");
	exit(-1);
    }

    /* Sending post request to get event */
    while(sent < (int)strlen(post_req))
    {
	tempres = send(sock, post_req+sent, strlen(post_req)-sent, 0);
	if (tempres == -1)
	{
	    perror("send()");
	    exit(-1);
	}
	sent += tempres;
    }

    /* Recieve data and parse it */
    memset(buf, 0x00, sizeof(buf));
    while ((tempres = recv(sock, buf, BUF_SIZE, 0)) > 0)
    {
	if (htmlstart == 0)
	{
	    event = strstr(buf, "\r\n\r\n");
	    if (event != NULL)
	    {
		htmlstart = 1;
		event += 2;
	    } else {
		event = buf;
	    }
	}

	/* Write results to the shared user->event */
	if (htmlstart)
     	    memcpy(user->event, event, 1024);

	memset(buf, 0, tempres);
    }

    free(remote);
    close(sock);
    return;
}

/*******************/
/* Get stranger id */
/*******************/
void get_id(omegle *user)
{
    struct sockaddr_in *remote;
    int tempres;
    char *post_request;
    char buf[BUF_SIZE+1];
    char *result;

    /* Creating socket */
    if((sock = create_tcp_socket()) < 0)
	exit(-1);

    remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    remote->sin_family = AF_INET;
    if((inet_pton(AF_INET, user->ip, (void *)&remote->sin_addr.s_addr)) < 0) {
	perror("inet_pton()");
	exit(-1);
    }
    remote->sin_port = htons(80);

    if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0) {
	    perror("connect()");
	    exit(-1);
    }

    /* Create the POST request to get stranger id */
    post_request = build_post(user->ip, "/start", "");

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

    user->id = result;		/* user->id shared */
    free(remote);
    free(post_request);
    close(sock); 
    return;
}

/*********************/
/* IP of hostname    */
/* @return ipaddress */
/*********************/
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

/*********************************************/
/* Create a socket fd and return it's number */
/* @return sock fd                           */
/*********************************************/
int create_tcp_socket()
{
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("socket()");
	return -1;
    }
    return sock;
}

/****************************/
/* Build post request	    */
/* @parama host, page, data */
/* @return post request	    */
/****************************/
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

    if(fixedpage[0] == '/')
	fixedpage = page + 1;


    query = (char *)malloc(sizeof(char)*(strlen(host)+strlen(fixedpage)+strlen("Omeglebot")
					 +strlen(command)+5+strlen(data)));
    if(query == NULL) {
	perror("malloc()");
	exit(-1);
    }

    sprintf(query, command, fixedpage, host, strlen(data), data);
    return query;
}
