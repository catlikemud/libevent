/*
 * Copyright 2008-2009 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "event-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/resource.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <event.h>
#include <evutil.h>
#include <evhttp.h>
#include <event2/thread.h>

static void http_basic_cb(struct evhttp_request *req, void *arg);

static char *content;
static size_t content_len = 0;

static void
http_basic_cb(struct evhttp_request *req, void *arg)
{
	struct evbuffer *evb = evbuffer_new();

	evbuffer_add(evb, content, content_len);

	/* allow sending of an empty reply */
	evhttp_send_reply(req, HTTP_OK, "Everything is fine", evb);

	evbuffer_free(evb);
}

/* cheasy way of detecting evbuffer_add_reference */
#ifdef _EVENT2_EVENT_H_
static void
http_ref_cb(struct evhttp_request *req, void *arg)
{
	struct evbuffer *evb = evbuffer_new();

	evbuffer_add_reference(evb, content, content_len, NULL, NULL);

	/* allow sending of an empty reply */
	evhttp_send_reply(req, HTTP_OK, "Everything is fine", evb);

	evbuffer_free(evb);
}
#endif

int
main(int argc, char **argv)
{
	struct event_base *base;
	struct evhttp *http;
	int c;
	int use_iocp = 0;

#ifdef WIN32
        WSADATA WSAData;
        WSAStartup(0x101, &WSAData);
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return (1);
#endif

	base = event_base_new();
	unsigned short port = 8080;

	while ((c = getopt(argc, argv, "p:l:i")) != -1) {
		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'l':
			content_len = atol(optarg);
			if (content_len == 0) {
				fprintf(stderr, "Bad content length\n");
				exit(1);
			}
			break;
#ifdef WIN32
		case 'i':
			use_iocp = 1;
			evthread_use_windows_threads();
			event_base_start_iocp(base);
			break;
#endif
		default:
			fprintf(stderr, "Illegal argument \"%c\"\n", c);
			exit(1);
		}
	}

	http = evhttp_new(base);

	content = malloc(content_len);
	if (content == NULL) {
		fprintf(stderr, "Cannot allocate content\n");
		exit(1);
	} else {
		int i = 0;
		for (i = 0; i < content_len; ++i)
			content[i] = (i & 255);
	}

	evhttp_set_cb(http, "/ind", http_basic_cb, NULL);
	fprintf(stderr, "/ind - basic content (memory copy)\n");

#ifdef _EVENT2_EVENT_H_
	evhttp_set_cb(http, "/ref", http_ref_cb, NULL);
	fprintf(stderr, "/ref - basic content (reference)\n");
#endif

	fprintf(stderr, "Serving %d bytes on port %d using %s\n",
	    (int)content_len, port,
	    use_iocp? "IOCP" : event_base_get_method(base));

	evhttp_bind_socket(http, "0.0.0.0", port);

	if (use_iocp) {
		struct timeval tv={99999999,0};
		event_base_loopexit(base, &tv);
	}
	event_base_dispatch(base);

	/* NOTREACHED */
	return (0);
}
