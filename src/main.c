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
#include "config.h"
#include "zeromq.h"


/*** DEFINES ***/

#define SATAN_IDENTITY "satan"

#define CONF_SUBSCRIBE_ENDPOINT   "satan.subscribe.endpoint"
#define CONF_SUBSCRIBE_HWM        "satan.subscribe.hwm"
#define CONF_SUBSCRIBE_LINGER     "satan.subscribe.linger"
#define CONF_LSD_GROUP            "satan.lsd.group"


typedef struct _mainloop_args_t {
	void *socket;
	char *endpoint;
	int hwm;
	int linger;

	config_context* cfg_ctx;
	lsd_handle_t* lsd_handle;
	char* lsdgroup;
} mainloop_args_t;


static void s_help(void)
{
	errorLog("Usage: satan [-e SUB_ENDPOINT]\n");
	exit(1);
}

static void s_handle_cmdline(mainloop_args_t* args, int argc, char** argv) {
	int flags = 0;

	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
			case 'e': 
				if(flags+2<argc) {
					flags++;
					args->endpoint = strndup(argv[1+flags],MAX_STRING_LEN);
				} else {
					errorLog("Error: Please specify a valid endpoint !");
				}
				break;
			case 'h': 
				s_help();
				break;
			default:
				errorLog("Error: Unsupported option '%s' !", argv[1+flags]);
				s_help();
				exit(1);
		}
		flags++;
	}

	if (argc < flags + 1)
		s_help();

}

static void s_lsd_callback (lsd_handle_t* handle,
			int event,
			const char *node,
			const char *group, 
			const uint8_t *msg,
			size_t len,
			void* reserved)
{
	debugLog("Event %d, node %s, len %d", event, node, (int)len);
}

static int s_parse_config_val(const char *str, char *key, char *value)
{
	int i;
	char buf[MAX_STRING_LEN];

	memset(buf,0,MAX_STRING_LEN);

	strncpy(buf,str,MAX_STRING_LEN);
	for(i=0;i<MAX_STRING_LEN;i++) {
		if (buf[i] == '=') {
			buf[i] = ' ';
			break;
		}
	}

	if (sscanf(buf, "%s %s", key, value) == 2) 
		return true;

	return false;
}

static int s_parse_key(const char *key, char *pkg, char *section, char* prop)
{
	int i;
	char buf[MAX_STRING_LEN];
	memset(buf,0,MAX_STRING_LEN);

	strncpy(buf,key,MAX_STRING_LEN);
	for(i=0;i<MAX_STRING_LEN;i++) {
		if (buf[i] == '.') 	buf[i] = ' ';
		if (buf[i] == '\0') break;
	}

	if (sscanf(buf, "%s %s %s", pkg, section, prop) == 3) 
		return true;

	return false;
}

static void s_start_daemon(char* pkg)
{
	int sig = SIGCHLD;
	char path[MAX_STRING_LEN];
  pid_t process_id = fork();
  if (!process_id) {
		snprintf(path,MAX_STRING_LEN,"/etc/init.d/%s", pkg);
		execl(path,path,"start",NULL);
		exit(0);
	}

	if(process_id>0) wait(&sig);
}

static void s_stop_daemon(char* pkg)
{
	int sig = SIGCHLD;
	char path[MAX_STRING_LEN];
  pid_t process_id = fork();
  if (!process_id) {
		snprintf(path,MAX_STRING_LEN,"/etc/init.d/%s", pkg);
		execl(path,path,"stop",NULL);
		exit(0);
	}
	if(process_id>0) wait(&sig);
}

static void s_apply_config(mainloop_args_t* args, const char* key, const char* value) 
{
	char str[MAX_STRING_LEN];
	char pkg[MAX_STRING_LEN], section[MAX_STRING_LEN], prop[MAX_STRING_LEN];
	/*  Determine if it's an UCI config */
	if(s_parse_key(key, pkg, section, prop)) {
		snprintf(str,MAX_STRING_LEN,"%s=%s",key,value);
		config_set(args->cfg_ctx,str);
		config_commit(args->cfg_ctx,pkg);
		s_stop_daemon(pkg);
		s_start_daemon(pkg);
	}
	/*  TODO: package install feature */
}


int main(int argc, char *argv[])
{
	char key[MAX_STRING_LEN], value[MAX_STRING_LEN];
	mainloop_args_t* args = malloc(sizeof(mainloop_args_t));

	memset(args,0,sizeof(args));

	args->cfg_ctx = config_new();

	config_get_str(args->cfg_ctx,CONF_SUBSCRIBE_ENDPOINT,&args->endpoint);
	config_get_str(args->cfg_ctx,CONF_LSD_GROUP,&args->lsdgroup);
	args->hwm = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_HWM);
	args->linger = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_LINGER);
	
	s_handle_cmdline(args, argc, argv);

	args->lsd_handle = lsd_init(s_lsd_callback, NULL);
	assert(args->lsd_handle != NULL);
	lsd_join(args->lsd_handle, args->lsdgroup);

	zctx_t *zmq_ctx = zctx_new ();
	args->socket = zeromq_create_socket(zmq_ctx, args->endpoint, ZMQ_REP, args->linger, args->hwm);
	assert(args->socket != NULL);
	
	/*  Main listener loop */
	while(!zctx_interrupted) {
		char *message = zstr_recv (args->socket);
		debugLog("received '%s' from REQ server.", message);
		zstr_send (args->socket, "ACK");
		if(message) {
			lsd_shout(args->lsd_handle, args->lsdgroup, (const uint8_t*)message, strlen(message));
			if(s_parse_config_val(message, key, value)) {
				s_apply_config(args, key, value);
			}
			free(message);
		}
	}

	zsocket_destroy (zmq_ctx, args->socket);
	lsd_leave(args->lsd_handle, args->lsdgroup);
	lsd_destroy(args->lsd_handle);
	zctx_destroy (&zmq_ctx);
	config_destroy(args->cfg_ctx);
	free(args);

	exit(0);
}
