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
#include <czmq.h>

#include "main.h"
#include "lsd.h"
#include "hiredis/hiredis.h"
#include "config-daemon.h"


/*** DEFINES ***/

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_TIMEOUT 2 // secs


#define DEFAULT_LINGER 0
#define DEFAULT_HWM 2


/*** GLOBAL VARIABLES ***/
zctx_t *zmq_ctx    = NULL; 
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


//  ---------------------------------------------------------------------
//  Create socket
static void *zmq_socket_create (zctx_t *context, char* endpoint, int type, int linger, int hwm) {

	void* socket = NULL;

	assert(context);
	assert(endpoint);
		
	if(endpoint[0] == 0) {
		errorLog("Trying to open NULL output socket !");
		return NULL;
	}

	socket = zsocket_new (context, type);
	zsocket_set_sndhwm (socket, hwm);
	zsocket_set_rcvhwm (socket, hwm);
	zsocket_set_linger (socket, linger);
	if(type == ZMQ_SUB)
		zsocket_set_subscribe (socket, "");
	zsocket_connect (socket, "%s", endpoint);
	debugLog("Connected to zmq endpoint : %s",endpoint);
	return socket;
}

//  ---------------------------------------------------------------------
//  Callback for LSD
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
	zmq_ctx = zctx_new ();
	req_socket = zmq_socket_create(zmq_ctx, req_server, ZMQ_REQ, DEFAULT_LINGER, DEFAULT_HWM);
	if(req_socket == NULL) {
		errorLog("NULL socket opened on %s", req_server);
		zctx_destroy(&zmq_ctx);
		exit(1);
	}
	sub_socket = zmq_socket_create(zmq_ctx, sub_server, ZMQ_SUB, DEFAULT_LINGER, DEFAULT_HWM);
	if(sub_socket == NULL) {
		errorLog("NULL socket opened on %s", sub_server);
		zsocket_destroy(zmq_ctx, req_socket);
		zctx_destroy(&zmq_ctx);
		exit(1);
	}

	zstr_send (req_socket, "PING");
	char *msg = zstr_recv (req_socket);
	debugLog("REQ socket answer: %s", msg);
	free(msg);


	/*  Main listener loop */
	while(!zctx_interrupted) {
		char *message = zstr_recv (sub_socket);
		debugLog("received == %s", message);
		if(message)
			free(message);
	}

	zsocket_destroy (zmq_ctx, sub_socket);
	zsocket_destroy (zmq_ctx, req_socket);
	zctx_destroy (&zmq_ctx);

	lsd_destroy(lsd_handle);

	exit(0);
}
