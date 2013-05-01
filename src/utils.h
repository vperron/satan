/**
 * =====================================================================================
 *
 *   @file utils.h
 *   @author Victor Perron (), victor@iso3103.net
 *
 *        Version:  1.0
 *        Created:  03/29/2013 03:55:30 PM
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


#ifndef _SATAN_UTILS_H_
#define _SATAN_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

pid_t utils_execute_task(const char *cmd,  const char *device_id,
    const char *msgid, const char *push_endpoint);

int utils_write_file(const char *file_name, const char *data, int len);

process_item *utils_msg2processitem(zmsg_t *message);

#ifdef __cplusplus
}
#endif

#endif // _SATAN_UTILS_H_

