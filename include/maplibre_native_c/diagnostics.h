/**
 * @file maplibre_native_c/diagnostics.h
 * Public C API declarations for diagnostics.
 */

#ifndef MAPLIBRE_NATIVE_C_DIAGNOSTICS_H
#define MAPLIBRE_NATIVE_C_DIAGNOSTICS_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the last thread-local diagnostic message.
 *
 * The returned string is empty when no diagnostic is available. The pointer is
 * owned by the C API and remains valid until the next C API call on the same
 * thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void) MLN_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif  // MAPLIBRE_NATIVE_C_DIAGNOSTICS_H
