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
 *       LGPL v2.1
 *
 * =====================================================================================
 */



#ifndef _SATAN_CONFIG_H_
#define _SATAN_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uci_context config_context;

config_context* config_new();
void config_destroy(config_context* ctx);
int config_get_str(config_context* ctx, char *key, char** output);
int config_get_int(config_context* ctx, char *key);
bool config_get_bool(config_context* ctx, char *key);
double config_get_double(config_context* ctx, char *key);
int config_set(config_context* ctx, char *key);
int config_commit(config_context* ctx, char *key);

/*  Adaptations for multiple datastreams  */
int config_get_str_ext(config_context* ctx, char *pkg, char *section, char *option, char** output);
int config_get_int_ext(config_context* ctx, char *pkg, char *section, char *option);
bool config_get_bool_ext(config_context* ctx, char *pkg, char *section, char* option);
double config_get_double_ext(config_context* ctx, char* pkg, char *section, char *option);

#ifdef __cplusplus
}
#endif

#endif // _SATAN_CONFIG_H_
