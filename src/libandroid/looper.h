#ifndef LOOPER_H
#define LOOPER_H
#include <stdbool.h>


enum {
	ALOOPER_EVENT_INPUT = 1 << 0,
	ALOOPER_EVENT_OUTPUT = 1 << 1,
	ALOOPER_EVENT_ERROR = 1 << 2,
	ALOOPER_EVENT_HANGUP = 1 << 3,
	ALOOPER_EVENT_INVALID = 1 << 4
};

typedef void ALooper;
typedef int (*Looper_callbackFunc)(int fd, int events, void *data);

extern ALooper *main_thread_looper;

ALooper *ALooper_prepare(int opts);
void ALooper_wake(ALooper *looper);
bool ALooper_isPolling(ALooper *looper);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_addFd(ALooper *looper, int fd, int ident, int events, Looper_callbackFunc callback, void *data);

#endif
