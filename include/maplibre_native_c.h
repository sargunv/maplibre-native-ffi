/**
 * @file maplibre_native_c.h
 * Public C API for low-level MapLibre Native bindings.
 *
 * Functions that operate on thread-affine handles validate the caller thread
 * and return MLN_STATUS_WRONG_THREAD for owner-thread mismatches. Functions
 * without an explicit owner-thread requirement may be called from any thread.
 *
 * Status-returning functions clear thread-local diagnostics on entry. After a
 * synchronous failure status is returned, read
 * mln_thread_last_error_message() on the same thread before making another C
 * API call. Asynchronous native failures are reported through runtime events.
 *
 * This header targets C23.
 */

#ifndef MAPLIBRE_NATIVE_C_H
#define MAPLIBRE_NATIVE_C_H

#include "maplibre_native_c/base.h"            // IWYU pragma: export
#include "maplibre_native_c/camera.h"          // IWYU pragma: export
#include "maplibre_native_c/diagnostics.h"     // IWYU pragma: export
#include "maplibre_native_c/logging.h"         // IWYU pragma: export
#include "maplibre_native_c/map.h"             // IWYU pragma: export
#include "maplibre_native_c/projection.h"      // IWYU pragma: export
#include "maplibre_native_c/query.h"           // IWYU pragma: export
#include "maplibre_native_c/render_session.h"  // IWYU pragma: export
#include "maplibre_native_c/runtime.h"         // IWYU pragma: export
#include "maplibre_native_c/style.h"           // IWYU pragma: export
#include "maplibre_native_c/surface.h"         // IWYU pragma: export
#include "maplibre_native_c/texture.h"         // IWYU pragma: export

#endif  // MAPLIBRE_NATIVE_C_H
