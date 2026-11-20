#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <glib.h>

#include "looper.h"

ALooper *main_thread_looper;

// dummy strong pointer class
struct sp {
	ALooper *ptr;
	/* the struct has to be larger then 16 bytes, because on aarch64 the
	* calling convention for returning structs larger than 16 bytes is the
	* same as the calling convention for returning large C++ objects */
	char filler[16];
};

/* --- */

struct sp _ZN7android6Looper12getForThreadEv(void);
ALooper *ALooper_forThread(void)
{
	return _ZN7android6Looper12getForThreadEv().ptr;
}

struct sp _ZN7android6Looper7prepareEi(int opts);
ALooper *ALooper_prepare(int opts)
{
	return _ZN7android6Looper7prepareEi(opts).ptr;
}

void _ZNK7android7RefBase9incStrongEPKv(ALooper *this, void *unused);
void ALooper_acquire(ALooper *looper)
{
	_ZNK7android7RefBase9incStrongEPKv(looper, (void *)ALooper_acquire);
}

void _ZNK7android7RefBase9decStrongEPKv(ALooper *this, void *unused);
void ALooper_release(ALooper *looper)
{
	_ZNK7android7RefBase9decStrongEPKv(looper, (void *)ALooper_acquire);
}

int _ZN7android6Looper7pollAllEiPiS1_PPv(ALooper *this, int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_pollAll(int timeoutMillis, int *outFd, int *outEvents, void **outData)
{
	ALooper *looper = ALooper_forThread();
	if (!looper) {
		fprintf(stderr, "ALooper_pollAll: ALooper_forThread returned NULL\n");
		return 0;
	}

	return _ZN7android6Looper7pollAllEiPiS1_PPv(looper, timeoutMillis, outFd, outEvents, outData);
}

int _ZN7android6Looper8pollOnceEiPiS1_PPv(ALooper *this, int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData)
{
	ALooper *looper = ALooper_forThread();
	if (!looper) {
		fprintf(stderr, "ALooper_pollAll: ALooper_forThread returned NULL\n");
		return 0;
	}

	return _ZN7android6Looper8pollOnceEiPiS1_PPv(looper, timeoutMillis, outFd, outEvents, outData);
}

struct fd_source {
	GSource source;
	gpointer tag;
	Looper_callbackFunc callback;
	void *data;
};

static gboolean dispatch_func(GSource *source, GSourceFunc callback, gpointer user_data)
{
	struct fd_source *fd_source = (struct fd_source *)source;
	int fd = GPOINTER_TO_INT(user_data);
	GIOCondition gevents = g_source_query_unix_fd(source, fd_source->tag);
	// glib and Android event mask are almost compatible. Except that glib has an additional
	// priority intput event on the second bit shifting all later bits by one.
	int events = (gevents & G_IO_IN) | ((gevents & ~G_IO_IN) >> 1);
	return fd_source->callback(fd, events, fd_source->data);
}

static GSourceFuncs source_funcs = {
	.dispatch = dispatch_func,
};

int _ZN7android6Looper5addFdEiiiPFiiiPvES1_(ALooper *this, int fd, int ident, int events, Looper_callbackFunc callback, void *data);
int ALooper_addFd(ALooper *looper, int fd, int ident, int events, Looper_callbackFunc callback, void *data)
{
	if (looper == main_thread_looper && callback) {
		g_source_remove_by_funcs_user_data(&source_funcs, GINT_TO_POINTER(fd));
		struct fd_source *source = (struct fd_source *)g_source_new(&source_funcs, sizeof(struct fd_source));
		source->callback = callback;
		source->data = data;
		// glib and Android event mask are almost compatible. Except that glib has an additional
		// priority intput event on the second bit shifting all later bits by one.
		GIOCondition gevents = (events & ALOOPER_EVENT_INPUT) | ((events & ~ALOOPER_EVENT_INPUT) << 1);
		source->tag = g_source_add_unix_fd(&source->source, fd, gevents);
		g_source_set_callback(&source->source, NULL, GINT_TO_POINTER(fd), NULL); // func can be NULL, as we handle everything in the dispatch_func
		g_source_attach(&source->source, NULL);
		g_source_unref(&source->source);
		return 1;
	}
	return _ZN7android6Looper5addFdEiiiPFiiiPvES1_(looper, fd, ident, events, callback, data);
}

void _ZN7android6Looper4wakeEv(ALooper *this);
void ALooper_wake(ALooper *looper)
{
	_ZN7android6Looper4wakeEv(looper);
}

int _ZN7android6Looper8removeFdEi(ALooper *this, int fd);
int ALooper_removeFd(ALooper *looper, int fd)
{
	if (looper == main_thread_looper)
		return g_source_remove_by_funcs_user_data(&source_funcs, GINT_TO_POINTER(fd));
	return _ZN7android6Looper8removeFdEi(looper, fd);
}

/* this is not part of the android API, but we use it internally */

bool _ZNK7android6Looper9isPollingEv(ALooper *this);
bool ALooper_isPolling(ALooper *looper)
{
	return _ZNK7android6Looper9isPollingEv(looper);
}
