/*
 * Copyright (c) 1998-2001, Robert O'Callahan
 * (C) 2004- TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "x11util.h"

#include <openssl/rand.h>
#include "util.h"
#include <stdlib.h>

typedef struct {
	PTInstVar pvar;
	X11AuthData *auth_data;

	unsigned char *init_buf;
	int init_buf_len;
	int init_data_len;
} X11UnspoofingFilterClosure;

void parse_DISPLAY_str(char *name_buf, int name_buf_len,
                          int *port, int *screen, char *DISPLAY)
{
	strncpy_s(name_buf, name_buf_len, "localhost", _TRUNCATE);
	*port = 6000;
	*screen = 0;

	if (DISPLAY != NULL) {
		int i;

		for (i = 0; DISPLAY[i] != 0 && DISPLAY[i] != ':'; i++) {
		}

		if (i > 0) {
			char c = DISPLAY[i];
			DISPLAY[i] = 0;
			strncpy_s(name_buf, name_buf_len, DISPLAY, _TRUNCATE);
			DISPLAY[i] = c;
		}

		if (DISPLAY[i] == ':') {
			*port = atoi(&DISPLAY[++i]) + 6000;
			while (DISPLAY[i] >= '0' && DISPLAY[i] <= '9') {
				i++;
			}
			if (DISPLAY[i] == '.' && DISPLAY[i+1] != 0) {
				*screen = atoi(&DISPLAY[++i]);
			}
		}
	}
}

void X11_get_DISPLAY_info(PTInstVar pvar, char *name_buf, int name_buf_len,
                          int *port, int *screen)
{
	char *DISPLAY = getenv("DISPLAY");

	if (pvar->settings.X11Display[0] != 0) {
		parse_DISPLAY_str(name_buf, name_buf_len, port, screen, pvar->settings.X11Display);
	}
	else {
		parse_DISPLAY_str(name_buf, name_buf_len, port, screen, DISPLAY);
	}
}


X11AuthData *X11_load_local_auth_data(int screen_num)
{
	X11AuthData *auth_data =
		(X11AuthData *) malloc(sizeof(X11AuthData));
	char *local_auth_data_str;

	auth_data->local_protocol = getenv("TTSSH_XAUTH_PROTOCOL_NAME");

	local_auth_data_str = getenv("TTSSH_XAUTH_PROTOCOL_DATA");
	if (local_auth_data_str == NULL) {
		auth_data->local_data_len = 0;
		auth_data->local_data = NULL;
	} else {
		int str_len = strlen(local_auth_data_str);
		int i;

		auth_data->local_data_len = (str_len + 1) / 2;
		auth_data->local_data = malloc(auth_data->local_data_len);

		if (auth_data->local_data_len * 2 > str_len) {
			char buf[2] = { local_auth_data_str[0], 0 };

			auth_data->local_data[0] =
				(unsigned char) strtol(buf, NULL, 16);
			i = 1;
		} else {
			i = 0;
		}

		for (; i < str_len; i += 2) {
			char buf[3] =
				{ local_auth_data_str[i], local_auth_data_str[i + 1], 0 };

			auth_data->local_data[(i + 1) / 2] =
				(unsigned char) strtol(buf, NULL, 16);
		}
	}

	auth_data->spoofed_protocol = _strdup("MIT-MAGIC-COOKIE-1");
	auth_data->spoofed_data_len = 16;
	auth_data->spoofed_data = malloc(auth_data->spoofed_data_len);
	RAND_bytes(auth_data->spoofed_data, auth_data->spoofed_data_len);

	return auth_data;
}

void X11_dispose_auth_data(X11AuthData *auth_data)
{
	SecureZeroMemory(auth_data->local_data, auth_data->local_data_len);
	free(auth_data->local_data);
	free(auth_data->spoofed_protocol);
	SecureZeroMemory(auth_data->spoofed_data, auth_data->spoofed_data_len);
	free(auth_data->spoofed_data);
	free(auth_data);
}

void *X11_init_unspoofing_filter(PTInstVar pvar,
                                 X11AuthData *auth_data)
{
	X11UnspoofingFilterClosure *closure =
		malloc(sizeof(X11UnspoofingFilterClosure));

	closure->pvar = pvar;
	closure->auth_data = auth_data;

	closure->init_data_len = 0;
	buf_create(&closure->init_buf, &closure->init_buf_len);

	return closure;
}

#define MERGE_NEED_MORE     0
#define MERGE_GOT_GOOD_DATA 1
#define MERGE_GOT_BAD_DATA  2

static int merge_into_X11_init_packet(X11UnspoofingFilterClosure *
                                      closure, int length,
                                      unsigned char *buf)
{
	buf_ensure_size_growing(&closure->init_buf, &closure->init_buf_len,
	                        closure->init_data_len + length);
	memcpy(closure->init_buf + closure->init_data_len, buf, length);
	closure->init_data_len += length;

	if (closure->init_data_len < 12) {
		return MERGE_NEED_MORE;
	} else {
		int name_len;
		int data_len;
		int padded_name_len;
		int padded_data_len;

		switch (closure->init_buf[0]) {
		case 0x42:				/* MSB first */
			name_len = (closure->init_buf[6] << 8) | closure->init_buf[7];
			data_len = (closure->init_buf[8] << 8) | closure->init_buf[9];
			break;
		case 0x6C:				/* LSB first */
			name_len = (closure->init_buf[7] << 8) | closure->init_buf[6];
			data_len = (closure->init_buf[9] << 8) | closure->init_buf[8];
			break;
		default:
			return MERGE_GOT_BAD_DATA;
		}

		padded_name_len = (name_len + 3) & ~0x3;
		padded_data_len = (data_len + 3) & ~0x3;

		if (closure->init_data_len <
		    12 + padded_name_len + padded_data_len) {
			return MERGE_NEED_MORE;
		} else if (name_len ==
		           (int) strlen(closure->auth_data->spoofed_protocol)
		           && memcmp(closure->init_buf + 12,
		                     closure->auth_data->spoofed_protocol,
		                     name_len) == 0
		           && data_len == closure->auth_data->spoofed_data_len
		           && memcmp(closure->init_buf + 12 + padded_name_len,
		                     closure->auth_data->spoofed_data,
		                     data_len) == 0) {
			return MERGE_GOT_GOOD_DATA;
		} else {
			return MERGE_GOT_BAD_DATA;
		}
	}
}

static void insert_real_X11_auth_data(X11UnspoofingFilterClosure *
                                      closure, int *length,
                                      unsigned char **buf)
{
	int name_len = closure->auth_data->local_protocol == NULL
		? 0 : strlen(closure->auth_data->local_protocol);
	int data_len = closure->auth_data->local_data_len;
	int padded_name_len = (name_len + 3) & ~0x3;
	int padded_data_len = (data_len + 3) & ~0x3;

	*length = 12 + padded_name_len + padded_data_len;
	buf_ensure_size(&closure->init_buf, &closure->init_buf_len, *length);
	*buf = closure->init_buf;

	switch (closure->init_buf[0]) {
	case 0x42:					/* MSB first */
		closure->init_buf[6] = name_len >> 8;
		closure->init_buf[7] = name_len & 0xFF;
		closure->init_buf[8] = data_len >> 8;
		closure->init_buf[9] = data_len & 0xFF;
		break;
	case 0x6C:					/* LSB first */
		closure->init_buf[7] = name_len >> 8;
		closure->init_buf[6] = name_len & 0xFF;
		closure->init_buf[9] = data_len >> 8;
		closure->init_buf[8] = data_len & 0xFF;
		break;
	}

	memcpy(*buf + 12, closure->auth_data->local_protocol, name_len);
	memcpy(*buf + 12 + padded_name_len, closure->auth_data->local_data,
	       data_len);
}

FwdFilterResult X11_unspoofing_filter(void *void_closure, FwdFilterEvent event, int *length, unsigned char **buf)
{
	X11UnspoofingFilterClosure *closure =
		(X11UnspoofingFilterClosure *) void_closure;

	switch (event) {
	case FWD_FILTER_CLEANUP:
		buf_destroy(&closure->init_buf, &closure->init_buf_len);
		free(closure);
		return FWD_FILTER_REMOVE;

	case FWD_FILTER_FROM_SERVER:
		switch (merge_into_X11_init_packet(closure, *length, *buf)) {
		case MERGE_NEED_MORE:
			*length = 0;
			return FWD_FILTER_RETAIN;
		case MERGE_GOT_GOOD_DATA:
			insert_real_X11_auth_data(closure, length, buf);
			return FWD_FILTER_REMOVE;
		default:
		case MERGE_GOT_BAD_DATA:
			UTIL_get_lang_msg("MSG_X_AUTH_ERROR", closure->pvar,
			                  "Remote X application sent incorrect authentication data.\n"
			                  "Its X session is being cancelled.");
			notify_nonfatal_error(closure->pvar, closure->pvar->UIMsg);
			*length = 0;
			return FWD_FILTER_CLOSECHANNEL;
		}
		break;
	}

	return FWD_FILTER_RETAIN;
}
