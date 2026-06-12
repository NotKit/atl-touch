#pragma once

/*
 * Include the C-side JNI helpers (util.h / handle_cache.h) from C++.
 * handle_cache.h names struct fields `class`, which is a keyword in C++;
 * textually rename them to `klass` for C++ translation units. The struct
 * layout and linkage are unaffected.
 */

#include <gtk/gtk.h>
#include <jni.h>

#define class klass
extern "C" {
#include "util.h"
}
#undef class
