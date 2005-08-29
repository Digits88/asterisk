/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * URI encoding / decoding
 * 
 * For now this code only supports 8 bit characters, not unicode,
 * which we ultimately will need to support.
 * 
 * Copyright (C) 2005, Digium, Inc.
 * Created by Olle E. Johansson, Edvina.net 
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "asterisk.h"

/* ASTERISK_FILE_VERSION(__FILE__, "$Revision$") */

#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/logger.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/module.h"

/*--- builtin_function_uriencode: Encode URL according to RFC 2396 */
static char *builtin_function_uriencode(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) 
{
	char uri[BUFSIZ];

	if (!data || ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Syntax: URIENCODE(<data>) - missing argument!\n");
		return NULL;
	}

	ast_uri_encode(data, uri, sizeof(uri), 1);
	ast_copy_string(buf, uri, len);

	return buf;
}

/*--- builtin_function_uridecode: Decode URI according to RFC 2396 */
static char *builtin_function_uridecode(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) 
{
	if (!data || ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Syntax: URIDECODE(<data>) - missing argument!\n");
		return NULL;
	}

	
	ast_copy_string(buf, data, len);
	ast_uri_decode(buf);
	return buf;
}

#ifndef BUILTIN_FUNC
static
#endif
struct ast_custom_function urldecode_function = {
	.name = "URIDECODE",
	.synopsis = "Decodes an URI-encoded string.",
	.syntax = "URIDECODE(<data>)",
	.read = builtin_function_uridecode,
};

#ifndef BUILTIN_FUNC
static
#endif
struct ast_custom_function urlencode_function = {
	.name = "URIENCODE",
	.synopsis = "Encodes a string to URI-safe encoding.",
	.syntax = "URIENCODE(<data>)",
	.read = builtin_function_uriencode,
};

#ifndef BUILTIN_FUNC
static char *tdesc = "URI encode/decode functions";

int unload_module(void)
{
        return ast_custom_function_unregister(&urldecode_function) || ast_custom_function_unregister(&urlencode_function);
}

int load_module(void)
{
        return ast_custom_function_register(&urldecode_function) || ast_custom_function_register(&urlencode_function);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	return 0;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
#endif /* BUILTIN_FUNC */
