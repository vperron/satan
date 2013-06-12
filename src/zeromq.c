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

void zeromq_send_data(void *socket, char *identity, uint8_t *data, int size)
{
  assert(socket);
  assert(data);

  zmsg_t *msg = zmsg_new();

  zmsg_addstr (msg, "%s", identity);
  zframe_t *frame = zframe_new (data, size);
  zmsg_add (msg, frame);

  zmsg_send (&msg, socket);
}

void *zeromq_create_socket (zctx_t *context, const char *endpoint, int type,
    const char *topic, bool connect, int linger, int hwm)
{

  void *socket = NULL;

  assert(context);
  assert(endpoint);

  if (endpoint[0] == 0) {
    errorLog("Trying to open NULL output socket !");
    return NULL;
  }

  socket = zsocket_new (context, type);
  if (hwm != -1) {
    zsocket_set_sndhwm (socket, hwm);
    zsocket_set_rcvhwm (socket, hwm);
  }

  if (linger != -1) {
    zsocket_set_linger (socket, linger);
  } else { // Set linger to 0 as default, safety measure
    zsocket_set_linger (socket, 0);
  }

  // Allow for exponential backoff: up to 5 minutes and 1s minimum
  zsocket_set_reconnect_ivl(socket, 1 * 1000);
  zsocket_set_reconnect_ivl_max(socket, 5 * 60 * 1000);

  if (type == ZMQ_SUB)
    zsocket_set_subscribe (socket, (char*)(topic == NULL ? "" : topic));

  if (connect) {
    zsocket_connect (socket, "%s", endpoint);
  } else {
    zsocket_bind (socket, "%s", endpoint);
  }
  return socket;
}
