/**
 * =====================================================================================
 *
 *   @file zeromq.c
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  12/12/2012 06:17:55 PM
 *        
 *
 *   @section DESCRIPTION
 *
 *       Zeromq helper routines
 *       
 *   @section LICENSE
 *
 *      LGPL http://www.gnu.org/licenses/lgpl.html 
 *
 * =====================================================================================
 */

#include "main.h"
#include "zeromq.h"

void zeromq_send_data(void* socket, char *identity, uint8_t* data, int size) {
	assert(socket);
	assert(data);

	zmsg_t *msg = zmsg_new();

	zmsg_addstr (msg, "%s", identity);
	zframe_t *frame = zframe_new (data, size);
	zmsg_add (msg, frame);

	zmsg_send (&msg, socket);
}

void *zeromq_create_socket (zctx_t *context, char* endpoint, int type, 
		char* topic, bool connect, int linger, int hwm) {

	void* socket = NULL;
	char _endp[MAX_STRING_LEN];

	assert(context);
	assert(endpoint);

	if(endpoint[0] == 0) {
		errorLog("Trying to open NULL output socket !");
		return NULL;
	}

	socket = zsocket_new (context, type);
	if(hwm != -1) {
		zsocket_set_sndhwm (socket, hwm);
		zsocket_set_rcvhwm (socket, hwm);
	}
	
	if(linger != -1) {
		zsocket_set_linger (socket, linger);
	} else { // Set linger to 0 as default, safety measure
		zsocket_set_linger (socket, 0);
	}
	
	if(type == ZMQ_SUB)
		zsocket_set_subscribe (socket, topic == NULL ? "" : topic);

	strncpy(_endp, endpoint, MAX_STRING_LEN);
	if(connect) {
		zsocket_connect (socket, "%s", _endp);
		debugLog("Connected to zmq endpoint : %s",_endp);
	} else {
		zsocket_bind (socket, "%s", _endp);
		debugLog("Bound to zmq endpoint : %s",_endp);
	}
	return socket;
}
