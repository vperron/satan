/**
 * =====================================================================================
 *
 *   @file main.c
 *   @author Victor Perron (), victor.perron@locarise.com
 *   
 *        Version:  1.0
 *        Created:  10/21/2012 07:49:49 PM
 *        Company:  Locarise
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
#include "config.h"
#include "zeromq.h"


/*** DEFINES ***/

#define SATAN_IDENTITY "satan"

#define CONF_SUBSCRIBE_ENDPOINT   "satan.subscribe.endpoint"
#define CONF_SUBSCRIBE_HWM        "satan.subscribe.hwm"
#define CONF_SUBSCRIBE_LINGER     "satan.subscribe.linger"

#define CONF_REQUEST_ENDPOINT   "satan.request.endpoint"
#define CONF_REQUEST_HWM        "satan.request.hwm"
#define CONF_REQUEST_LINGER     "satan.request.linger"

#define CONF_DEVICE_UUID "device.info.uuid"

#define SATAN_ACK_OK "OK"

#define MSG_COMMAND_URLFIRM      "URLFIRM"
#define MSG_COMMAND_URLPAK       "URLPAK"
#define MSG_COMMAND_URLFILE      "URLFILE"
#define MSG_COMMAND_URLSCRIPT    "URLSCRIPT"
#define MSG_COMMAND_BINFIRM      "BINFIRM"
#define MSG_COMMAND_BINPAK       "BINPAK"
#define MSG_COMMAND_BINFILE      "BINFILE"
#define MSG_COMMAND_BINSCRIPT    "BINSCRIPT"
#define MSG_COMMAND_UCILINE      "UCILINE"
#define MSG_COMMAND_STATUS       "STATUS"

#define SERVER_CMD_URLFIRM      0x01
#define SERVER_CMD_URLPAK       0x02
#define SERVER_CMD_URLFILE      0x03
#define SERVER_CMD_URLSCRIPT    0x04
#define SERVER_CMD_BINFIRM      0x08
#define SERVER_CMD_BINPAK       0x09
#define SERVER_CMD_BINFILE      0x0A
#define SERVER_CMD_BINSCRIPT    0x0B
#define SERVER_CMD_UCILINE      0x10
#define SERVER_CMD_STATUS       0x80

#define MSG_PROCESS_OK            0x01
#define MSG_PROCESS_BADCRC        0x02
#define MSG_PROCESS_BROKENURL     0x04
#define MSG_PROCESS_PARSEERROR    0x08
#define MSG_PROCESS_EXECERROR     0x0C
#define MSG_PROCESS_UCIERROR      0x10
#define MSG_PROCESS_UNDEFERROR    0x20


typedef struct _satan_args_t {
	void *sub_socket;
	char *sub_endpoint;
	char *sub_topic; 
	int sub_hwm;
	int sub_linger;

	void *req_socket;
	char *req_endpoint;
	int req_hwm;
	int req_linger;

	config_context* cfg_ctx;
} satan_args_t;


static void s_help(void)
{
	errorLog("Usage: satan [-s SUB_ENDPOINT] [-r REQ_ENDPOINT]\n");
	exit(1);
}

static void s_handle_cmdline(satan_args_t* args, int argc, char** argv) {
	int flags = 0;

	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
			case 's': 
				if(flags+2<argc) {
					flags++;
					args->sub_endpoint = strndup(argv[1+flags],MAX_STRING_LEN);
				} else {
					errorLog("Error: Please specify a valid endpoint !");
				}
				break;
			case 'r': 
				if(flags+2<argc) {
					flags++;
					args->req_endpoint = strndup(argv[1+flags],MAX_STRING_LEN);
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
	char *argv[] = { NULL, "start", NULL };
	char *env[] = { NULL };
  pid_t process_id = vfork();
  if (!process_id) {
		snprintf(path,MAX_STRING_LEN,"/etc/init.d/%s", pkg);
		argv[0] = path;
		execve(path,argv, env);
		exit(0);
	}
	if(process_id>0) wait(&sig);
}

static void s_stop_daemon(char* pkg)
{
	int sig = SIGCHLD;
	char path[MAX_STRING_LEN];
	char *argv[] = { NULL, "stop", NULL };
	char *env[] = { NULL };
  pid_t process_id = vfork();
  if (!process_id) {
		snprintf(path,MAX_STRING_LEN,"/etc/init.d/%s", pkg);
		argv[0] = path;
		execve(path,argv, env);
		exit(0);
	}
	if(process_id>0) wait(&sig);
}

static void s_apply_config(satan_args_t* args, const char* key, const char* value) 
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

int s_parse_message(zmsg_t* message, char* msgid, u_int8_t* command) 
{
	u_int8_t _intcmd;
	memset(msgid,0,MAX_STRING_LEN);

	assert(message);
	assert(msgid);
	assert(command);

	/*  Check that the message is at least 3 times multipart */
	if(zmsg_size(message) < 3)
		return MSG_PROCESS_PARSEERROR;

	char* _uuid = zmsg_popstr(message);
	if(_uuid == NULL)
		return MSG_PROCESS_PARSEERROR;
	free(_uuid);

	char* _msgid = zmsg_popstr(message);
	if(_msgid == NULL)
		return MSG_PROCESS_PARSEERROR;
	strncpy(msgid,_msgid,MAX_STRING_LEN);
	free(_msgid);

	char* _command = zmsg_popstr(message);
	if(_command == NULL)
		return MSG_PROCESS_PARSEERROR;

	debugLog("Decomposed message into: %s - %s - %s", _uuid, _msgid, _command);

	if(str_equals(_command,MSG_COMMAND_URLFIRM)) {
		_intcmd = SERVER_CMD_URLFIRM;
	} else if(str_equals(_command,MSG_COMMAND_URLPAK)) {
		_intcmd = SERVER_CMD_URLPAK;
	} else if(str_equals(_command,MSG_COMMAND_URLFILE)) {
		_intcmd = SERVER_CMD_URLFILE;
	} else if(str_equals(_command,MSG_COMMAND_URLSCRIPT)) {
		_intcmd = SERVER_CMD_URLSCRIPT;
	} else if(str_equals(_command,MSG_COMMAND_BINFIRM)) {
		_intcmd = SERVER_CMD_BINFIRM;
	} else if(str_equals(_command,MSG_COMMAND_BINPAK)) {
		_intcmd = SERVER_CMD_BINPAK;
	} else if(str_equals(_command,MSG_COMMAND_BINFILE)) {
		_intcmd = SERVER_CMD_BINFILE;
	} else if(str_equals(_command,MSG_COMMAND_BINSCRIPT)) {
		_intcmd = SERVER_CMD_BINSCRIPT;
	} else if(str_equals(_command,MSG_COMMAND_UCILINE)) {
		_intcmd = SERVER_CMD_UCILINE;
	} else if(str_equals(_command,MSG_COMMAND_STATUS)) {
		_intcmd = SERVER_CMD_STATUS;
	} else {
		free(_command);
		return MSG_PROCESS_PARSEERROR;
	}

	free(_command);


	/*  TODO: Ultimately send stuff to background 
	 *  processing when parsing is over for a given value,
	 *  and return a first error message or OK. */
	switch(_intcmd) {
		case SERVER_CMD_URLFIRM:
		case SERVER_CMD_URLSCRIPT:
		case SERVER_CMD_URLFILE:
		case SERVER_CMD_URLPAK: 
			{
				char* _url = zmsg_popstr(message);
				if(_url == NULL)
					return MSG_PROCESS_PARSEERROR;
			} break;

		case SERVER_CMD_BINFIRM:
		case SERVER_CMD_BINSCRIPT:
		case SERVER_CMD_BINFILE:
		case SERVER_CMD_BINPAK:
			/*  TODO: unimplemented */
			break;

	}

	return MSG_PROCESS_OK;
}

int s_process_message(zmsg_t* message) 
{
	/*  
	char key[MAX_STRING_LEN], value[MAX_STRING_LEN];
	if(s_parse_config_val(_msg, key, value)) {
		s_apply_config(args, key, value);
	}
	*/ 
	return STATUS_OK;
}

int main(int argc, char *argv[])
{
	int ret;
	satan_args_t* args = malloc(sizeof(satan_args_t));

	memset(args,0,sizeof(args));

	args->cfg_ctx = config_new();

	config_get_str(args->cfg_ctx,CONF_SUBSCRIBE_ENDPOINT,&args->sub_endpoint);
	config_get_str(args->cfg_ctx,CONF_DEVICE_UUID,&args->sub_topic);
	args->sub_hwm = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_HWM);
	args->sub_linger = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_LINGER);

	config_get_str(args->cfg_ctx,CONF_REQUEST_ENDPOINT,&args->req_endpoint);
	args->req_hwm = config_get_int(args->cfg_ctx, CONF_REQUEST_HWM);
	args->req_linger = config_get_int(args->cfg_ctx, CONF_REQUEST_LINGER);
	
	s_handle_cmdline(args, argc, argv);

	zctx_t *zmq_ctx = zctx_new ();

	args->sub_socket = zeromq_create_socket(zmq_ctx, args->sub_endpoint, ZMQ_SUB,
			args->sub_topic, true, args->sub_linger, args->sub_hwm);
	assert(args->sub_socket != NULL);

	args->req_socket = zeromq_create_socket(zmq_ctx, args->req_endpoint, ZMQ_REQ, 
			NULL, true, args->req_linger, args->req_hwm);
	assert(args->req_socket != NULL);

	/*  TODO: check the REQ socket beforehand */

	/*  Main listener loop */
	while(!zctx_interrupted) {

		char msgid[MAX_STRING_LEN];
		u_int8_t command;

		zmsg_t *message = zmsg_recv (args->sub_socket);

		if(message) { /*  TODO: pay attention to whom this message memory has been allocated to before forking */
			debugLog("Received msg of len: %d bytes", (int)zmsg_content_size(message));
			ret = s_parse_message(message, msgid, &command);
			switch(ret) {
				case STATUS_OK:
					debugLog("Sending REQ message ");
					zstr_send (args->req_socket, SATAN_ACK_OK);
					break;
				default:
					break;
			}

			ret = s_process_message(message);
			switch(ret) {
				case STATUS_OK:
					debugLog("Sending REQ message ");
					zstr_send (args->req_socket, SATAN_ACK_OK);
					break;
				default:
					break;
			}

			zmsg_destroy(&message);
		} 
	}

	zsocket_destroy (zmq_ctx, args->sub_socket);
	zsocket_destroy (zmq_ctx, args->req_socket);
	zctx_destroy (&zmq_ctx);
	config_destroy(args->cfg_ctx);
	free(args);

	exit(0);
}
