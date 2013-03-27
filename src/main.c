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
#include <stdarg.h>
#include <json/json.h>

#include "main.h"
#include "config.h"
#include "zeromq.h"
#include "superfasthash.h"


/*** DEFINES ***/

#define __STR(a) #a

#define SATAN_IDENTITY "satan"

#define CONF_SUBSCRIBE_ENDPOINT   "satan.subscribe.endpoint"
#define CONF_SUBSCRIBE_HWM        "satan.subscribe.hwm"
#define CONF_SUBSCRIBE_LINGER     "satan.subscribe.linger"
#define CONF_ANSWER_ENDPOINT      "satan.answer.endpoint"
#define CONF_ANSWER_HWM           "satan.answer.hwm"
#define CONF_ANSWER_LINGER        "satan.answer.linger"

#define CONF_PING_INTERVAL        "satan.info.ping_interval"
#define CONF_DEVICE_UUID          "satan.info.thing_uid"

#define MSG_COMMAND_STR_URLFIRM      "URLFIRM"
#define MSG_COMMAND_STR_URLPAK       "URLPAK"
#define MSG_COMMAND_STR_URLFILE      "URLFILE"
#define MSG_COMMAND_STR_URLSCRIPT    "URLSCRIPT"
#define MSG_COMMAND_STR_BINFIRM      "BINFIRM"
#define MSG_COMMAND_STR_BINPAK       "BINPAK"
#define MSG_COMMAND_STR_BINFILE      "BINFILE"
#define MSG_COMMAND_STR_BINSCRIPT    "BINSCRIPT"
#define MSG_COMMAND_STR_UCILINE      "UCILINE"
#define MSG_COMMAND_STR_STATUS       "STATUS"

#define MSG_COMMAND_URLFIRM      0x01
#define MSG_COMMAND_URLPAK       0x02
#define MSG_COMMAND_URLFILE      0x03
#define MSG_COMMAND_URLSCRIPT    0x04
#define MSG_COMMAND_BINFIRM      0x08
#define MSG_COMMAND_BINPAK       0x09
#define MSG_COMMAND_BINFILE      0x0A
#define MSG_COMMAND_BINSCRIPT    0x0B
#define MSG_COMMAND_UCILINE      0x10
#define MSG_COMMAND_STATUS       0x80

#define MSG_ANSWER_STR_ACCEPTED      "MSGACCEPTED"
#define MSG_ANSWER_STR_COMPLETED     "MSGCOMPLETED"
#define MSG_ANSWER_STR_BADCRC        "MSGBADCRC"
#define MSG_ANSWER_STR_BROKENURL     "MSGBROKENURL"
#define MSG_ANSWER_STR_PARSEERROR    "MSGPARSEERROR"
#define MSG_ANSWER_STR_UNREADABLE    "MSGUNREADABLE"
#define MSG_ANSWER_STR_EXECERROR     "MSGEXECERROR"
#define MSG_ANSWER_STR_UCIERROR      "MSGUCIERROR"
#define MSG_ANSWER_STR_UNDEFERROR    "MSGUNDEFERROR"

#define MSG_ANSWER_ACCEPTED      0x01
#define MSG_ANSWER_COMPLETED     0x80
#define MSG_ANSWER_BADCRC        0x02
#define MSG_ANSWER_BROKENURL     0x04
#define MSG_ANSWER_PARSEERROR    0x08
#define MSG_ANSWER_UNREADABLE    0x09 // The message is SO WRONG we cannot even answer.
#define MSG_ANSWER_EXECERROR     0x0C
#define MSG_ANSWER_UCIERROR      0x10
#define MSG_ANSWER_UNDEFERROR    0x20

#define SATAN_CHECKSUM_SIZE 4
#define SATAN_FIRM_OPTIONS_LEN 4
#define SATAN_UUID_LEN 16*2
#define SATAN_MSGID_LEN 16*2  /*  Libuuid's uuid len, converted to string */

typedef struct _satan_args_t {
	void* worker_pipe;
	void *sub_socket;
	char *sub_endpoint;
	char *device_uuid; 
	int sub_hwm;
	int sub_linger;

	void *push_socket;
	char *push_endpoint;
	int req_hwm;
	int req_linger;

  int ping_interval;

	config_context* cfg_ctx;
} satan_args_t;


static void s_help(void)
{
	errorLog("Usage: satan [-u uuid] [-s SUB_ENDPOINT] [-p PUSH_ENDPOINT]\n");
	exit(1);
}

static void s_int_handler(int foo)
{
	zctx_interrupted = true;
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
			case 'u': 
				if(flags+2<argc) {
					flags++;
					args->device_uuid = strndup(argv[1+flags],MAX_STRING_LEN);
				} else {
					errorLog("Error: Please specify a valid uuid !");
				}
				break;
			case 'p': 
				if(flags+2<argc) {
					flags++;
					args->push_endpoint = strndup(argv[1+flags],MAX_STRING_LEN);
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

static int s_execute_child(char* command, int count, ...)
{
	int i = 0;
	int ret = STATUS_ERROR;
	int sig = SIGCHLD;
	char** argv = NULL;
	char *env[] = { NULL };

	if(access(command,F_OK) == -1) return STATUS_ERROR;

	argv = malloc((count+2)*sizeof(char*));
	memset(argv, 0, (count+2)*sizeof(char*));

	va_list _params;
	va_start(_params, count);
	for(i=0; i<count; i++) {
		argv[i+1] = va_arg(_params, char*);
	}
	va_end(_params);

	argv[0] = command;

	debugLog("Command '%s' , arg0 = %s, arg1 = %s, arg2 = %s", 
			command, argv[0], argv[1], argv[2]);

  pid_t process_id = vfork();
  if (!process_id) {
		execve(command,argv,env);
		exit(0);
	}
	if(process_id>0) {
		ret = wait(&sig) == -1 ? STATUS_ERROR : STATUS_OK;
		if(ret == STATUS_OK) {
			ret = WEXITSTATUS(sig) == 0 ? STATUS_OK : STATUS_ERROR;
		}
	}

	return ret;
}

/**
 * \name s_parse_message
 *
 * Parses the server message and checks its correctness.
 * It's only a PROTOCOL verification, not a semantic one.
 *
 * No checks to verify if URLs are not broken links, 
 * files actually exist, etc.
 * 
 * \param message the message to parse. 
 * \param msgid   the message ID that will be extracted from message.
 * \param command the integer-converted command identifier.
 * \param arguments the arguments left to the message
 *
 * \return status one of the MSG_ANSWER_* messages.
 */
int s_parse_message(zmsg_t* message, char** msgid, uint8_t* command, zmsg_t** arguments) 
{
	zmsg_t* duplicate = NULL;
	zmsg_t* _arguments = NULL;
	uint8_t _intcmd;
	uint32_t _computedsum;
	int ret;

	char *_uuid = NULL, *_msgid = NULL, *_command = NULL, *_url = NULL, *_file = NULL, *_exec = NULL;
	zframe_t *_bin = NULL, *_optframe = NULL, *_chksumframe = NULL;

	assert(message);
	assert(msgid);
	assert(command);
	assert(arguments);

	*command = 0;
	*msgid = NULL;
	*arguments = NULL;

	duplicate = zmsg_dup(message); // Work on a copy

	/*  Check that the message is at least 3 times multipart */
	if(zmsg_size(duplicate) < 4) goto s_parse_unreadable;

	/*  Pop arguments one by one, check them */
	_uuid = zmsg_popstr(duplicate);
	if(_uuid == NULL || strlen(_uuid) != SATAN_UUID_LEN) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_uuid,strlen(_uuid), 0);

	_msgid = zmsg_popstr(duplicate); 
	if(_msgid == NULL || strlen(_msgid) != SATAN_MSGID_LEN) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_msgid,strlen(_msgid),_computedsum);

	*msgid = strdup(_msgid);

	_command = zmsg_popstr(duplicate);
	if(_command == NULL) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_command,strlen(_command),_computedsum);

	if(str_equals(_command,MSG_COMMAND_STR_URLFIRM)) {
		_intcmd = MSG_COMMAND_URLFIRM;
	} else if(str_equals(_command,MSG_COMMAND_STR_URLPAK)) {
		_intcmd = MSG_COMMAND_URLPAK;
	} else if(str_equals(_command,MSG_COMMAND_STR_URLFILE)) {
		_intcmd = MSG_COMMAND_URLFILE;
	} else if(str_equals(_command,MSG_COMMAND_STR_URLSCRIPT)) {
		_intcmd = MSG_COMMAND_URLSCRIPT;
	} else if(str_equals(_command,MSG_COMMAND_STR_BINFIRM)) {
		_intcmd = MSG_COMMAND_BINFIRM;
	} else if(str_equals(_command,MSG_COMMAND_STR_BINPAK)) {
		_intcmd = MSG_COMMAND_BINPAK;
	} else if(str_equals(_command,MSG_COMMAND_STR_BINFILE)) {
		_intcmd = MSG_COMMAND_BINFILE;
	} else if(str_equals(_command,MSG_COMMAND_STR_BINSCRIPT)) {
		_intcmd = MSG_COMMAND_BINSCRIPT;
	} else if(str_equals(_command,MSG_COMMAND_STR_UCILINE)) {
		_intcmd = MSG_COMMAND_UCILINE;
	} else if(str_equals(_command,MSG_COMMAND_STR_STATUS)) {
		_intcmd = MSG_COMMAND_STATUS;
	} else {
		goto s_parse_parseerror;
	}

	*command = _intcmd;

	_arguments = zmsg_dup(duplicate); // Save the arguments somewhere

	switch(_intcmd) {
		case MSG_COMMAND_UCILINE: // an UCI line is also a string
		case MSG_COMMAND_URLFIRM:
		case MSG_COMMAND_URLSCRIPT:
		case MSG_COMMAND_URLFILE:
		case MSG_COMMAND_URLPAK: 
			{
				_url = zmsg_popstr(duplicate);
				if(_url == NULL) goto s_parse_parseerror;
				_computedsum = SuperFastHash((uint8_t*)_url,strlen(_url),_computedsum);
			} break;
		case MSG_COMMAND_BINFIRM:
		case MSG_COMMAND_BINSCRIPT:
		case MSG_COMMAND_BINFILE:
		case MSG_COMMAND_BINPAK:
			{
				_bin = zmsg_pop(duplicate);
				if(_bin == NULL) goto s_parse_parseerror;
				_computedsum = SuperFastHash(zframe_data(_bin),zframe_size(_bin),_computedsum);
			} break;
		default:
			break;
	}

	switch(_intcmd) {
		case MSG_COMMAND_BINFILE:
		case MSG_COMMAND_URLFILE:
			{
				if(zmsg_size(duplicate) != 2) goto s_parse_parseerror;
				_file = zmsg_popstr(duplicate);
				if(_file == NULL) goto s_parse_parseerror;
				_computedsum = SuperFastHash((uint8_t*)_file,strlen(_file),_computedsum);
			} break;
		case MSG_COMMAND_URLFIRM:
		case MSG_COMMAND_BINFIRM:
			{
				if(zmsg_size(duplicate) != 2) goto s_parse_parseerror;
				_optframe = zmsg_pop(duplicate);
				if(_optframe == NULL || zframe_size(_optframe) != SATAN_FIRM_OPTIONS_LEN) goto s_parse_parseerror;
				_computedsum = SuperFastHash(zframe_data(_optframe),zframe_size(_optframe),_computedsum);
			} break;
		case MSG_COMMAND_BINPAK:
		case MSG_COMMAND_URLPAK:
		case MSG_COMMAND_UCILINE:
			{
				int _k;
				int _size = zmsg_size(duplicate)-1;
				for(_k=0;_k<_size;_k++) {
					_exec = zmsg_popstr(duplicate);
					if(_exec == NULL) goto s_parse_parseerror;
					_computedsum = SuperFastHash((uint8_t*)_exec,strlen(_exec),_computedsum);
				}
			} break;
		default:
			break;
	}

	if(zmsg_size(duplicate) != 1 || zmsg_content_size(duplicate) != SATAN_CHECKSUM_SIZE)
		goto  s_parse_parseerror;

	/* Verify checksum */
	_chksumframe = zmsg_pop(duplicate);
	uint32_t _chksum = get32bits(zframe_data(_chksumframe));
	if(_chksum != _computedsum) 
		goto s_parse_badcrc;

	/*  Pop off the checksum from arguments before returning */
	zmsg_remove(_arguments, zmsg_last(_arguments));

	*arguments = zmsg_dup(_arguments);
	ret = MSG_ANSWER_ACCEPTED;

s_parse_finish: /*  Free everything */

	if(_uuid) free(_uuid);
  if(_msgid) free(_msgid);
	if(_command) free(_command);
	if(_url) free(_url);
	if(_file) free(_file);
	if(_exec) free(_exec);

	if(_bin) zframe_destroy(&_bin);
	if(_optframe) zframe_destroy(&_optframe);
	if(_chksumframe) zframe_destroy(&_chksumframe);

	if(_arguments) zmsg_destroy(&_arguments);

	zmsg_destroy(&duplicate);
	return ret;

s_parse_badcrc:
	ret = MSG_ANSWER_BADCRC;
	goto s_parse_finish;

s_parse_unreadable:
	ret = MSG_ANSWER_UNREADABLE;
	goto s_parse_finish;

s_parse_parseerror:
	ret = MSG_ANSWER_PARSEERROR;
	goto s_parse_finish;
}


static int s_apply_uciline(satan_args_t* args, zmsg_t* arguments, char** lasterror)
{
	int i, size, ret;
	char *param = NULL;
	char buffer[MAX_STRING_LEN], key[MAX_STRING_LEN], value[MAX_STRING_LEN];
	char pkg[MAX_STRING_LEN], section[MAX_STRING_LEN], prop[MAX_STRING_LEN];
	char path[MAX_STRING_LEN];

	assert(args);
	assert(arguments);
	assert(lasterror);

	*lasterror = NULL;

	param = zmsg_popstr(arguments);
	*lasterror = strdup(param); // Save as the last possible error
	if(param == NULL) 
		goto s_apply_uciline_parseerror;

	/* Check the correctness of the expression */ 
	for(i=0;i<strlen(param);i++) {
		if (param[i] == '=') { param[i] = ' '; break; }
	}

	if (sscanf(param, "%s %s", key, value) != 2) 
		goto s_apply_uciline_parseerror;

	for(i=0;i<strlen(key);i++) {
		if (key[i] == '.') 	key[i] = ' ';
	}
	if (sscanf(key, "%s %s %s", pkg, section, prop) != 3) 
		goto s_apply_uciline_parseerror;

	/*  Apply the configuration  */
	if(snprintf(buffer,MAX_STRING_LEN,"%s.%s.%s=%s",pkg,section,prop,value) < 0)
		goto s_apply_uciline_parseerror;

	if(config_set(args->cfg_ctx,buffer) != STATUS_OK) {
		goto s_apply_uciline_ucierror;
  }
	if(config_commit(args->cfg_ctx,pkg) != STATUS_OK) {
		goto s_apply_uciline_ucierror;
  }

	/*  Execute the eventual daemons */
	size = zmsg_size(arguments);
	for(i=0;i<size;i++) {
		char *daemon = zmsg_popstr(arguments);
		if(daemon == NULL) goto s_apply_uciline_parseerror;
		snprintf(path,MAX_STRING_LEN,"/etc/init.d/%s", daemon);
		free(daemon);

		free(*lasterror);
		*lasterror = strdup(path);

		ret = s_execute_child(path, 1, "stop");
		if(ret != STATUS_OK) goto s_apply_uciline_execerror;
		ret = s_execute_child(path, 1, "start");
		if(ret != STATUS_OK) goto s_apply_uciline_execerror;
	}
			
	if(*lasterror) {
		free(*lasterror);
		*lasterror = NULL;
	}

	ret = MSG_ANSWER_COMPLETED;

s_apply_uciline_end:

	if(param)
		free(param);

	return ret;

s_apply_uciline_execerror:
	ret = MSG_ANSWER_EXECERROR;
	goto s_apply_uciline_end;

s_apply_uciline_ucierror:
	ret = MSG_ANSWER_UCIERROR;
	goto s_apply_uciline_end;

s_apply_uciline_parseerror:
	ret = MSG_ANSWER_PARSEERROR;
	goto s_apply_uciline_end;

}

static int s_process_message(satan_args_t* args, uint8_t command, zmsg_t* arguments, char** lasterror) 
{

	assert(args);
	assert(lasterror);

	int ret = MSG_ANSWER_UNDEFERROR;

	switch(command) {
		case MSG_COMMAND_UCILINE:
				ret = s_apply_uciline(args, arguments,lasterror);
				break;
    case MSG_COMMAND_STATUS:
        // TODO: Implement status commands
        // ret = s_get_status(args, arguments, lasterror);
        break;

			/*  TODO: Implement the rest */
	}

	return ret;
}

static int s_send_answer(satan_args_t* args, int code, char* msgid, char* lasterror) 
{


	switch(code) {
		case MSG_ANSWER_EXECERROR:
			{
				/*  TODO: factor this a little  */
				zmsg_t* answer = zmsg_new();
				zmsg_pushstr(answer, "%s", lasterror);
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_EXECERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", args->device_uuid);
				zmsg_send(&answer, args->push_socket);
			} break;
		case MSG_ANSWER_PARSEERROR:
			{
				zmsg_t* answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_PARSEERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", args->device_uuid);
				zmsg_send(&answer, args->push_socket);
			} break;
		case MSG_ANSWER_UNDEFERROR:
			{
				zmsg_t* answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UNDEFERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", args->device_uuid);
				zmsg_send(&answer, args->push_socket);
			} break;
		case MSG_ANSWER_UCIERROR:
			{
				zmsg_t* answer = zmsg_new();
				zmsg_pushstr(answer, "%s", lasterror);
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UCIERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", args->device_uuid);
				zmsg_send(&answer, args->push_socket);
			} break;
		case MSG_ANSWER_COMPLETED:
			{
				zmsg_t* answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_COMPLETED);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", args->device_uuid);
				zmsg_send(&answer, args->push_socket);
			} break;
		default:
			break;
	}
	
	return STATUS_OK;
}

static void s_worker_loop (void *user_args, zctx_t *ctx, void *pipe)
{
	satan_args_t* args = (satan_args_t*)user_args;

  int ret;
  char* msgid;
  uint8_t command;
  zmsg_t* arguments;

	uint64_t next_ping = -1;

	if(args->ping_interval != -1)
		next_ping = zclock_time() + args->ping_interval * 1000;

	while (!zctx_interrupted) {

		signal(SIGINT, s_int_handler);
		signal(SIGQUIT, s_int_handler);

    if(zclock_time() > next_ping) {
      // s_hermes_send(args->hermes_socket, true);
      next_ping = zclock_time() + args->ping_interval * 1000;
    }

		if (zsocket_poll(pipe, 500)) {  // poll for 500 msecs
      zmsg_t* message = zmsg_recv (pipe);
      if(!message) continue;

      debugLog("Received msg of len: %d bytes", (int)zmsg_content_size(message));

      ret = s_parse_message(message, &msgid, &command, &arguments);
      switch(ret) {
        case MSG_ANSWER_UNREADABLE:
          {
            zmsg_t* answer = zmsg_dup(message);
            zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UNREADABLE);
            zmsg_pushstr(answer, "%032d", 0); /*  send a zeroed msgid */
            zmsg_pushstr(answer, "%s", args->device_uuid);
            zmsg_send(&answer, args->push_socket);
          } break;
        case MSG_ANSWER_PARSEERROR:
        case MSG_ANSWER_BADCRC:
          {
            zmsg_t* answer = zmsg_dup(message);
            if(ret == MSG_ANSWER_BADCRC) 
              zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_BADCRC);
            else
              zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_PARSEERROR);
            zmsg_pushstr(answer, "%s", msgid);
            zmsg_pushstr(answer, "%s", args->device_uuid);
            zmsg_send(&answer, args->push_socket);
          } break;
        case MSG_ANSWER_ACCEPTED:
          {
            char* lasterror = NULL;

            zmsg_t* answer = zmsg_new();
            zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_ACCEPTED);
            zmsg_pushstr(answer, "%s", msgid);
            zmsg_pushstr(answer, "%s", args->device_uuid);
            zmsg_send(&answer, args->push_socket);

            ret = s_process_message(args, command, arguments,&lasterror);
            s_send_answer(args, ret, msgid, lasterror);

          } break;
      }

      if(msgid)
        free(msgid);
      if(arguments) 
        zmsg_destroy(&arguments);
      if(message)
        zmsg_destroy(&message);
    }
  }
	args->worker_pipe = NULL;
}

int main(int argc, char *argv[])
{
	satan_args_t* args = malloc(sizeof(satan_args_t));

	memset(args,0,sizeof(args));

	args->cfg_ctx = config_new();

	config_get_str(args->cfg_ctx,CONF_SUBSCRIBE_ENDPOINT,&args->sub_endpoint);
	config_get_str(args->cfg_ctx,CONF_DEVICE_UUID,&args->device_uuid);

	args->sub_hwm = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_HWM);
	args->sub_linger = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_LINGER);

	config_get_str(args->cfg_ctx,CONF_ANSWER_ENDPOINT,&args->push_endpoint);
	args->req_hwm = config_get_int(args->cfg_ctx, CONF_ANSWER_HWM);
	args->req_linger = config_get_int(args->cfg_ctx, CONF_ANSWER_LINGER);

	args->ping_interval = config_get_int(args->cfg_ctx, CONF_PING_INTERVAL);
	
	s_handle_cmdline(args, argc, argv);

	zctx_t *zmq_ctx = zctx_new ();

	args->sub_socket = zeromq_create_socket(zmq_ctx, args->sub_endpoint, ZMQ_SUB,
			args->device_uuid, true, args->sub_linger, args->sub_hwm);
	assert(args->sub_socket != NULL);

	/*  NOTE: Until now, we acknowledge the messages with a PUSH socket.
	 *  It blocks on a full HWM, meaning "if too many messages in the queue"
	 *  If we use a REQ socket, we will also have various issues while waiting 
	 *  for our answers. Moreover, getting an ACK or not from the server changes
	 *  NOTHING here. 
   *  See in production tests what happens.
   *  */
	args->push_socket = zeromq_create_socket(zmq_ctx, args->push_endpoint, ZMQ_PUSH, 
			NULL, true, args->req_linger, args->req_hwm);
	assert(args->push_socket != NULL);

	/*  Create worker thread  */
	args->worker_pipe = zthread_fork(zmq_ctx, s_worker_loop, args);

	/*  Main listener loop */
	while(!zctx_interrupted) {
		if (zsocket_poll(args->sub_socket, 1000)) { 
			zmsg_t *message = zmsg_recv (args->sub_socket);
			if(message){ 
				if(args->worker_pipe)
					zmsg_send(&message,args->worker_pipe);
				else
					break;
			}
		}
	}

	zsocket_destroy (zmq_ctx, args->sub_socket);
	zsocket_destroy (zmq_ctx, args->push_socket);
	zctx_destroy (&zmq_ctx);
	config_destroy(args->cfg_ctx);

	free(args);

	exit(0);
}
