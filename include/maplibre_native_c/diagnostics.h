/**
 * @file maplibre_native_c/diagnostics.h
 * Public C API declarations for diagnostics.
 */

#ifndef MAPLIBRE_NATIVE_C_DIAGNOSTICS_H
#define MAPLIBRE_NATIVE_C_DIAGNOSTICS_H

// Public C ABI uses C enums.
// NOLINTBEGIN(cppcoreguidelines-use-enum-class)
// Public C ABI structs.
// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
// Public C ABI includes C headers.
// NOLINTBEGIN(modernize-deprecated-headers)
// Public C ABI return syntax.
// NOLINTBEGIN(modernize-use-trailing-return-type)
// Public C ABI typedefs.
// NOLINTBEGIN(modernize-use-using)

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Diagnostics
/**
 * Returns the last thread-local diagnostic message.
 *
 * The returned string is empty when no diagnostic is available. The pointer is
 * owned by the C API and remains valid until the next C API call on the same
 * thread that writes a thread-local diagnostic.
 */
MLN_API const char* mln_thread_last_error_message(void) MLN_NOEXCEPT;

#pragma endregion

#ifdef __cplusplus
}
#endif

// NOLINTEND(modernize-use-using)
// NOLINTEND(modernize-use-trailing-return-type)
// NOLINTEND(modernize-deprecated-headers)
// NOLINTEND(cppcoreguidelines-pro-type-member-init)
// NOLINTEND(cppcoreguidelines-use-enum-class)

#endif  // MAPLIBRE_NATIVE_C_DIAGNOSTICS_H
