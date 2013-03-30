/**
 * =====================================================================================
 *
 *   @file utils.c
 *   @author Victor Perron (), victor@iso3103.net
 *
 *        Version:  1.0
 *        Created:  03/29/2013 03:51:20 PM
 *        Company:  Locarise
 *
 *   @section DESCRIPTION
 *
 *
 *
 *   @section LICENSE
 *
 *       LGPLv2.1
 *
 * =====================================================================================
 */

#include "main.h"
#include "messages.h"
#include "zeromq.h"
#include "utils.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <czmq.h>
#include <stdarg.h>
#include <unistd.h>

#define LONG_BUFFER_LEN 2000

zmsg_t *utils_gen_msg(const char *device_id, const char *msgid, const char *msg, char *bytes, int len)
{
  zmsg_t *answer = zmsg_new();
  if (bytes != NULL) {
    zframe_t *frame = zframe_new (bytes, len);
    zmsg_push(answer, frame);
  }
  zmsg_pushstr(answer, "%s", msg);
  zmsg_pushstr(answer, "%s", msgid);
  zmsg_pushstr(answer, "%s", device_id);
  return answer;
}

void s_monitored_execution(void *socket, const char *device_id, const char *msgid, const char *cmd)
{
  assert(socket);
  assert(device_id);
  assert(msgid);

  char answer[LONG_BUFFER_LEN];
  memset(answer,0,LONG_BUFFER_LEN);
  unsigned int answer_len = 0;

  char buffer[MAX_STRING_LEN];
  memset(buffer,0,MAX_STRING_LEN);
  unsigned int buffer_len = 0;

  /*  Execute the command and circularly read the output, sending the buffer upstream */
  FILE *stream = popen(cmd, "r");
  while ( fgets(buffer, MAX_STRING_LEN, stream) != NULL ) {
    buffer_len = strlen(buffer);
    if (answer_len + buffer_len >= LONG_BUFFER_LEN) {
      zmsg_t *msg = utils_gen_msg(device_id, msgid,
          MSG_ANSWER_STR_CMDOUTPUT, answer, answer_len);
      zmsg_send(&msg, socket);

      /*  Reset the buffer */
      memset(answer,0,LONG_BUFFER_LEN);
      answer_len = 0;
    }
    strncat(answer, buffer, MAX_STRING_LEN);
    answer_len += buffer_len;
  }
  if (answer_len != 0) {
    zmsg_t *msg = utils_gen_msg(device_id, msgid,
        MSG_ANSWER_STR_CMDOUTPUT, answer, answer_len);
    zmsg_send(&msg, socket);
  }
  pclose(stream);
}

pid_t utils_execute_task(const char *cmd,  const char *device_id,
    const char *msgid, const char *push_endpoint)
{
  assert(cmd);
  assert(msgid);
  assert(device_id);
  assert(push_endpoint);

  pid_t process_id = fork();
  if (!process_id) {
    zctx_t *zmq_ctx = zctx_new ();
    void *socket = zeromq_create_socket(zmq_ctx, push_endpoint, ZMQ_PUSH, NULL, true, -1, -1);
    s_monitored_execution(socket, device_id, msgid, cmd);
    sleep(1); // Give some time to send the latest messages
    zsocket_destroy (zmq_ctx, socket);
    zctx_destroy (&zmq_ctx);
    exit(0);
  }

  return process_id;
}

int utils_write_file(const char *file_name, const char *data, int len)
{
	int written = 0;

	int file = open(file_name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP);
	if (file < 0) {
		return STATUS_ERROR;
	}

	written = write(file,data,len);
	close(file);

	if (written != len) {
		unlink(file_name);
		return STATUS_ERROR;
	}

	return STATUS_OK;
}

process_item *utils_msg2processitem(zmsg_t *message)
{
  process_item *item = malloc(sizeof(process_item));

  char *msgid = zmsg_popstr(message);
  char *command = zmsg_popstr(message);
  zframe_t *frame = zmsg_pop(message);
  pid_t *pid = (pid_t*)frame;
  memcpy(&item->pid,pid,sizeof(pid_t));
  item->message_id = msgid;
  item->command = command;

  return item;
}
