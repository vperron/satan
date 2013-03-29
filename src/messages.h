/**
 * =====================================================================================
 *
 *   @file messages.h
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  03/29/2013 03:52:49 PM
 *        Company:  Locarise
 *
 *   @section DESCRIPTION
 *
 *       Message-related routine defintions
 *       
 *   @section LICENSE
 *
 *       LGPLv2.1
 *
 * =====================================================================================
 */

#include <czmq.h>

#ifndef _SATAN_MESSAGE_H_
#define _SATAN_MESSAGE_H_


#define MSG_COMMAND_STR_EXEC          "EXEC"
#define MSG_COMMAND_STR_PUSH          "PUSH"

#define MSG_COMMAND_EXEC              0x01
#define MSG_COMMAND_PUSH              0x02

#define MSG_ANSWER_STR_ACCEPTED      "MSGACCEPTED"
#define MSG_ANSWER_STR_COMPLETED     "MSGCOMPLETED"
#define MSG_ANSWER_STR_BADCRC        "MSGBADCRC"
#define MSG_ANSWER_STR_PARSEERROR    "MSGPARSEERROR"
#define MSG_ANSWER_STR_UNREADABLE    "MSGUNREADABLE"
#define MSG_ANSWER_STR_EXECERROR     "MSGEXECERROR"
#define MSG_ANSWER_STR_UNDEFERROR    "MSGUNDEFERROR"
#define MSG_ANSWER_STR_CMDOUTPUT     "MSGCMDOUTPUT"
#define MSG_ANSWER_STR_TASK          "MSGTASK"

// Internal use messages
#define MSG_SERVER                   "MSGSERVER"
#define MSG_INTERNAL                 "MSGINTERNAL"

#define MSG_ANSWER_ACCEPTED          0x01
#define MSG_ANSWER_CMDOUTPUT         0x40
#define MSG_ANSWER_COMPLETED         0x80
#define MSG_ANSWER_BADCRC            0x02
#define MSG_ANSWER_PARSEERROR        0x08
#define MSG_ANSWER_UNREADABLE        0x09 // The message is SO WRONG we cannot even answer.
#define MSG_ANSWER_EXECERROR         0x0C
#define MSG_ANSWER_UNDEFERROR        0x20
#define MSG_ANSWER_TASK              0xC0

pid_t messages_exec(const char* device_id, const char* msgid, const char* push_endpoint, const char* cmd);
int messages_push(char* msgid, zmsg_t* arguments);

zmsg_t *messages_parse_result2msg(char* device_id, int code, char* msgid, zmsg_t* original);
zmsg_t *messages_exec_result2msg(char* device_id, int code, char* msgid);


#endif // _SATAN_MESSAGE_H_
