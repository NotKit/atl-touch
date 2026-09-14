/*
 * bionic libc entry points an app's native code links against and glibc has no
 * equivalent of.
 *
 * These belong in bionic_translation's libc, not here. They live in libandroid
 * because that is the one host library every Android native library already
 * resolves against (bionic_translation maps libandroid.so onto it), and the
 * shim linker looks a symbol up as bionic_<name> across the whole process
 * before anything else.
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

/* bionic: the kernel thread id of a pthread. Only the calling thread's is
 * knowable from outside bionic's own pthread internals. */
pid_t bionic_pthread_gettid_np(pthread_t thread)
{
	static bool logged;

	if (thread == pthread_self())
		return gettid();

	if (!logged) {
		logged = true;
		fprintf(stderr, "pthread_gettid_np: only the calling thread's tid is available "
		                "under bionic_translation\n");
	}
	return -1;
}
