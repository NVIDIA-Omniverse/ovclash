// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ovclash/ovclash.h"

typedef struct ovstage_instance_t ovstage_instance_t;

/**
 * Performs one synchronous caller-owned ovstage operation.
 *
 * The callback must not throw or enter any clash runtime. Nested owner-operation calls are
 * allowed.
 *
 * @param user_data [in] Caller-owned operation state, used only during the call.
 * @return 1 on success, or any other value on failure.
 */
typedef int32_t (*ovclash_ovstage_owner_operation_fn)(void* user_data);

/**
 * Evaluates the borrowed stage at a time in seconds, where NaN means Default.
 *
 * Never called for an attachment with a composition.
 *
 * On success the callback returns 1 with a newly committed ordinal. On failure it returns any
 * other value, conventionally 0, and may set a UTF-8 diagnostic. The diagnostic must remain valid
 * until the enclosing ovclash call returns. The callback runs on the calling pipeline thread,
 * which may be a native worker. It must not throw or enter any clash runtime.
 *
 * Before it returns success, the callback must publish the complete sampled canonical local
 * transforms and reset-stack opinions, and then seal the ordinal. The publication includes
 * invisible instance placements and prototype operands. A rendering-only time update may omit
 * invisible placements.
 *
 * ovclash invokes the callback as an owner operation: the calling thread already holds the
 * process-wide gate of ovclash_ovstage_run_owner_operation. Nested owner operations on that
 * thread are allowed. The callback must never wait for another thread that acquires the gate,
 * because that deadlocks. Owner locks taken in the callback are acquired after the gate, as that
 * function requires.
 *
 * @param user_data [in] The binding's caller-owned callback state.
 * @param seconds [in] Evaluation time in seconds, or NaN for Default.
 * @param out_committed_ordinal [out] Receives the sealed read ordinal on success.
 * @param out_error [out] Optional borrowed diagnostic on failure.
 * @return 1 on success; any other value, conventionally 0, reports failure. The ordinal must
 * strictly increase and fit the ordinal type of the supplied ovstage implementation
 * (ovstage_ordinal_t, a uint64_t in the pinned ovstage).
 * @note Threading: one context invokes the callback serially. The owner serializes callbacks
 * and the external use of any Stage shared by several contexts.
 */
typedef int32_t (*ovclash_ovstage_evaluate_time_fn)(void* user_data,
                                                    double seconds,
                                                    uint64_t* out_committed_ordinal,
                                                    const char** out_error);

/**
 * Opts in to authored transform classification on the borrowed Stage.
 *
 * The owner must populate every composed raw xformOp:* value and the USD-default
 * xformOpOrder:usd-metadata:default for all prims, ancestors and prototypes. A missing order must
 * mean that no default order is authored, not that the property was left out of the population.
 * No source edits, topology changes or manual raw-op writes may occur during a query.
 *
 * The evaluator may be called twice at identical finite seconds. Between those two committed
 * ordinals, the raw-op update records must contain exactly the initially valued operations that
 * have more than one effective composed USD time sample. Equal-valued samples count. Single
 * samples, and defaults that mask weaker samples, must not produce raw-op updates. Unused
 * operations may be populated. Classification follows the default order and all ancestors, also
 * across reset-stack boundaries. A missing initial active value stays unresolved.
 *
 * The owner must preserve the complete intervening update interval until the callback returns.
 * Canonical local and reset writes are allowed. A derived matrix change is never an
 * authored-animation signal. The capability requires evaluate_time. Without the capability,
 * attached meshes stay conservatively non-static. An attachment with a composition classifies
 * from its own population and must leave capabilities zero.
 */
#define OVCLASH_OVSTAGE_CAP_AUTHORED_TRANSFORM_UPDATES UINT64_C(1)

/** @brief Kind of the root of an attached Stage's composition. */
// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_ovstage_composition_root_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_OVSTAGE_COMPOSITION_ROOT_FILE = 0, /**< A USD file path or an omniverse:// URL. */
    OVCLASH_OVSTAGE_COMPOSITION_ROOT_USDA = 1 /**< Inline USDA text. */
} ovclash_ovstage_composition_root_t;

/**
 * Composition of an attached Stage, for time evaluation by ovclash.
 *
 * Names the ovstage population calls whose surviving effect is the borrowed Stage's USD source:
 * - root is the exact string the owner passed to ovstage_population_open_usd_from_file or its
 *   _with_desc variant (OVCLASH_OVSTAGE_COMPOSITION_ROOT_FILE: a path or an omniverse:// URL), or
 *   to ovstage_population_open_usd_from_string or its _with_desc variant
 *   (OVCLASH_OVSTAGE_COMPOSITION_ROOT_USDA). The replay does not depend on which variant the
 *   owner called.
 * - layers are the USDA texts of the owner's surviving
 *   ovstage_population_add_usd_reference_from_string(stage, text, "/") merges, in the order the
 *   owner added them, earliest first. A merge the owner removed, for example to mute it, is
 *   left out.
 *
 * ovclash replays the same calls with the same strings in the same order into ovstage instances
 * of its own. The strength order and the anchoring of inline layers therefore follow the owner's.
 * A composition cannot express references at other target paths, file references, or values
 * written directly into the owner's data plane. An owner that relies on any of them at finite
 * times must not supply a composition.
 *
 * Set struct_size to sizeof(ovclash_ovstage_composition_t). root_kind holds an
 * ovclash_ovstage_composition_root_t value. root and every layer are non-empty UTF-8 without an
 * embedded NUL. layers may be null only when nb_layers is 0. The attach call copies every
 * string.
 */
typedef struct ovclash_ovstage_composition_t
{
    uint64_t struct_size;
    ovclash_string_t root;
    uint32_t root_kind;
    uint32_t nb_layers;
    const ovclash_string_t* layers;
} ovclash_ovstage_composition_t;

/**
 * Binding of an existing ovstage Stage.
 *
 * The owner resolves the metadata. Without a composition, ovclash opens no file and probes no file
 * metadata. Set struct_size to the size of this structure. The stage and callback stay
 * caller-owned and must outlive the attachment and every operation on it.
 *
 * time_seconds describes the current evaluation of the stage. It must be finite, or NaN for
 * Default. Reads at that time use committed_ordinal without calling evaluate_time until another
 * time is evaluated. That publication must therefore be as complete as one made by
 * evaluate_time. meters_per_unit and both rates must be finite and positive. The timeline
 * endpoints must be finite. committed_ordinal must be positive, retained and covered by the
 * global write floor. The instance bundle and its version-query slot must be valid pointers, and
 * the instance must report the ovstage API major and minor version that ovclash was built
 * against, those of the pinned ovstage. evaluate_time may be null. user_data is passed to
 * evaluate_time unchanged.
 */
typedef struct ovclash_ovstage_binding_t
{
    uint64_t struct_size;
    ovstage_instance_t* stage;
    uint64_t committed_ordinal;
    double time_seconds;
    double meters_per_unit;
    double start_time_code;
    double end_time_code;
    double time_codes_per_second;
    double frames_per_second;
    ovclash_ovstage_evaluate_time_fn evaluate_time;
    void* user_data;
    /**
     * Optional tail guarded by struct_size: a struct_size that ends right before this field is
     * accepted and means no capabilities. Zero keeps the conservative classification. Unknown
     * bits are rejected, and a nonzero value requires evaluate_time.
     */
    uint64_t capabilities;
    /**
     * Optional tail guarded by struct_size: a struct_size that ends right before this field is
     * accepted and means no composition. Null keeps the owner-evaluated attachment.
     *
     * A non-null composition makes the attachment engine-evaluated; see
     * ovclash_ovstage_composition_t and ovclash_ovstage_attach_stage. time_seconds must then be
     * NaN, evaluate_time must be null and capabilities must be zero.
     */
    const ovclash_ovstage_composition_t* composition;
} ovclash_ovstage_binding_t;

/**
 * @defgroup ovclash_ovstage_api ovstage backend extensions
 *
 * Only the ovstage backend exports these symbols. A caller that selects a backend at run time
 * checks the backend before it loads these optional symbols.
 * @{
 */

/** @brief Matrix decomposition used for ovstage mesh poses and scales. */
// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_ovstage_matrix_decomposition_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_OVSTAGE_MATRIX_DECOMPOSITION_OPENUSD = 0, /**< OpenUSD-compatible math; the default. */
    OVCLASH_OVSTAGE_MATRIX_DECOMPOSITION_OVCLASH = 1 /**< Original ovclash math, for comparison. */
} ovclash_ovstage_matrix_decomposition_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Runs an owner operation under the process-wide ovstage lifecycle gate of ovclash.
 *
 * The gate coordinates external Stage creation, destruction, population, source edits,
 * inspection composition and renderer lifecycle with native file populations. The callback runs
 * synchronously on the calling thread and is never retained. Nested calls on the same thread
 * share the gate, up to 64 levels deep. Inside an owner callback, every guarded canonical entry
 * point and every Suite function that returns ovclash_suite_result_t fails with a reentry error.
 * Nested owner operations stay allowed. So do the unguarded ovclash_get_settings,
 * ovclash_get_last_error and the version, result-string, DLPack tensor-view and reset helpers.
 * Acquire this gate before any owner-specific lock. Attached evaluate_time callbacks already
 * run under it. Data-plane reads and collision work do not acquire it. The OpenUSD backend does
 * not export this optional symbol.
 *
 * Before and after the callback, the gate looks for the loaded client that sits beside the
 * ovstage runtime. The first successful client discovery, through SDK instance creation or this
 * gate, reads OVCLASH_OMNI_CLIENT_RETRY_MAX_MS and configures the client retry window once per
 * process. The default 0 disables reconnect retries; a positive value allows that many
 * milliseconds. A value that is not an unsigned 32-bit integer is reported on stderr and treated
 * as 0. The policy also affects other callers that share the process-wide client. While the
 * client is not available, configuration stays pending and a later discovery retries it. SDK
 * instance creation reports a missing client once per process on stderr when an explicit retry
 * setting cannot be applied. The checks made by this gate are silent.
 *
 * @param operation [in] Operation callback. Must not be null.
 * @param user_data [in] Borrowed callback state. May be null.
 * @return Success when the callback returns 1. Error for a null callback, callback failure,
 *         excessive nesting or a native exception. The gate is released on every exit. Callback
 *         diagnostics remain the caller's responsibility.
 *
 * @note Threading: owner operations on different threads are serialized. Nested calls on one
 *       thread run synchronously under the gate that thread already holds.
 */
OVCLASH_API ovclash_result_t ovclash_ovstage_run_owner_operation(ovclash_ovstage_owner_operation_fn operation,
                                                                 void* user_data);

/**
 * @brief Borrows an existing ovstage data-plane instance for one context.
 *
 * Never writes, evaluates on its own or destroys the instance.
 *
 * Without a composition the attachment is owner-evaluated and opens no file. Every read uses the
 * borrowed Stage, and a time other than time_seconds calls evaluate_time. Without a callback only
 * the supplied evaluation is available, and Default selection reads that frozen snapshot.
 *
 * With a composition the attachment is engine-evaluated:
 * - Default selection, geometry, visibility, subcomponent kinds, immediate queries and
 *   Default-time matrix queries read the borrowed Stage at committed_ordinal. committed_ordinal
 *   must stay retained until detach. Units and the timeline come from the binding, as without a
 *   composition.
 * - ovclash evaluates every finite time itself. That covers numeric transforms, static/dynamic
 *   classification, matrix prefetch and direct matrix queries at finite time codes. It replays
 *   the composition into private ovstage instances under the gate of
 *   ovclash_ovstage_run_owner_operation, at the first finite-time read, and destroys them at
 *   detach, reset, ovclash_release_memory or replacement.
 * - ovclash never calls evaluate_time and never advances the Stage.
 * - The owner guarantees that, at every finite time, its canonical local transforms and
 *   reset-stack opinions equal those of a fresh population of the composition. A transform the
 *   owner writes only into the data plane is therefore seen by Default selection and by no
 *   finite-time read.
 * - ovclash reopens the root by its string at every replay, including the first finite-time read,
 *   the next finite-time read after a failed replay, the first one after ovclash_release_memory, a
 *   read at an earlier time than the last one, each classification and, for an ambiguous rate,
 *   the first direct matrix query at a finite time code. The root file and every file that the
 *   root or a layer references must therefore stay unchanged on disk until detach; a changed
 *   file gives wrong finite-time poses without an error, or fails the run. A relative root
 *   resolves against the process working directory at each replay, so open the Stage from an
 *   absolute path or a URL and pass that string.
 * - meters_per_unit and time_codes_per_second must equal the values the composition resolves;
 *   otherwise the first finite-time read fails. One exception: when the root resolves
 *   timeCodesPerSecond 24 together with a framesPerSecond F other than 24, an authored 24 cannot
 *   be told from an absent rate that falls back to F, so a binding rate of 24 or F is accepted.
 *   The first direct matrix query at a finite time code, such as ovclash_get_prim_matrix, then
 *   resolves the root's effective rate and fails on a mismatch. Until then, sampling and
 *   classification, which work in seconds, do not detect a wrong rate, and
 *   ovclash_get_stage_timeline_data keeps reporting the binding's rate. Once detected, a
 *   mismatch fails every later finite-time read and classification of the attachment with the
 *   same diagnostic and without another replay, also after ovclash_release_memory, until the
 *   next attach.
 * - A composition that ovstage cannot replay fails the first finite-time read with the ovstage
 *   diagnostic.
 * - The owner must never hold that gate while it waits for a clash call.
 *
 * In both modes, while clash operations use the instance, the owner excludes rendering, edits,
 * time changes and destruction. A replacement invalidates geometry, results and pipelines.
 * ovclash_release_memory keeps the attachment; ovclash_reset_context, ovclash_ovstage_detach_stage
 * and ovclash_open_stage release it. Active async handles are rejected, including completed
 * handles that are not yet released.
 *
 * A successful attachment, first or replacement, also clears the scope paths and last error of
 * the context. Like ovclash_open_stage, it does not restore OVCLASH_SETTINGS_FLAG_FULL_SCENE
 * after ovclash_set_scope_paths cleared it, so a run would then select nothing. Set the scope
 * after attaching, or call ovclash_set_full_scene(context, true) for an unscoped run.
 *
 * @param context [in] An ovstage context from ovclash_create_context().
 * @param binding [in] Caller-owned Stage, metadata, optional callback and optional composition.
 *        Must not be null. The descriptor and the composition are copied; the Stage is borrowed.
 * @return Success, or error for a null context or binding, an invalid size, metadata, ordinal,
 *         Stage or composition, or active work. An invalid binding leaves the previous
 *         attachment and scope intact. ovclash_get_last_error reports the context diagnostic.
 *
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_ovstage_attach_stage(ovclash_context_t* context,
                                                          const ovclash_ovstage_binding_t* binding);

/**
 * @brief Releases a borrowed stage after all operations finish, without destroying it.
 *
 * Invalidates results and pipelines and clears the last error of the context. Active async
 * handles are rejected. On a context without an attachment the call also succeeds: it still
 * clears results and forgets any stage opened with ovclash_open_stage.
 *
 * @param context [in] An ovstage context from ovclash_create_context().
 * @return Success, or error for an invalid context or active work.
 *
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_ovstage_detach_stage(ovclash_context_t* context);

/**
 * @brief Selects mesh matrix decomposition for one ovstage context.
 *
 * A changed mode clears results, invalidates borrowed reports, and discards the current pipeline
 * and the cached geometry and poses. Release reports before the call, and create a new pipeline
 * for the next run. Reapplying the same mode keeps the native results and pipeline state.
 * Immediate mesh queries also use the selected mode. Raw reported world matrices and point-cloud
 * scaling do not depend on it.
 *
 * The setting survives a context reset and a stage replacement. New contexts use OPENUSD.
 * OVCLASH can change which pairs clash at zero tolerance and what they report. In both modes,
 * mesh decomposition rejects non-finite matrix values and values or linear row magnitudes outside
 * the float range. It also rejects resulting poses or scales that are non-finite or not
 * representable, including rotation quaternions that are not unit length. An affected pipeline
 * or immediate call reports an error.
 *
 * @param context [in] An ovstage context from ovclash_create_context().
 * @param implementation [in] OPENUSD or OVCLASH.
 * @return Success, or error for a null context, an invalid mode, an active async handle
 *         (including a completed handle that is not yet released) or a native failure. A failure
 *         leaves the selected mode unchanged. ovclash_get_last_error() reports context errors.
 *
 * @note Threading: serialize all calls on one context. Different contexts select independently.
 */
OVCLASH_API ovclash_result_t ovclash_ovstage_set_matrix_decomposition(
    ovclash_context_t* context, ovclash_ovstage_matrix_decomposition_t implementation);

/**
 * @brief Reads the selected decomposition of an ovstage context.
 *
 * Does not change results or pipeline state.
 *
 * @param context [in] An ovstage context from ovclash_create_context().
 * @param implementation [out] Receives the selected mode on success.
 * @return Success, or error for a null context or output pointer.
 *
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_ovstage_get_matrix_decomposition(
    ovclash_context_t* context, ovclash_ovstage_matrix_decomposition_t* implementation);

#ifdef __cplusplus
}
#endif
/** @} */
