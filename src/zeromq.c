/**
 * =====================================================================================
 *
 *   @file zeromq.c
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  12/12/2012 06:17:55 PM
 *        Company:  iso3103 Labs
 *
 *   @section DESCRIPTION
 *
 *       Zeromq routines
 *       
 *   @section LICENSE
 *
 *       
 *
 * =====================================================================================
 */

#include "main.h"
#include "zeromq.h"

void zeromq_send_data(void* socket, char *identity, u_int8_t* data, int size) {
	assert(socket);
	assert(data);

	zmsg_t *msg = zmsg_new();

	zmsg_addstr (msg, "%s", identity);
	zframe_t *frame = zframe_new (data, size);
	zmsg_add (msg, frame);

	zmsg_send (&msg, socket);
}

void *zeromq_create_socket (zctx_t *context, char* endpoint, int type, int linger, int hwm) {

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
