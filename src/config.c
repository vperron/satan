/**
 * =====================================================================================
 *
 *   @file uci.c
 *   @author Victor Perron (), victor@iso3103.net
 *   
 *        Version:  1.0
 *        Created:  12/19/2012 05:42:27 PM
 *
 *   @section DESCRIPTION
 *
 *       Unified Config Interface helper
 *       
 *   @section LICENSE
 *
 *   This code is borrowed and adapted from uci CLI tool itself 
 *   from nbd (Felix Fietkau)
 *       
 *
 * =====================================================================================
 */


#include <uci.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "config.h"


enum {
	/* section cmds */
	CMD_GET,
	CMD_SET,
	CMD_ADD_LIST,
	CMD_DEL_LIST,
	CMD_DEL,
	CMD_RENAME,
	CMD_REVERT,
	CMD_REORDER,
	/* package cmds: UNSUPPORTED except commit */
	CMD_SHOW,
	CMD_CHANGES,
	CMD_EXPORT,
	CMD_COMMIT,
	/* other cmds: UNSUPPORTED */
	CMD_ADD,
	CMD_IMPORT,
	CMD_HELP,
};

static int uci_cmd(struct uci_context* ctx, int cmd, char *arg, char** output)
{
	struct uci_element *e;
	struct uci_ptr ptr;
	int ret = STATUS_OK;
	int dummy;
	char tmp_arg[MAX_STRING_LEN];
	
	strncpy(tmp_arg,arg,MAX_STRING_LEN);
	ret = uci_lookup_ptr(ctx, &ptr, tmp_arg, true);
	if(ret != UCI_OK)
		return STATUS_ERROR;

	if (ptr.value && 
			(cmd != CMD_SET) && (cmd != CMD_DEL) &&
			(cmd != CMD_ADD_LIST) && (cmd != CMD_DEL_LIST) &&
			(cmd != CMD_RENAME) && (cmd != CMD_REORDER))
		return STATUS_ERROR;

	e = ptr.last;
	switch(cmd) {
		case CMD_GET:
			if (!(ptr.flags & UCI_LOOKUP_COMPLETE)) {
				ctx->err = UCI_ERR_NOTFOUND;
				return STATUS_ERROR;
			}

			if(output) {
				switch(e->type) {
					case UCI_TYPE_SECTION:
						*output = ptr.s->type;
						break;
					case UCI_TYPE_OPTION:
						if(ptr.o->type == UCI_TYPE_STRING)
							*output = ptr.o->v.string;
						break;
					default:
						break;
				}
			}
			break;

		case CMD_RENAME:
			ret = uci_rename(ctx, &ptr);
			break;
		case CMD_REVERT:
			ret = uci_revert(ctx, &ptr);
			break;
		case CMD_SET:
			ret = uci_set(ctx, &ptr);
			break;
		case CMD_ADD_LIST:
			ret = uci_add_list(ctx, &ptr);
			break;
		case CMD_REORDER:
			if (!ptr.s || !ptr.value) {
				ctx->err = UCI_ERR_NOTFOUND;
				return STATUS_ERROR;
			}
			ret = uci_reorder_section(ctx, ptr.s, strtoul(ptr.value, NULL, 10));
			break;
		case CMD_DEL:
			if (ptr.value && !sscanf(ptr.value, "%d", &dummy))
				return STATUS_ERROR;
			ret = uci_delete(ctx, &ptr);
			break;
		case CMD_COMMIT:
			ret = uci_commit(ctx, &ptr.p, true);
			uci_unload(ctx, ptr.p);
			break;
	}

	/* no save necessary for get */
	if ((cmd == CMD_GET) || (cmd == CMD_REVERT) || (cmd == CMD_COMMIT))
		return STATUS_OK;

	/* save changes, but don't commit them yet */
	if (ret == UCI_OK)
		ret = uci_save(ctx, ptr.p);

	if (ret != UCI_OK) {
		return STATUS_ERROR;
	}

	return STATUS_OK;

}

config_context* config_new() 
{
	return uci_alloc_context();
}

void config_destroy(config_context* ctx)
{
	uci_free_context(ctx);
}

int config_get_str(config_context* ctx, char *key, char** output) 
{
	return uci_cmd(ctx, CMD_GET, key, output);
}

int config_get_int(config_context* ctx, char *key) 
{
	int ret;
	char* buf = NULL;
	ret = uci_cmd(ctx, CMD_GET, key, &buf);
	if(ret == STATUS_OK && buf != NULL)
		return atoi(buf);
	return STATUS_ERROR;
}

double config_get_double(config_context* ctx, char *key) 
{
	int ret;
	char* buf = NULL;
	ret = uci_cmd(ctx, CMD_GET, key, &buf);
	if(ret == STATUS_OK && buf != NULL)
		return atof(buf);
	return STATUS_ERROR;
}

bool config_get_bool(config_context* ctx, char *key) 
{
	int ret = config_get_int(ctx, key);
	if(ret != STATUS_ERROR)
		return ret != 0;
	return false; // return false as default
}

int config_set(config_context* ctx, char *key)
{
	return uci_cmd(ctx, CMD_SET, key, NULL);
}

int config_commit(config_context* ctx, char *key) 
{
	return uci_cmd(ctx, CMD_COMMIT, key, NULL);
}

int config_get_str_ext(config_context* ctx, char *pkg, char *section, char *option, char** output)
{
	char key[MAX_STRING_LEN];
	snprintf(key,MAX_STRING_LEN,"%s.%s.%s",pkg,section,option);
	return config_get_str(ctx, key, output);
}

int config_get_int_ext(config_context* ctx, char *pkg, char *section, char *option)
{
	char key[MAX_STRING_LEN];
	snprintf(key,MAX_STRING_LEN,"%s.%s.%s",pkg,section,option);
	return config_get_int(ctx, key);
}

bool config_get_bool_ext(config_context* ctx, char *pkg, char *section, char* option)
{
	char key[MAX_STRING_LEN];
	snprintf(key,MAX_STRING_LEN,"%s.%s.%s",pkg,section,option);
	return config_get_bool(ctx, key);
}

double config_get_double_ext(config_context* ctx, char* pkg, char *section, char *option)
{
	char key[MAX_STRING_LEN];
	snprintf(key,MAX_STRING_LEN,"%s.%s.%s",pkg,section,option);
	return config_get_double(ctx, key);
}

