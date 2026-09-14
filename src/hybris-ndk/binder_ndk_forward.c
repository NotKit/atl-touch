/*
 * libbinder_ndk.so for an app's own native code, forwarded to the device's
 * Android library through libhybris. See hybris_ndk.h.
 *
 * Google Camera's libgcastartup.so imports only the AStatus_* accessors: the
 * status objects come back from other Android libraries (camera2 NDK errors,
 * vendor services) and the app reads them. The process-wide calls are here too
 * because the thread pool is shared state - ATL's camera2ndk backend needs it
 * running before a camera is opened, and it must be started once, not per
 * caller.
 *
 * The binder NDK headers are not vendored, so the handful of prototypes used
 * here are spelled out; AStatus is opaque on both sides.
 */

#include <stdbool.h>
#include <stdint.h>

#include "hybris_ndk.h"

#define SONAME "libbinder_ndk.so"

typedef struct AStatus AStatus;
typedef int32_t binder_exception_t;
typedef int32_t binder_status_t;

/* the exception code for "the transaction never happened" */
#define EX_TRANSACTION_FAILED (-129)

ATL_HYBRIS_FORWARD(SONAME, bool, AStatus_isOk, (const AStatus *status), (status), false)

ATL_HYBRIS_FORWARD(SONAME, binder_exception_t, AStatus_getExceptionCode,
                   (const AStatus *status), (status), EX_TRANSACTION_FAILED)

ATL_HYBRIS_FORWARD(SONAME, int32_t, AStatus_getServiceSpecificError,
                   (const AStatus *status), (status), 0)

ATL_HYBRIS_FORWARD(SONAME, const char *, AStatus_getMessage, (const AStatus *status), (status), "")

ATL_HYBRIS_FORWARD(SONAME, const char *, AStatus_getDescription,
                   (const AStatus *status), (status), "")

ATL_HYBRIS_FORWARD_VOID(SONAME, AStatus_deleteDescription,
                        (const AStatus *status, const char *description), (status, description))

ATL_HYBRIS_FORWARD(SONAME, bool, ABinderProcess_isThreadPoolStarted, (void), (), false)

ATL_HYBRIS_FORWARD_VOID(SONAME, ABinderProcess_setThreadPoolMaxThreadCount,
                        (uint32_t numThreads), (numThreads))

/* one pool for the app and for ATL's own backend, started at most once */
void ABinderProcess_startThreadPool(void)
{
	atl_hybris_ndk_binder_pool();
}
