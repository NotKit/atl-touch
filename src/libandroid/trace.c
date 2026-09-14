/*
 * android/trace.h: systrace. ATL has no trace channel to write to, so a section
 * is accounted for and dropped. ATL_TRACE=1 prints them instead, which is the
 * only way to see where a native library with no logging of its own spends its
 * startup.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool trace_printing(void)
{
	static int enabled = -1;

	if (enabled < 0) {
		const char *env = getenv("ATL_TRACE");

		enabled = env && *env && strcmp(env, "0") ? 1 : 0;
	}
	return enabled == 1;
}

bool ATrace_isEnabled()
{
	return false;
}

void ATrace_beginSection(const char *sectionName)
{
	if (trace_printing())
		fprintf(stderr, "atl-trace: begin %s\n", sectionName);
}

void ATrace_endSection(void)
{
	if (trace_printing())
		fprintf(stderr, "atl-trace: end\n");
}

void ATrace_beginAsyncSection(const char *sectionName, int32_t cookie)
{
	if (trace_printing())
		fprintf(stderr, "atl-trace: begin async %s (%d)\n", sectionName, cookie);
}

void ATrace_endAsyncSection(const char *sectionName, int32_t cookie)
{
	if (trace_printing())
		fprintf(stderr, "atl-trace: end async %s (%d)\n", sectionName, cookie);
}

void ATrace_setCounter(const char *counterName, int64_t counterValue)
{
}
