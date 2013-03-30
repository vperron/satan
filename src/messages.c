/**
 * =====================================================================================
 *
 *   @file messages.c
 *   @author Victor Perron (), victor@iso3103.net
 *
 *        Version:  1.0
 *        Created:  03/29/2013 03:49:59 PM
 *        Company:  Locarise
 *
 *   @section DESCRIPTION
 *
 *       Message Management source
 *
 *   @section LICENSE
 *
 *       LGPL v2.1
 *
 * =====================================================================================
 */

#include "main.h"
#include "messages.h"
#include "utils.h"

pid_t messages_exec(const char *device_id, const char *msgid, const char *push_endpoint, const char *cmd)
{
	int pid = -1;

	assert(device_id);
  assert(msgid);
	assert(push_endpoint);
  assert(cmd);

  pid = utils_execute_task(cmd, device_id, msgid, push_endpoint);
  if ((pid == 0) || (pid == -1)) return -1;

  return pid;
}

int messages_push(char *msgid, zmsg_t *arguments)
{
	int ret;
  char filename[MAX_STRING_LEN];
	zframe_t *param = NULL;
  uint8_t *data = NULL;
  int size = 0;

  assert(msgid);
	assert(arguments);

  param = zmsg_pop(arguments);
  if (param == NULL) goto s_msg_push_parseerror;
  data = zframe_data(param);
  size = zframe_size(param);

  if (zmsg_size(arguments) > 0) {
    char *file = zmsg_popstr(arguments);
    ret = utils_write_file(file,(char*)data,size);
    free(file);
  } else {
    sprintf(filename, "/tmp/%s", msgid);
    ret = utils_write_file(filename,(char*)data,size);
  }

  if (ret != STATUS_OK) goto s_msg_push_execerror;

	ret = MSG_ANSWER_COMPLETED;

s_msg_push_end:
	if (param)
		free(param);
	return ret;

s_msg_push_execerror:
	ret = MSG_ANSWER_EXECERROR;
	goto s_msg_push_end;

s_msg_push_parseerror:
	ret = MSG_ANSWER_PARSEERROR;
	goto s_msg_push_end;
}

zmsg_t *messages_parse_result2msg(char *device_id, int code, char *msgid, zmsg_t *original)
{
  zmsg_t *answer = NULL;

  switch (code) {
    case MSG_ANSWER_UNREADABLE:
      {
        answer = zmsg_dup(original);
        zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UNREADABLE);
        zmsg_pushstr(answer, "%s", "");
        zmsg_pushstr(answer, "%s", device_id);
      } break;
    case MSG_ANSWER_PARSEERROR:
      {
        answer = zmsg_dup(original);
        zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_PARSEERROR);
        zmsg_pushstr(answer, "%s", msgid);
        zmsg_pushstr(answer, "%s", device_id);
      } break;
    case MSG_ANSWER_BADCRC:
      {
        answer = zmsg_dup(original);
        zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_BADCRC);
        zmsg_pushstr(answer, "%s", msgid);
        zmsg_pushstr(answer, "%s", device_id);
      } break;
    case MSG_ANSWER_ACCEPTED:
      {
        answer = zmsg_new();
        zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_ACCEPTED);
        zmsg_pushstr(answer, "%s", msgid);
        zmsg_pushstr(answer, "%s", device_id);
      } break;
  }

  return answer;
}

zmsg_t *messages_exec_result2msg(char *device_id, int code, char *msgid)
{
  zmsg_t *answer = NULL;

  assert(device_id);
  assert(msgid);

	switch (code) {
		case MSG_ANSWER_EXECERROR:
			{
				answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_EXECERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", device_id);
			} break;
		case MSG_ANSWER_PARSEERROR:
			{
				answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_PARSEERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", device_id);
			} break;
		case MSG_ANSWER_UNDEFERROR:
			{
				answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_UNDEFERROR);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", device_id);
			} break;
		case MSG_ANSWER_TASK:
			{
				answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_TASK);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", device_id);
			} break;
		case MSG_ANSWER_COMPLETED:
			{
				answer = zmsg_new();
				zmsg_pushstr(answer, "%s", MSG_ANSWER_STR_COMPLETED);
				zmsg_pushstr(answer, "%s", msgid);
				zmsg_pushstr(answer, "%s", device_id);
			} break;
		default:
			break;
	}

	return answer;
}

