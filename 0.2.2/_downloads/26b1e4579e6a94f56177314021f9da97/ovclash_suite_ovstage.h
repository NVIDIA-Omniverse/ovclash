// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ovclash/ovclash_ovstage.h"
#include "ovclash_suite/ovclash_suite.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Runs a synchronous owner operation under the native ovstage lifecycle gate.
 *
 * Only the ovstage build of the library exports this function, like every function of this
 * header. Nesting and lock ordering are those of ovclash_ovstage_run_owner_operation. No runtime
 * or Stage is created or retained. Inside the callback, every Suite function that returns
 * ovclash_suite_result_t fails with OVCLASH_SUITE_STATUS_ERROR. This includes data-store, model,
 * export and utility helpers that touch no runtime. Only nested
 * ovclash_suite_run_ovstage_owner_operation calls and the last-error getters are allowed.
 *
 * \param operation Caller-owned callback. Must not be null.
 * \param user_data Borrowed callback state. May be null.
 * \return Success when the callback returns 1. Error for an invalid callback, excessive nesting,
 * callback failure or a native exception.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_run_ovstage_owner_operation(ovclash_ovstage_owner_operation_fn operation, void* user_data);

/**
 * Borrows an existing ovstage Stage for the runtime.
 *
 * Ownership and evaluation follow ovclash_ovstage_attach_stage. Active async and fetch handles
 * are rejected, including completed handles that are not yet released. A successful attachment
 * clears the scope set by ovclash_suite_set_scope. Replacing a Stage also clears results and
 * pipelines. A non-empty scope turns the full-scene selection of the engine off. Like
 * ovclash_suite_open_stage, a successful attachment turns it back on when the run settings in
 * effect enable enable_full_scene. The settings in effect are those last applied with
 * ovclash_suite_set_run_settings or, before any successful apply, the engine defaults, which
 * enable it. A failed attachment keeps the previous binding.
 *
 * The runtime is thread-compatible: serialize calls on one runtime, and do not reenter it from
 * the owner evaluation callback.
 *
 * \param runtime An initialized Suite runtime using the ovstage backend. Must not be null.
 * \param binding Caller-owned Stage, metadata and optional callback.
 * \return Success, or error for invalid arguments, active work or attachment failure.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_attach_ovstage(ovclash_suite_runtime_t* runtime,
                                                                const ovclash_ovstage_binding_t* binding);

/**
 * Releases the borrowed Stage without destroying it.
 *
 * Results and pipelines are invalidated. Active async and fetch handles are rejected, including
 * completed handles that are not yet released. Serialize calls on one runtime.
 *
 * \param runtime An initialized Suite runtime using the ovstage backend. Must not be null.
 * \return Success, or error for invalid arguments or active work.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_detach_ovstage(ovclash_suite_runtime_t* runtime);
#ifdef __cplusplus
}
#endif
