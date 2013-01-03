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

#define __STR(a) #a

#define SATAN_IDENTITY "satan"
#define CONF_SUBSCRIBE_ENDPOINT   "satan.subscribe.endpoint"
#define CONF_SUBSCRIBE_HWM        "satan.subscribe.hwm"
#define CONF_SUBSCRIBE_LINGER     "satan.subscribe.linger"
#define CONF_ANSWER_ENDPOINT   "satan.answer.endpoint"
#define CONF_ANSWER_HWM        "satan.answer.hwm"
#define CONF_ANSWER_LINGER     "satan.answer.linger"
#define CONF_DEVICE_UUID "device.info.uuid"

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
	void *sub_socket;
	char *sub_endpoint;
	char *device_uuid; 
	int sub_hwm;
	int sub_linger;

	void *push_socket;
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

/**
 * SuperFastHash : courtesy of Paul Hsieh,
 * http://www.azillionmonkeys.com/qed/hash.html
 * LGPLv2.1 code.
 */
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const u_int16_t *) (d)))
#define get32bits(d) (*((const u_int32_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((u_int32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(u_int32_t)(((const uint8_t *)(d))[0]) )
#define get32bits(d) ((((u_int32_t)(((const uint8_t *)(d))[3])) << 24)\
											+(((u_int32_t)(((const uint8_t *)(d))[2])) << 16)\
											+(((u_int32_t)(((const uint8_t *)(d))[1])) << 8)\
                      +(u_int32_t)(((const uint8_t *)(d))[0]) )
#endif

static u_int32_t s_hash32_fast (u_char* data, int len, u_int32_t hash) {
	u_int32_t tmp;
	int rem;

	if (len <= 0 || data == NULL) return 0;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (;len > 0; len--) {
		hash  += get16bits (data);
		tmp    = (get16bits (data+2) << 11) ^ hash;
		hash   = (hash << 16) ^ tmp;
		data  += 2*sizeof (u_int16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */
	switch (rem) {
		case 3: hash += get16bits (data);
						hash ^= hash << 16;
						hash ^= ((signed char)data[sizeof (u_int16_t)]) << 18;
						hash += hash >> 11;
						break;
		case 2: hash += get16bits (data);
						hash ^= hash << 11;
						hash += hash >> 17;
						break;
		case 1: hash += (signed char)*data;
						hash ^= hash << 10;
						hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
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
int s_parse_message(zmsg_t* message, char** msgid, u_int8_t* command, zmsg_t** arguments) 
{
	zmsg_t* duplicate, *_arguments;
	u_int8_t _intcmd;
	u_int32_t _computedsum;
	int ret;

	char *_uuid = 0, *_msgid = 0, *_command = 0, *_url = 0, *_file = 0, *_exec = 0;
	zframe_t *_bin = 0, *_optframe = 0, *_chksumframe = 0;

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
	_computedsum = s_hash32_fast((u_char*)_uuid,strlen(_uuid), 0);

	_msgid = zmsg_popstr(duplicate); 
	if(_msgid == NULL || strlen(_msgid) != SATAN_MSGID_LEN) goto s_parse_unreadable;
	_computedsum = s_hash32_fast((u_char*)_msgid,strlen(_msgid),_computedsum);

	*msgid = strdup(_msgid);

	_command = zmsg_popstr(duplicate);
	if(_command == NULL) goto s_parse_unreadable;
	_computedsum = s_hash32_fast((u_char*)_command,strlen(_command),_computedsum);

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
				_computedsum = s_hash32_fast((u_char*)_url,strlen(_url),_computedsum);
			} break;
		case MSG_COMMAND_BINFIRM:
		case MSG_COMMAND_BINSCRIPT:
		case MSG_COMMAND_BINFILE:
		case MSG_COMMAND_BINPAK:
			{
				_bin = zmsg_pop(duplicate);
				if(_bin == NULL) goto s_parse_parseerror;
				_computedsum = s_hash32_fast(zframe_data(_bin),zframe_size(_bin),_computedsum);
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
				_computedsum = s_hash32_fast((u_char*)_file,strlen(_file),_computedsum);
			} break;
		case MSG_COMMAND_URLFIRM:
		case MSG_COMMAND_BINFIRM:
			{
				if(zmsg_size(duplicate) != 2) goto s_parse_parseerror;
				_optframe = zmsg_pop(duplicate);
				if(_optframe == NULL || zframe_size(_optframe) == SATAN_FIRM_OPTIONS_LEN) goto s_parse_parseerror;
				_computedsum = s_hash32_fast(zframe_data(_optframe),zframe_size(_optframe),_computedsum);
			} break;
		case MSG_COMMAND_BINPAK:
		case MSG_COMMAND_URLPAK:
			{
				int _k;
				int _size = zmsg_size(duplicate)-1;
				for(_k=0;_k<_size;_k++) {
					_exec = zmsg_popstr(duplicate);
					if(_exec == NULL) goto s_parse_parseerror;
					_computedsum = s_hash32_fast((u_char*)_exec,strlen(_exec),_computedsum);
				}
			} break;
		default:
			break;
	}

	if(zmsg_size(duplicate) != 1 || zmsg_content_size(duplicate) != SATAN_CHECKSUM_SIZE)
		goto  s_parse_parseerror;

	/* Verify checksum */
	_chksumframe = zmsg_pop(duplicate);
	u_int32_t _chksum = get32bits(zframe_data(_chksumframe));
	debugLog("Sent checksum = %08X VS computed = %08X", _chksum, _computedsum);
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

int s_process_message(zmsg_t* message) 
{
	/*  TODO: Implement actual fucntionality */
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
	config_get_str(args->cfg_ctx,CONF_DEVICE_UUID,&args->device_uuid);
	args->sub_hwm = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_HWM);
	args->sub_linger = config_get_int(args->cfg_ctx, CONF_SUBSCRIBE_LINGER);

	config_get_str(args->cfg_ctx,CONF_ANSWER_ENDPOINT,&args->req_endpoint);
	args->req_hwm = config_get_int(args->cfg_ctx, CONF_ANSWER_HWM);
	args->req_linger = config_get_int(args->cfg_ctx, CONF_ANSWER_LINGER);
	
	s_handle_cmdline(args, argc, argv);

	zctx_t *zmq_ctx = zctx_new ();

	args->sub_socket = zeromq_create_socket(zmq_ctx, args->sub_endpoint, ZMQ_SUB,
			args->device_uuid, true, args->sub_linger, args->sub_hwm);
	assert(args->sub_socket != NULL);

	/*  TODO: Until now, we acknowledge the messages with a PUSH socket.
	 *  It blocks on a full HWM, meaning "if too many messages in the queue"
	 *  If we use a REQ socket, we will also have various issues while waiting 
	 *  for our answers. Moreover, getting an ACK or not from the server changes
	 *  NOTHING here. */
	args->push_socket = zeromq_create_socket(zmq_ctx, args->req_endpoint, ZMQ_PUSH, 
			NULL, true, args->req_linger, args->req_hwm);
	assert(args->push_socket != NULL);

	/*  Main listener loop */
	while(!zctx_interrupted) {

		char* msgid;
		u_int8_t command;
		zmsg_t* arguments;

		zmsg_t *message = zmsg_recv (args->sub_socket);

		if(message) {
			debugLog("Received msg of len: %d bytes", (int)zmsg_content_size(message));

			ret = s_parse_message(message, &msgid, &command, &arguments);
			debugLog("Parse operation returned : %d with %s, %d ", 
					ret, msgid, command);
			switch(ret) {
				case MSG_ANSWER_UNREADABLE:
					{
						zmsg_t* answer = zmsg_dup(message);
						zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UNREADABLE);
						zmsg_pushstr(answer, "%0" __STR(SATAN_MSGID_LEN) "d", 0); /*  send a zeroed msgid */
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
						zmsg_t* answer = zmsg_new();
						zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_ACCEPTED);
						zmsg_pushstr(answer, "%s", msgid);
						zmsg_pushstr(answer, "%s", args->device_uuid);
						zmsg_send(&answer, args->push_socket);
					} break;
			}
		
			ret = s_process_message(message);

			if(msgid)
				free(msgid);
			if(arguments) 
				zmsg_destroy(&arguments);
			if(message)
				zmsg_destroy(&message);
		} 
	}

	zsocket_destroy (zmq_ctx, args->sub_socket);
	zsocket_destroy (zmq_ctx, args->push_socket);
	zctx_destroy (&zmq_ctx);
	config_destroy(args->cfg_ctx);
	free(args);

	exit(0);
}
