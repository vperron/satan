/**
 * =====================================================================================
 *
 *   @file main.c
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  10/21/2012 07:49:49 PM
 *        Company:  iso3103 Labs
 *
 *   @section DESCRIPTION
 *
 *       Main forwarder
 *       
 *   @section LICENSE
 *
 *       
 *
 * =====================================================================================
 */


#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zmq.h>

#include "main.h"
#include "lsd.h"
#include "hiredis/hiredis.h"


/*** DEFINES ***/

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_TIMEOUT 2 // secs

#define LSD_CONFIG_GROUP "X-CONFIG"

#define DEFAULT_LINGER 0
#define DEFAULT_HWM 2


/*** GLOBAL VARIABLES ***/
void *zmq_ctx    = NULL; 
void *req_socket = NULL;
void *sub_socket = NULL;

char req_server[MAX_STRING_LEN];  // Config server
char sub_server[MAX_STRING_LEN];    // Configuration file 


/*** STATIC FUNCTIONS ***/
static void help(void) __attribute__ ((noreturn));
static int handle_cmdline(int argc, char** argv);

static void help(void)
{
	errorLog("Usage: config-daemon < -r REQ_SERVER > < -s SUB_SERVER >\n");
	exit(1);
}

static int handle_cmdline(int argc, char** argv) {
	int flags = 0;

	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
			case 'r': 
				if(flags+2<argc) {
					flags++;
					strncpy(req_server,argv[1+flags],MAX_STRING_LEN);
					debugLog("reqserver %s", req_server);
				} else {
					errorLog("Error: Please specify a valid REQ server endpoint !");
				}
				break;
			case 's': 
				if(flags+2<argc) {
					flags++;
					strncpy(sub_server,argv[1+flags],MAX_STRING_LEN);
					debugLog("subserver %s", sub_server);
				} else {
					errorLog("Error: Please specify a valid SUB server endpoint !");
				}
				break;
			case 'h': 
				help();
				break;
			default:
				errorLog("Error: Unsupported option '%s' !", argv[1+flags]);
				help();
				exit(1);
		}
		flags++;
	}

	if (argc < flags + 1)
		help();

	return flags;
}

// From zhelpers.c : receive 0MQ string from socket and convert into C string
static char *s_recv (void *socket) {
    zmq_msg_t message;
    zmq_msg_init (&message);
    int size = zmq_msg_recv (&message, socket, 0);
    if (size == -1)
        return NULL;
    char *string = malloc (size + 1);
    memcpy (string, zmq_msg_data (&message), size);
    zmq_msg_close (&message);
    string [size] = 0;
    return (string);
}


static int s_send (void *socket, char *string) {
    zmq_msg_t message;
    zmq_msg_init_size (&message, strlen (string));
    memcpy (zmq_msg_data (&message), string, strlen (string));
    int size = zmq_msg_send (&message, socket, 0);
    zmq_msg_close (&message);
    return (size);
}


//  ---------------------------------------------------------------------
//  Create socket
static void* zmq_connect_socket(void* context, char* endpoint, int type, int linger, int64_t hwm) {

	void* socket = NULL;
	char* topic = "";
		
	if(endpoint == NULL || endpoint[0] == 0) {
		errorLog("Trying to open NULL output socket !");
		return NULL;
	}

	socket = zmq_socket (context, type);
// 	zmq_setsockopt (socket, ZMQ_LINGER, &linger, sizeof(linger));
// 	zmq_setsockopt (socket, ZMQ_SNDHWM, &hwm, sizeof(hwm));
// 	zmq_setsockopt (socket, ZMQ_RCVHWM, &hwm, sizeof(hwm));
	zmq_connect (socket, endpoint);
	if(type == ZMQ_SUB)
		zmq_setsockopt (socket, ZMQ_SUBSCRIBE, topic, strlen (topic));
	debugLog("Connected to zmq input endpoint : %s",endpoint);
	return socket;
}

//  ---------------------------------------------------------------------
//  Create socket
static void callback (lsd_handle_t* handle,
			int event,
			const char *node,
			const char *group, 
			const uint8_t *msg,
			size_t len,
			void* reserved)
{
	debugLog("Event %d, node %s, len %d", event, node, (int)len);
}



//  ---------------------------------------------------------------------
//  Main loop
int main(int argc, char *argv[])
{

	redisContext *redis_ctx;
	redisReply *redis_reply;
	lsd_handle_t* lsd_handle;

	struct timeval timeout = { REDIS_TIMEOUT , 0 }; // 2 seconds

	memset(req_server,0,sizeof req_server);
	memset(sub_server,0,sizeof sub_server);

	handle_cmdline(argc, argv);

	if(req_server[0] == 0 || sub_server[0] == 0) {
		errorLog("req_server and psub_server parameters are mandatory !");
		help();
		exit(1);
	}


	/*  Init redis connection */
	redis_ctx = redisConnectWithTimeout((char*)REDIS_HOST, REDIS_PORT, timeout);
	if (redis_ctx->err) {
		errorLog("Connection error to %s : %s", REDIS_HOST, redis_ctx->errstr);
		exit(1);
	}

	redis_reply = redisCommand(redis_ctx,"PING");
	debugLog("PING: %s", redis_reply->str);
	freeReplyObject(redis_reply);

	/*  Init lsd  */
	lsd_handle = lsd_init(callback, NULL);
	if(lsd_handle == NULL) {
		errorLog("lsd connection error");
		exit(1);
	}
	lsd_join(lsd_handle, LSD_CONFIG_GROUP);

	/*  Init ZMQ */
	zmq_ctx = zmq_init (1);
	req_socket = zmq_connect_socket(zmq_ctx, req_server, ZMQ_REQ, DEFAULT_LINGER, DEFAULT_HWM);
	if(req_socket == NULL) {
		errorLog("NULL socket opened on %s", req_server);
		zmq_term(zmq_ctx);
		exit(1);
	}
	sub_socket = zmq_connect_socket(zmq_ctx, sub_server, ZMQ_SUB, DEFAULT_LINGER, DEFAULT_HWM);
	if(sub_socket == NULL) {
		errorLog("NULL socket opened on %s", sub_server);
		zmq_close(req_socket);
		zmq_term(zmq_ctx);
		exit(1);
	}

	s_send(req_socket, "PING");
	char *msg = s_recv(req_socket);
	debugLog("REQ socket answer: %s", msg);
	free(msg);


	/*  Main listener loop */
	while(1) {
		char *message = s_recv(sub_socket);
		debugLog("received == %s", message);
		free(message);
	}

	zmq_close(sub_socket);
	zmq_close(req_socket);
	zmq_term(zmq_ctx);

	lsd_destroy(lsd_handle);
	exit(0);
}
