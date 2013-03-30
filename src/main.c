/**
 * =====================================================================================
 *
 *   @file main.c
 *   @author Victor Perron (), victor@iso3103.net
 *
 *        Version:  1.0
 *        Created:  10/21/2012 07:49:49 PM
 *
 *
 *   @section DESCRIPTION
 *
 *
 *   @section LICENSE
 *
 *       LGPL (http://www.gnu.org/licenses/lgpl.html)
 *
 * =====================================================================================
 */


#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <czmq.h>
#include <stdarg.h>

#include "main.h"
#include "utils.h"
#include "config.h"
#include "messages.h"
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

#define CONF_DEVICE_UUID          "satan.info.uid"


#define SATAN_CHECKSUM_SIZE 4
#define SATAN_PUSH_ARGS_LEN 4
#define MIN_UUID_LEN 4

typedef struct _satan_args_t {
	void *pipe;
	void *inbox;

	void *sub_socket;
	char *sub_endpoint;
	char *device_uuid;
	int sub_hwm;
	int sub_linger;

	void *push_socket;
	char *push_endpoint;
	int req_hwm;
	int req_linger;

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

static void s_handle_cmdline(satan_args_t *args, int argc, char** argv) {
	int flags = 0;

	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
			case 's':
				if (flags+2<argc) {
					flags++;
					args->sub_endpoint = strndup(argv[1+flags],MAX_STRING_LEN);
				} else {
					errorLog("Error: Please specify a valid endpoint !");
				}
				break;
			case 'u':
				if (flags+2<argc) {
					flags++;
					args->device_uuid = strndup(argv[1+flags],MAX_STRING_LEN);
				} else {
					errorLog("Error: Please specify a valid uuid !");
				}
				break;
			case 'p':
				if (flags+2<argc) {
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

int s_parse_message(zmsg_t *message, char** msgid, uint8_t *command, zmsg_t** arguments)
{
	zmsg_t *duplicate = NULL;
	zmsg_t *_arguments = NULL;
	uint8_t _intcmd;
	uint32_t _computedsum;
	int ret;

	char *_uuid = NULL, *_msgid = NULL, *_command = NULL, *_exec = NULL;
	zframe_t *_bin = NULL, *_chksumframe = NULL;

	assert(message);
	assert(msgid);
	assert(command);
	assert(arguments);

	*command = 0;
	*msgid = NULL;
	*arguments = NULL;

	duplicate = zmsg_dup(message); // Work on a copy

	/*  Check that the message is at least 3 times multipart */
	if (zmsg_size(duplicate) < 4) goto s_parse_unreadable;

	/*  Pop arguments one by one, check them */
	_uuid = zmsg_popstr(duplicate);
	if (_uuid == NULL || strlen(_uuid) < MIN_UUID_LEN) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_uuid,strlen(_uuid), 0);

	_msgid = zmsg_popstr(duplicate);
	if (_msgid == NULL || strlen(_msgid) < MIN_UUID_LEN) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_msgid,strlen(_msgid),_computedsum);

	*msgid = strdup(_msgid);

	_command = zmsg_popstr(duplicate);
	if (_command == NULL) goto s_parse_unreadable;
	_computedsum = SuperFastHash((uint8_t*)_command,strlen(_command),_computedsum);

	if (str_equals(_command,MSG_COMMAND_STR_PUSH)) {
		_intcmd = MSG_COMMAND_PUSH;
	} else if (str_equals(_command,MSG_COMMAND_STR_EXEC)) {
		_intcmd = MSG_COMMAND_EXEC;
	} else {
		goto s_parse_parseerror;
	}

	*command = _intcmd;

	_arguments = zmsg_dup(duplicate); // Save the arguments somewhere

	switch (_intcmd) {
		case MSG_COMMAND_EXEC:
			{
				_exec = zmsg_popstr(duplicate);
				if (_exec == NULL) goto s_parse_parseerror;
				_computedsum = SuperFastHash((uint8_t*)_exec,strlen(_exec),_computedsum);
			} break;
		case MSG_COMMAND_PUSH:
			{
				_bin = zmsg_pop(duplicate);
				if (_bin == NULL) goto s_parse_parseerror;
				_computedsum = SuperFastHash(zframe_data(_bin),zframe_size(_bin),_computedsum);
        if (zmsg_size(duplicate) > 1) {
          char *filename = zmsg_popstr(duplicate);
          if (filename == NULL) goto s_parse_parseerror;
          _computedsum = SuperFastHash((uint8_t*)filename,strlen(filename),_computedsum);
          free(filename);
        }
			} break;
		default:
			break;
	}

	if (zmsg_size(duplicate) != 1 || zmsg_content_size(duplicate) != SATAN_CHECKSUM_SIZE)
		goto  s_parse_parseerror;

	/* Verify checksum */
	_chksumframe = zmsg_pop(duplicate);
	uint32_t _chksum = get32bits(zframe_data(_chksumframe));
	if (_chksum != _computedsum)
		goto s_parse_badcrc;

	/*  Pop off the checksum from arguments before returning */
	zmsg_remove(_arguments, zmsg_last(_arguments));
	*arguments = zmsg_dup(_arguments);

	ret = MSG_ANSWER_ACCEPTED;

s_parse_finish: /*  Free everything */

	if (_uuid) free(_uuid);
  if (_msgid) free(_msgid);
	if (_command) free(_command);
	if (_exec) free(_exec);

	if (_bin) zframe_destroy(&_bin);
	if (_chksumframe) zframe_destroy(&_chksumframe);

	if (_arguments) zmsg_destroy(&_arguments);

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

static int s_process_message(satan_args_t *args, char *msgid, uint8_t command, zmsg_t *arguments)
{
	assert(args);
  assert(msgid);

	int ret = MSG_ANSWER_UNDEFERROR;

	switch (command) {
		case MSG_COMMAND_EXEC:
      {
        char *cmd = zmsg_popstr(arguments);
        pid_t pid = messages_exec(args->device_uuid, msgid, args->push_endpoint, cmd);
        if (pid == -1) {
          ret = MSG_ANSWER_EXECERROR;
        } else {
          zmsg_t *msg = zmsg_new();
          zframe_t *frame = zframe_new(&pid, sizeof(pid_t));
          zmsg_push(msg, frame);
          zmsg_pushstr(msg, "%s", cmd);
          zmsg_pushstr(msg, "%s", msgid);
          zmsg_pushstr(msg, "%s", MSG_INTERNAL);
          zmsg_send(&msg, args->pipe);
          ret = MSG_ANSWER_TASK;
        }
      } break;
    case MSG_COMMAND_PUSH:
      ret = messages_push(msgid, arguments);
      break;
	}

	return ret;
}

static void s_check_children_termination(zlist_t *processlist, char *device_id, void *socket)
{
  int status;
  process_item *item = zlist_first(processlist);
  while (item != NULL) {
    // TODO make this act on THREADS
    if (waitpid(item->pid, &status, WNOHANG) != 0) {
      zmsg_t *answer = zmsg_new();
      zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_COMPLETED);
      zmsg_pushstr(answer, "%s", item->message_id);
      zmsg_pushstr(answer, "%s", device_id);
      zmsg_send(&answer, socket);
      free(item->message_id);
      free(item->command);
      zlist_remove(processlist, item);
      free(item);
    }
    item = zlist_next(processlist);
  }
}

static void s_server_message (satan_args_t *args, zmsg_t *message)
{
  /*  Server message, to be processed  */
  uint8_t command;
  int ret = STATUS_ERROR;
  zmsg_t *arguments = NULL;
  char *msgid = NULL;
  zmsg_t *answer = NULL;

  ret = s_parse_message(message, &msgid, &command, &arguments);
  answer = messages_parse_result2msg(args->device_uuid, ret, msgid, message);
  assert(answer != NULL);
  zmsg_send(&answer, args->push_socket);

  if (ret == MSG_ANSWER_ACCEPTED) {
    ret = s_process_message(args, msgid, command, arguments);
    answer = messages_exec_result2msg(args->device_uuid, ret, msgid);
    assert(answer != NULL);
    zmsg_send(&answer, args->push_socket);
  }

  if (msgid)
    free(msgid);
  if (arguments)
    zmsg_destroy(&arguments);
}

static void s_worker_loop (void *user_args, zctx_t *ctx, void *pipe)
{
	satan_args_t *args = (satan_args_t*)user_args;
  zlist_t *process_items = zlist_new();

	while (!zctx_interrupted) {

		signal(SIGINT, s_int_handler);
		signal(SIGQUIT, s_int_handler);

    s_check_children_termination(process_items, args->device_uuid, args->push_socket);

    if (zsocket_poll(pipe, 500)) {
      zmsg_t *message = zmsg_recv (pipe);
      if (!message) continue;

      char *header = zmsg_popstr(message);
      if (str_equals(header, MSG_INTERNAL)) {
        process_item *item = utils_msg2processitem(message);
        zlist_append(process_items, item);
      } else if (str_equals(header, MSG_SERVER)) {
        s_server_message(args, message);
      }

      zmsg_destroy(&message);
    }
  }

  zlist_destroy(&process_items);
	args->pipe = NULL;
}

int main(int argc, char *argv[])
{
	satan_args_t *args = malloc(sizeof(satan_args_t));

	memset(args,0,sizeof(args));

  /*  Get UCI configuration, if any.  */
	config_context *cfg_ctx = config_new();
	config_get_str(cfg_ctx,CONF_SUBSCRIBE_ENDPOINT,&args->sub_endpoint);
	config_get_str(cfg_ctx,CONF_DEVICE_UUID,&args->device_uuid);
	args->sub_hwm = config_get_int(cfg_ctx, CONF_SUBSCRIBE_HWM);
	args->sub_linger = config_get_int(cfg_ctx, CONF_SUBSCRIBE_LINGER);
	config_get_str(cfg_ctx,CONF_ANSWER_ENDPOINT,&args->push_endpoint);
	args->req_hwm = config_get_int(cfg_ctx, CONF_ANSWER_HWM);
	args->req_linger = config_get_int(cfg_ctx, CONF_ANSWER_LINGER);
	config_destroy(cfg_ctx);

  /*  Eventually override it with command line args.  */
	s_handle_cmdline(args, argc, argv);

	/*  ZeroMQ sockets init  */
  zctx_t *zmq_ctx = zctx_new ();
	args->sub_socket = zeromq_create_socket(zmq_ctx, args->sub_endpoint, ZMQ_SUB,
			args->device_uuid, true, args->sub_linger, args->sub_hwm);
	assert(args->sub_socket != NULL);
	args->push_socket = zeromq_create_socket(zmq_ctx, args->push_endpoint, ZMQ_PUSH,
			NULL, true, args->req_linger, args->req_hwm);
	assert(args->push_socket != NULL);

	/*  Create worker thread  */
	args->pipe = zthread_fork(zmq_ctx, s_worker_loop, args);

	/*  Main listener loop */
	while (!zctx_interrupted) {
		if (zsocket_poll(args->sub_socket, 1000)) {
			zmsg_t *message = zmsg_recv (args->sub_socket);
			if (message){
				if (!args->pipe) break;
        zmsg_pushstr(message, MSG_SERVER);
        zmsg_send(&message,args->pipe);
			}
		}
	}

	zsocket_destroy (zmq_ctx, args->sub_socket);
	zsocket_destroy (zmq_ctx, args->push_socket);
	zctx_destroy (&zmq_ctx);

	free(args);

	exit(0);
}
