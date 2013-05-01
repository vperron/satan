/**
 * =====================================================================================
 *
 *   @file uci.h
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  12/19/2012 06:04:08 PM
 *
 *   @section DESCRIPTION
 *
 *       
 *       
 *   @section LICENSE
 *
 *       
 *
 * =====================================================================================
 */

#include "main.h"

#ifndef _SNOW_CONFIG_H_
#define _SNOW_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uci_context config_context;

config_context* config_new();
void config_destroy(config_context* ctx);
char *config_get_str(config_context* ctx, char *key);
int config_get_int(config_context* ctx, char *key);
bool config_get_bool(config_context* ctx, char *key);
double config_get_double(config_context* ctx, char *key);
int config_set(config_context* ctx, char *key);
int config_commit(config_context* ctx, char *key);

#ifdef __cplusplus
}
#endif

#endif
