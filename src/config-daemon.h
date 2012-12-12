/**
 * =====================================================================================
 *
 *   @file config-daemon.h
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  12/11/2012 11:40:06 AM
 *        Company:  iso3103 Labs
 *
 *   @section DESCRIPTION
 *
 *       Public defines for config daemon
 *       
 *   @section LICENSE
 *
 *       
 *
 * =====================================================================================
 */


#ifndef _CONFIG_DAEMON_H_
#define _CONFIG_DAEMON_H_

// ------------------------------------------------------
// X-CONFIG group 
//
// In this group, we receive messages formatted as:
//     KEY=VALUE
// where the KEYS are defined below.
// They SHOULD follow a redis-friendly nomenclature. 

#define LSD_GROUP_CONFIG "X-CONFIG"

#define XCONFKEY_DATA_SERVER "xconfig:datastream_server"

#endif 
