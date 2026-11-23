// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ovclash/ovclash_export.h"
#include "ovclash_suite/ovclash_suite_types.h"
#include "ovclash_suite/ovclash_suite_models.h"
#include "ovclash_suite/ovclash_suite_serializer.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \defgroup ovclash_suite_c_api ovclash_suite C API
 *
 * C-first Suite API for clash runtime orchestration, data serialization, and HTML and JSON
 * export. Functions complete their operation before they return, except the three launchers that
 * return an async handle: ovclash_suite_run_pipeline_async (asynchronous pipeline), and
 * ovclash_suite_runtime_fetch_and_save_async and
 * ovclash_suite_runtime_fetch_and_save_async_with_reader (asynchronous fetch and save).
 * Independent data and model handles are thread-compatible, subject to database and filesystem
 * locking. Runtime calls follow the shared-state limits of the backend: OpenUSD runtimes must be
 * serialized, because their cancellation, out-of-memory and underlying error state are
 * process-global.
 *
 * Every function returns ovclash_suite_result_t, except the three last-error getters
 * (ovclash_suite_get_last_error, ovclash_suite_data_get_last_error and
 * ovclash_suite_get_thread_last_error), which return the text directly. String parameters and
 * fields are length-prefixed ovclash_suite_string_t values. A returned view has the lifetime that
 * its producing API documents. Data getters reuse per-kind scratch records, so a later read of
 * the same kind can invalidate an earlier view without mutating the database.
 *
 * Buffers take an explicit byte size and return the required UTF-8 byte count through
 * out_required_size. A null buffer with size zero is the supported size query. A non-null buffer
 * that cannot hold the text plus its trailing null fails and still reports the required byte
 * count. On error, call the get_last_error function that matches the failed handle, or
 * ovclash_suite_get_thread_last_error for APIs without a handle. An input ovclash_suite_string_t
 * is valid only when ptr is non-null or length is zero. A null ptr with a non-zero length is
 * rejected as OVCLASH_SUITE_STATUS_ERROR before the API reads it. Filesystem paths are UTF-8 and
 * must not contain an embedded NUL character, which the operating system would read as the end
 * of a shorter path. Such a path is rejected as OVCLASH_SUITE_STATUS_ERROR before anything is
 * touched, except by ovclash_suite_file_exists, which reports it as no file.
 *
 * Common error catalog for functions returning ovclash_suite_result_t:
 * - OVCLASH_SUITE_STATUS_SUCCESS: the operation completed and output parameters are initialized.
 * - OVCLASH_SUITE_STATUS_ERROR: an invalid argument, string span or UTF-8 text, a counted buffer
 *   that is null with a non-zero count, an unsupported backend capability, a failed runtime or
 *   source operation, a conflict with a live fetch or cursor, a SQLite or filesystem failure, an
 *   output buffer that is too small, an allocation failure, a caught C++ exception, or, in
 *   ovstage builds, a call made inside an owner-operation callback on the same thread (see
 *   ovclash_suite_run_ovstage_owner_operation). The thread-local last-error string describes the
 *   specific failure, and the handle-local last-error accessors keep the failures of their
 *   handles.
 * - OVCLASH_SUITE_STATUS_UNAVAILABLE: only create_overlap_report and create_clash_info return
 *   this, when a valid record has no available report. No report is returned. Extraction,
 *   allocation, cancellation and out-of-memory failures remain OVCLASH_SUITE_STATUS_ERROR.
 *
 * This common catalogue is part of the contract of every function that returns
 * ovclash_suite_result_t, unless the function documents a narrower error list. The function
 * documentation defines the valid ranges, ownership, side effects and threading of each call.
 * Callers therefore need not infer the repeated C ABI failure states from implementation details.
 *
 * Example:
 * \code
 * ovclash_suite_runtime_t* runtime = 0;
 * if (ovclash_suite_create_runtime(&runtime).status == OVCLASH_SUITE_STATUS_SUCCESS) {
 *     ovclash_suite_destroy_runtime(runtime);
 * }
 * \endcode
 *
 * Runtime example:
 * \code
 * ovclash_suite_runtime_t* runtime = 0;
 * ovclash_suite_create_runtime(&runtime);
 * ovclash_suite_open_stage(runtime, (ovclash_suite_string_t){ "scene.usda", 10 });
 * ovclash_suite_set_scope(runtime, NULL, 0, NULL, 0);
 * uint32_t steps = 0;
 * ovclash_suite_create_pipeline(runtime, &steps);
 * for (uint32_t i = 0; i < steps; ++i) {
 *     ovclash_suite_run_pipeline_step(runtime, i);
 * }
 * ovclash_suite_destroy_runtime(runtime);
 * \endcode
 *
 * Data example:
 * \code
 * ovclash_suite_data_t* data = 0;
 * ovclash_suite_data_create(&data);
 * ovclash_suite_data_open(data, (ovclash_suite_string_t){ "scene.ovclashdb", 15 });
 * ovclash_suite_data_commit(data);
 * ovclash_suite_data_destroy(data);
 * \endcode
 *
 * Export example:
 * \code
 * ovclash_suite_export_column_t columns[] = {
 *     { 0, { "Name", 4 }, 0 },
 *     { 1, { "State", 5 }, 0 },
 * };
 * ovclash_suite_string_t rows[] = {
 *     { "A", 1 }, { "Active", 6 },
 * };
 * ovclash_suite_export_text_t* text = 0;
 * ovclash_suite_export_rows_json_text(columns, 2, rows, 2, 1, NULL, 0, &text);
 * ovclash_suite_export_text_destroy(text);
 * \endcode
 *
 * Preconditions: out pointers must be non-null unless documented as optional.
 * Side effects: create, open, save, insert, update and remove functions mutate the supplied handle.
 * Ownership: release every handle returned through an out parameter with the matching destroy
 * call.
 */
/** @{ */

/**
 * Fills a run-settings struct with the suite defaults.
 *
 * The defaults are the engine defaults of ovclash_reset_settings, expressed as Suite fields. The
 * exceptions: pose_epsilon and area_epsilon hold the exact double 1e-6, bounds_epsilon the exact
 * double 0.01, and the Suite-only depth_epsilon is -1.0.
 *
 * \param out_settings Receives the default run settings. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_default_run_settings(ovclash_suite_run_settings_t* out_settings);

/**
 * Serializes run settings into the legacy clash-query settings JSON object.
 *
 * The native Suite layer owns this serializer, so C and Python callers share the setting names,
 * defaults and numeric formatting of the persisted clash_detect_settings. The keys are the
 * ovclash_suite_setting_id_t names without their OVCLASH_SUITE_ prefix.
 *
 * Finite doubles are written in printf %.17g form, with up to 17 significant digits and trailing
 * zeros dropped, so they round-trip exactly. This is deliberately not the shortest form: a
 * default epsilon of 1e-6 is written as `9.9999999999999995e-07`. The text is persisted
 * verbatim, so its spelling is part of the format. Negative zero is written as -0.0. Formatting
 * uses the classic numeric locale and
 * temporarily selects the default floating-point environment, then restores the complete
 * environment of the caller. Directed rounding or flushed denormals therefore do not change the
 * saved text.
 *
 * \param settings Run settings to serialize. Must not be null.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when serialization completed or a size query succeeded.
 *         OVCLASH_SUITE_STATUS_ERROR when settings/out_required_size are null, any settings
 *         double is non-finite, out_buffer is null with non-zero out_buffer_size, out_buffer
 *         is too small, allocation fails, or an exception is caught.
 *         The thread-local last-error string contains the concrete reason.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_run_settings_to_query_settings_json(
    const ovclash_suite_run_settings_t* settings, char* out_buffer, size_t out_buffer_size, size_t* out_required_size);

/**
 * Returns the semantic version of the ovclash library, which the Suite shares, as compiled.
 *
 * \param out_major Receives the major version number. Must not be null.
 * \param out_minor Receives the minor version number. Must not be null.
 * \param out_patch Receives the patch version number. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_version(uint32_t* out_major,
                                                             uint32_t* out_minor,
                                                             uint32_t* out_patch);

/**
 * Capability and load-path information for the native clash backend the library was built with.
 *
 * Every value is a compile-time constant. Field catalogue:
 * - backend_name: "openusd" or "ovstage"; borrows a static library-owned string valid for the
 *   process lifetime.
 * - native_openusd_linked: 1 for the OpenUSD backend, otherwise 0.
 * - ovpopulation_runtime_loaded: 1 for the ovstage backend, otherwise 0.
 * - ovstage_cuda_enabled: 1 only for an ovstage backend built with CUDA support.
 */
typedef struct ovclash_suite_backend_info_t
{
    ovclash_suite_string_t backend_name;
    uint32_t native_openusd_linked;
    uint32_t ovpopulation_runtime_loaded;
    uint32_t ovstage_cuda_enabled;
} ovclash_suite_backend_info_t;

/**
 * Returns capability and load-path information for the active clash backend.
 *
 * \param out_info Receives the backend name and capability flags. Must not be null.
 * \return SUCCESS, or ERROR when out_info is null or, in ovstage builds, when the call runs inside an
 *         owner-operation callback on the same thread (see ovclash_suite_run_ovstage_owner_operation).
 *         Nothing is written on error; ovclash_suite_get_thread_last_error gives the reason.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_backend_info(ovclash_suite_backend_info_t* out_info);

/**
 * \name Runtime API
 *
 * Per-function error catalogue for the runtime API: each runtime function returning
 * ovclash_suite_result_t returns OVCLASH_SUITE_STATUS_SUCCESS when the requested stage,
 * settings, scope, pipeline, query, report or persistence operation completed and the documented
 * outputs are initialized. It returns OVCLASH_SUITE_STATUS_ERROR when:
 * - a runtime handle, output pointer or required matrix pointer is null;
 * - a string span is invalid or not UTF-8;
 * - no stage is open, or the selected backend does not support the settings or scope;
 * - the pipeline is in the wrong state, or a step, overlap, record or duplicate index is out of
 *   range;
 * - native clash execution fails, or report buffers are missing or inconsistent;
 * - a data handle supplied to the fast-save path is not writable;
 * - allocation fails, or a C++ exception is caught.
 *
 * ovclash_suite_get_last_error returns the runtime-specific detail, and
 * ovclash_suite_get_thread_last_error the thread-local detail.
 *
 * Threading: callers serialize all access to a shared runtime or async handle. A runtime and
 * every async handle it created share one serialization domain. No call on such an async handle
 * may overlap any call on its runtime, ovclash_suite_get_last_error and
 * ovclash_suite_destroy_runtime included. The async calls update the active-run guard and error
 * text of the runtime, and destroying the runtime detaches its async handle. Poll progress from
 * another thread only while no call on the runtime is in flight. Independent ovstage runtime
 * handles may be used concurrently. OpenUSD runtime calls must also be serialized across handles,
 * because cancellation, out-of-memory and underlying error state are process-global.
 *
 * Ownership: the caller owns runtime-created report and async handles until it calls their
 * matching destroy function. Runtime string and report views are borrowed from the owning runtime
 * or report handle. On ovstage, report arrays borrow runtime result storage: destroy reports
 * before rerunning or resetting the runtime or invalidating its results. Always destroy reports
 * before their runtime.
 *
 * A live fetch-and-save operation reserves its runtime until the fetch is destroyed, also after
 * completion or failure. During that reservation, runtime destroy and reset, stage, settings and
 * scope changes, pipeline creation and execution, and depth computation fail. Read-only access to
 * results stays available during a synchronous fetch. An asynchronous fetch rejects every runtime
 * call outside its worker until the fetch is destroyed. The guard relies on serialized caller
 * access; it is neither a mutex nor a lease for separately created report handles. Destroy the
 * fetch before retrying a rejected operation.
 */
/** @{ */

/**
 * Creates a suite runtime handle.
 *
 * \param out_runtime Receives the created runtime handle. Must not be null and must be destroyed with
 * ovclash_suite_destroy_runtime.
 *
 * \return Success when the handle is allocated. Failure when out_runtime is
 *         null, allocation fails, or an exception is caught.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_create_runtime(ovclash_suite_runtime_t** out_runtime);

/**
 * Destroys a suite runtime handle.
 *
 * Destruction does not cancel an unfinished pipeline started by ovclash_suite_run_pipeline_async.
 * Like ovclash_release_context, it blocks until that run completes and discards its results.
 * Call ovclash_suite_async_cancel first to stop the run early. The async handle stays
 * caller-owned; release it with ovclash_suite_async_destroy afterwards. No call on that handle
 * may be in flight during destruction. Afterwards every call on it except
 * ovclash_suite_async_destroy fails.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. May be null.
 *
 * \return Success when the handle is released or runtime is null. Failure if a fetch is alive
 *         or cleanup catches an unexpected exception. An active-fetch rejection retains the
 *         runtime; destroy the fetch before retrying runtime destruction.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_destroy_runtime(ovclash_suite_runtime_t* runtime);

/**
 * Clears runtime state while keeping the handle alive.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 *
 * \return Success, or error for a null runtime, an active async pipeline or fetch, or a reset
 *         the backend rejects.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_reset_runtime(ovclash_suite_runtime_t* runtime);

/**
 * Returns the last error message recorded on a runtime handle.
 *
 * While an async fetch owner is alive, returns the thread-local access-rejection diagnostic of
 * the caller without reading runtime state. ovclash_suite_fetch_async_get_last_error returns the
 * diagnostic of the operation. Destroy that owner before querying runtime state.
 *
 * \param runtime Runtime handle to query. May be null. A null handle, or one with no recorded
 *        error, falls back to the thread-local last error; the view is empty only when that is
 *        empty too.
 */
OVCLASH_API ovclash_suite_string_t ovclash_suite_get_last_error(const ovclash_suite_runtime_t* runtime);

/**
 * Returns the last thread-local error recorded by a Suite API without a handle-local error
 * channel.
 *
 * Once the thread-local storage of the calling thread is destroyed, a failure records a static
 * notice instead of its detail. At exit, glibc destroys that storage of the main thread before
 * static destructors run.
 *
 * \return Library-owned UTF-8 string view, valid until the next Suite API call on the same thread.
 */
OVCLASH_API ovclash_suite_string_t ovclash_suite_get_thread_last_error(void);

/**
 * Looks up a public numeric ABI constant by its C identifier.
 *
 * Dynamic language bindings use it to derive enum, flag, schema-version and compatibility
 * constants from the native library instead of duplicating integer literals. The supported names
 * are:
 * - the enumerators of the OVCLASH_SUITE_* enums;
 * - the alias spellings OVCLASH_OVERLAP_TYPE_* and OVCLASH_OVERLAP_REPORT_*, without the PAIR or
 *   FLAG infix of ovclash.h;
 * - the schema versions OVCLASH_SUITE_CLASH_QUERY_VERSION, OVCLASH_SUITE_CLASH_INFO_VERSION,
 *   OVCLASH_SUITE_CLASH_FRAME_INFO_VERSION and OVCLASH_SUITE_POINT_CLOUD_SETTINGS_VERSION;
 * - the model default OVCLASH_SUITE_POINT_CLOUD_SETTINGS_MAX_LOD_LEVEL.
 *
 * The enumerator names of the canonical ovclash.h are not looked up, and an unknown name fails.
 *
 * \param name Length-prefixed constant identifier, for example
 * OVCLASH_SUITE_SETTING_DYNAMIC.
 * \param out_value Receives the constant value. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_named_constant(ovclash_suite_string_t name, int64_t* out_value);

/**
 * Queries whether the last runtime execution reported native out-of-memory.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_is_out_of_memory Receives 1 when the backend reported out-of-memory, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_is_out_of_memory(ovclash_suite_runtime_t* runtime,
                                                                  uint32_t* out_is_out_of_memory);

/**
 * Encodes a USD Sdf path into the integer path form used by the clash backend.
 *
 * \param prim_path Length-prefixed USD prim path to encode.
 * \param out_encoded_path Receives the encoded path value. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_sdf_path_to_int(ovclash_suite_string_t prim_path,
                                                                 uint64_t* out_encoded_path);

/**
 * Reads a prim's local-to-world transform from the currently opened stage.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param prim_path Length-prefixed USD prim path to query.
 * \param time USD time code (not seconds) at which the transform is evaluated.
 * \param out_matrix Receives a row-major 4x4 transform. Must point to 16 doubles.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_prim_matrix(ovclash_suite_runtime_t* runtime,
                                                                 ovclash_suite_string_t prim_path,
                                                                 double time,
                                                                 double* out_matrix);

/**
 * Reads the timeline metadata of the currently opened stage.
 *
 * Fails when no stage is open. Use it to convert between the time codes of the stage and the
 * seconds in start_time and end_time of ovclash_suite_run_settings_t, which select the sampled
 * range: seconds = time code / time_codes_per_second. It can also supply a dynamic range from the
 * stage. static_time, dynamic_start_time and dynamic_end_time are persisted query
 * metadata only; the runtime does not read them. Available on both backends without the OpenUSD
 * Python bindings.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_data Receives the stage timeline record; zeroed before validation. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_stage_timeline_data(ovclash_suite_runtime_t* runtime,
                                                                         ovclash_suite_stage_timeline_data_t* out_data);

/**
 * Opens a file-backed USD stage for clash detection.
 *
 * A successful open clears the scope set by ovclash_suite_set_scope; set it again for a scoped
 * run on the new stage. A failed open keeps it. A non-empty scope turns the engine's full-scene
 * selection off; a successful open turns it back on when the run settings in effect enable
 * enable_full_scene, so a run with no new scope covers the whole new stage. Those are the
 * settings last applied with ovclash_suite_set_run_settings or, before any successful apply, the
 * engine defaults, which enable it.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param stage_path Length-prefixed filesystem path to the USD stage.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_open_stage(ovclash_suite_runtime_t* runtime,
                                                            ovclash_suite_string_t stage_path);

/**
 * Applies clash runtime settings.
 *
 * The threading settings may change after the pipeline finishes and before
 * ovclash_suite_runtime_fetch_and_save_begin, for example parallel detection followed by
 * single-threaded fetching. Completed results are kept. To switch the fetch mode, read the current
 * settings first (ovclash_suite_get_default_run_settings when none were applied, because
 * ovclash_suite_get_run_settings then returns a zeroed struct) and change only the threading
 * fields. Finish or cancel an active async pipeline,
 * and destroy any live fetch operation, before changing settings.
 *
 * A scope set by ovclash_suite_set_scope survives a settings change, in either call order. The
 * floating-point fields that the engine stores as float must lie within the float range:
 * pose_epsilon, bounds_epsilon, area_epsilon, contact_cutoff, tolerance, point_size_override, and
 * depth_epsilon while discard_touching_contacts is set.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param settings Settings to apply. Must not be null. The struct is copied.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when the engine accepted the settings.
 *         OVCLASH_SUITE_STATUS_ERROR, keeping the previous settings, when an argument is null,
 *         an async pipeline or a fetch is alive, a double is non-finite, a float-stored field
 *         exceeds the float range, max_local_depth_mode exceeds 2, nb_tasks exceeds 1000000,
 *         tris_per_leaf is outside [1, 15], time_codes_per_second is negative, set_times is
 *         non-zero with end_time below start_time, or the engine rejects them. A failure to
 *         re-apply the cached scope afterwards also returns OVCLASH_SUITE_STATUS_ERROR, with the
 *         new settings already in effect.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_set_run_settings(ovclash_suite_runtime_t* runtime,
                                                                  const ovclash_suite_run_settings_t* settings);

/**
 * Reads back the run settings currently applied to a runtime.
 *
 * The inverse of ovclash_suite_set_run_settings. Reports the last settings that the engine
 * accepted; a rejected apply leaves the previous values in place. Before any successful apply it
 * reports a zero-initialized struct. That struct differs from
 * ovclash_suite_get_default_run_settings, and the runtime does not use it either: until then,
 * detection and fetch preparation run with the engine defaults, the ovclash_reset_settings table.
 *
 * Suite-only fields that the canonical settings struct cannot carry (dynamic, static_time,
 * set_times, depth_epsilon, ...) round-trip here, although the engine cannot return them.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_settings Receives the current settings. Must not be null.
 * \return OVCLASH_SUITE_STATUS_SUCCESS, or an error status if either argument is
 *         null or an async fetch owner is alive (nothing is written).
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_run_settings(const ovclash_suite_runtime_t* runtime,
                                                                  ovclash_suite_run_settings_t* out_settings);

/**
 * Restricts clash detection to one or two sets of USD prim paths.
 *
 * The paths are copied into runtime state and forwarded to ovclash_set_scope_paths. paths1 alone
 * means paths1 against itself, both non-empty means paths1 against paths2, and both empty means
 * no scope. The runtime handle is mutated only after argument validation succeeds. The runtime
 * applies the copy again after each ovclash_suite_set_run_settings, and before
 * ovclash_suite_create_pipeline builds, so scope and settings may be set in either order. A
 * successful ovclash_suite_open_stage or ovstage attachment clears the scope.
 *
 * A non-empty scope turns the full-scene selection of the engine off. A both-empty scope, a
 * successful open or an attachment turns it back on when the run settings in effect enable
 * enable_full_scene. The settings in effect are those last applied with
 * ovclash_suite_set_run_settings or, before any successful apply, the engine defaults, which
 * enable it. An unscoped run therefore selects the whole stage, whichever of scope and settings
 * was set last. With enable_full_scene cleared, an unscoped run selects nothing.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param prim_paths1 Array of first-scope prim paths. May be null when prim_paths1_count is zero.
 * \param prim_paths1_count Number of entries in prim_paths1.
 * \param prim_paths2 Array of second-scope prim paths. May be null when prim_paths2_count is zero.
 * \param prim_paths2_count Number of entries in prim_paths2.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when the scope is accepted.
 *         OVCLASH_SUITE_STATUS_ERROR when runtime is null, an async pipeline or a fetch is
 *         alive, either array pointer is null with a non-zero count, any string span is
 *         invalid or non-UTF-8, the engine rejects an entry (empty, containing an embedded NUL
 *         character, or not an absolute prim path), memory allocation fails, or a C++
 *         exception is caught. Error detail is available through
 *         ovclash_suite_get_last_error(runtime) and ovclash_suite_get_thread_last_error.
 *
 * Threading: callers serialize access to a shared runtime while changing the scope. Independent
 * ovstage runtime handles may be configured concurrently.
 *
 * Example:
 * \code
 * ovclash_suite_string_t a[] = { { "/World/A", 8 } };
 * ovclash_suite_string_t b[] = { { "/World/B", 8 } };
 * ovclash_suite_set_scope(runtime, a, 1, b, 1);
 * \endcode
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_set_scope(ovclash_suite_runtime_t* runtime,
                                                           const ovclash_suite_string_t* prim_paths1,
                                                           size_t prim_paths1_count,
                                                           const ovclash_suite_string_t* prim_paths2,
                                                           size_t prim_paths2_count);

/**
 * Builds the runtime pipeline for the currently opened stage, settings, and scope.
 *
 * Preconditions: configure settings and scope first when non-default behavior is needed. The
 * runtime must not be running an async pipeline. Building the pipeline needs no stage; a missing
 * stage surfaces from ovclash_suite_run_pipeline_step or the async run.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_step_count Receives the number of pipeline steps. Must not be null.
 *
 * Side effects: applies the scope set by ovclash_suite_set_scope again and, on success, replaces
 * any previously prepared pipeline and stores the new step count. A failed ovstage build keeps the
 * previous pipeline. Runtime result buffers are
 * populated only when ovclash_suite_run_pipeline_step or ovclash_suite_run_pipeline_async runs. A
 * failure to apply the scope or to create the pipeline leaves *out_step_count at 0.
 *
 * Ownership: the caller keeps owning runtime. out_step_count receives a scalar value and owns no
 * memory.
 *
 * Threading: callers serialize access to runtime while creating or running a pipeline.
 * Independent ovstage runtime handles may create pipelines concurrently.
 *
 * Example:
 * \code
 * uint32_t steps = 0;
 * if (ovclash_suite_create_pipeline(runtime, &steps).status == OVCLASH_SUITE_STATUS_SUCCESS) {
 *     for (uint32_t index = 0; index < steps; ++index) {
 *         ovclash_suite_run_pipeline_step(runtime, index);
 *     }
 * }
 * \endcode
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when the pipeline is prepared and out_step_count is
 *         initialized. OVCLASH_SUITE_STATUS_ERROR when runtime or out_step_count is null, an
 *         async pipeline or a fetch is alive, applying the scope again fails, backend pipeline
 *         creation fails, an unsupported backend capability is requested, allocation fails, or a
 *         C++ exception is caught. ovclash_suite_get_last_error(runtime) and
 *         ovclash_suite_get_thread_last_error return the failure detail.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_create_pipeline(ovclash_suite_runtime_t* runtime,
                                                                 uint32_t* out_step_count);

/**
 * Describes one planned pipeline step.
 *
 * progress is step_index / nb_steps and finished marks the last step; neither reflects execution.
 * Use ovclash_suite_async_get_step_data for live progress.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param step_index Zero-based pipeline step index.
 * \param out_step_data Receives step name, index, count, progress, and finished state. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_pipeline_step_data(ovclash_suite_runtime_t* runtime,
                                                                        uint32_t step_index,
                                                                        ovclash_suite_step_data_t* out_step_data);

/**
 * Executes one pipeline step.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param step_index Zero-based pipeline step index to execute.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_run_pipeline_step(ovclash_suite_runtime_t* runtime,
                                                                   uint32_t step_index);

/**
 * Starts asynchronous execution of the prepared pipeline.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_async Receives the async handle. Must not be null and must be destroyed with
 * ovclash_suite_async_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_run_pipeline_async(ovclash_suite_runtime_t* runtime,
                                                                    ovclash_suite_async_t** out_async);

/**
 * Polls whether an asynchronous pipeline is still running.
 *
 * \param async_handle Async handle returned by ovclash_suite_run_pipeline_async. Must not be null.
 * \param out_running Receives 1 while execution is still active, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_async_is_running(ovclash_suite_async_t* async_handle,
                                                                  uint32_t* out_running);

/**
 * Reads progress information from an asynchronous pipeline.
 *
 * progress is forwarded unclamped from ovclash_get_pipeline_step_data_async. On OpenUSD, a poll
 * that races the work-counter reset of a phase can report it outside [0, 1] or non-finite.
 * Consumers clamp it and treat a non-finite value as unknown. ovstage reports a locked, clamped
 * snapshot. Clash results are unaffected.
 *
 * \param async_handle Async handle returned by ovclash_suite_run_pipeline_async. Must not be null.
 * \param out_step_data Receives the active step information. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_async_get_step_data(ovclash_suite_async_t* async_handle,
                                                                     ovclash_suite_step_data_t* out_step_data);

/**
 * Waits for asynchronous pipeline execution to finish and consumes the engine run.
 *
 * After finish or cancel, polling, finish, and cancel calls on the same handle return
 * OVCLASH_SUITE_STATUS_ERROR. Release the remaining handle with ovclash_suite_async_destroy.
 *
 * \param async_handle Async handle returned by ovclash_suite_run_pipeline_async. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_async_finish(ovclash_suite_async_t* async_handle);

/**
 * Requests cancellation of asynchronous pipeline execution.
 *
 * Blocks until the worker stops and consumes the engine run. Release the remaining handle
 * with ovclash_suite_async_destroy; a later ovclash_suite_async_finish on it returns
 * OVCLASH_SUITE_STATUS_ERROR.
 *
 * \param async_handle Async handle returned by ovclash_suite_run_pipeline_async. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_async_cancel(ovclash_suite_async_t* async_handle);

/**
 * Releases an asynchronous pipeline handle.
 *
 * An unfinished run is finished first, blocking until it completes. The handle may outlive its
 * runtime; after ovclash_suite_destroy_runtime this only frees it. While the runtime is alive,
 * this call, like every other call on the handle, must not overlap a call on that runtime.
 *
 * \param async_handle Async handle returned by ovclash_suite_run_pipeline_async. May be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_async_destroy(ovclash_suite_async_t* async_handle);

/**
 * Queries the number of detected overlap groups.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_overlap_count Receives the overlap group count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_nb_overlaps(ovclash_suite_runtime_t* runtime,
                                                                 uint32_t* out_overlap_count);

/**
 * Queries the number of detected duplicate mesh pairs.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_duplicate_count Receives the duplicate pair count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_nb_duplicates(ovclash_suite_runtime_t* runtime,
                                                                   uint32_t* out_duplicate_count);

/**
 * Reads detailed data for one overlap record.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param overlap_index Zero-based overlap group index.
 * \param record_index Zero-based record/frame index inside the overlap group.
 * \param out_data Receives overlap names, hashes, counts, type, time, distance, and depth. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_overlap_data(ovclash_suite_runtime_t* runtime,
                                                                  uint32_t overlap_index,
                                                                  uint32_t record_index,
                                                                  ovclash_suite_overlap_data_t* out_data);

/**
 * Reads duplicate mesh data for one duplicate pair.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param duplicate_index Zero-based duplicate mesh pair index.
 * \param out_data Receives duplicate names, hashes, triangle count and time; the matrices stay zero. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_duplicate_data(ovclash_suite_runtime_t* runtime,
                                                                    uint32_t duplicate_index,
                                                                    ovclash_suite_duplicate_mesh_data_t* out_data);

/**
 * Queries runtime statistics for the last pipeline execution.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param out_stats Receives mesh and point cloud counters. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_query_stats(ovclash_suite_runtime_t* runtime,
                                                                 ovclash_suite_query_stats_t* out_stats);

/**
 * Computes the maximum local depth for an already detected overlap record.
 *
 * Not supported for point-cloud overlaps. The backends compute the depth from the mesh pair behind
 * the record, so an overlap whose ovclash_suite_overlap_data_t::type is
 * OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH fails here. Test the overlap type before the call.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param overlap_index Zero-based overlap group index.
 * \param record_index Zero-based record/frame index inside the overlap group.
 * \param out_depth Receives the computed maximum local depth. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_compute_overlap_max_local_depth(ovclash_suite_runtime_t* runtime,
                                                                                 uint32_t overlap_index,
                                                                                 uint32_t record_index,
                                                                                 double* out_depth);

/**
 * Computes maximum local depth between two mesh instances.
 *
 * Both paths must resolve to UsdGeomMesh prims. A UsdGeomPoints path is never loaded as a mesh,
 * so the call fails for it. A stage must be open, but no pipeline run is required: the call loads
 * and cooks the two meshes with the settings in effect. It fails unless those settings enable
 * compute_max_local_depth, which the engine defaults do not. max_local_depth_mode selects the
 * algorithm. *out_depth is unchanged on failure.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param mesh_path0 Length-prefixed USD path of the first mesh.
 * \param matrix0 Row-major 4x4 transform for the first mesh. Must point to 16 doubles.
 * \param mesh_path1 Length-prefixed USD path of the second mesh.
 * \param matrix1 Row-major 4x4 transform for the second mesh. Must point to 16 doubles.
 * \param out_depth Receives the computed maximum local depth. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_compute_max_local_depth(ovclash_suite_runtime_t* runtime,
                                                                         ovclash_suite_string_t mesh_path0,
                                                                         const double* matrix0,
                                                                         ovclash_suite_string_t mesh_path1,
                                                                         const double* matrix1,
                                                                         double* out_depth);

/**
 * Computes penetration depth along a supplied direction between two mesh instances.
 *
 * Both paths must resolve to UsdGeomMesh prims. A UsdGeomPoints path is never loaded as a mesh, so
 * the call fails for it.
 *
 * The direction need not be normalized; it is converted to float before the query. Each
 * component must be finite and no larger in magnitude than FLT_MAX. The square of at least one
 * float component must be a normal float; squares below FLT_MIN count as zero, because PhysX
 * flushes them. The squared float length must not overflow. A zero direction, a direction too
 * small to survive the conversion, and a direction whose length overflows float are rejected with
 * an error rather than queried.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param mesh_path0 Length-prefixed USD path of the first mesh.
 * \param matrix0 Row-major 4x4 transform for the first mesh. Must point to 16 doubles.
 * \param mesh_path1 Length-prefixed USD path of the second mesh.
 * \param matrix1 Row-major 4x4 transform for the second mesh. Must point to 16 doubles.
 * \param dir_x X component of the query direction.
 * \param dir_y Y component of the query direction.
 * \param dir_z Z component of the query direction.
 * \param out_data Receives penetration direction and depth. Must not be null. A failure other than
 *        the rejection while a fetch is alive, or a call made from inside an ovstage owner
 *        operation, leaves a zero direction and depth -1.0.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_compute_penetration_depth(ovclash_suite_runtime_t* runtime,
                                        ovclash_suite_string_t mesh_path0,
                                        const double* matrix0,
                                        ovclash_suite_string_t mesh_path1,
                                        const double* matrix1,
                                        double dir_x,
                                        double dir_y,
                                        double dir_z,
                                        ovclash_suite_depenetration_data_t* out_data);

/**
 * Creates a detailed overlap report containing requested face, point, outline, and matrix buffers.
 *
 * Destroy the report before advancing another pipeline step, rerunning or resetting the runtime,
 * invalidating its results, or destroying the runtime. The runtime must stay alive until the
 * report is destroyed. On ovstage, report buffers borrow storage from the runtime results;
 * creating a report does not extend the lifetime of that storage.
 *
 * Point-cloud overlaps are supported. For them the report holds the indices of the clashing
 * points (OVCLASH_SUITE_OVERLAP_REPORT_POINTS0) and the cloud-wide point_size0. That size is 0.0
 * when per-point authored widths that actually vary size the cloud; this API does not resolve
 * those widths. A uniform widths array holds one value repeated per point. USD's default "vertex"
 * interpolation makes an exporter write such an array for a cloud with one size. For such an
 * array the report holds that single width on the ovstage backend, and 0.0 on the OpenUSD
 * backend, whose sizing lives in the shared engine. See "Point-cloud widths in reports" in the
 * Limitations guide of the ovclash documentation.
 *
 * Faces and collision outlines are reported for mesh-vs-mesh pairs only. Both backends ignore the
 * flags that do not apply to the pair type, and OVCLASH_SUITE_OVERLAP_REPORT_DEFAULT and
 * OVCLASH_SUITE_OVERLAP_REPORT_FACES select nothing here. Both backends compute collision outlines
 * under the floating-point controls of the calling thread, so reports created under different
 * rounding or denormal modes can differ. A Suite runtime always uses the default ovstage matrix
 * decomposition, so its outlines always follow the calling thread's controls.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param overlap_index Zero-based overlap group index.
 * \param record_index Zero-based record/frame index inside the overlap group.
 * \param flags Bitmask of ovclash_suite_overlap_report_flag_t values selecting report buffers.
 * \param out_report Receives the created report handle. Must not be null and must be destroyed with
 * ovclash_suite_destroy_overlap_report.
 * \return OVCLASH_SUITE_STATUS_SUCCESS with a report; OVCLASH_SUITE_STATUS_UNAVAILABLE with a
 * null report when the record exists but cannot provide report data; OVCLASH_SUITE_STATUS_ERROR
 * for invalid arguments, extraction failures, cancellation, or allocation failure.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_create_overlap_report(ovclash_suite_runtime_t* runtime,
                                                                       uint32_t overlap_index,
                                                                       uint32_t record_index,
                                                                       uint32_t flags,
                                                                       ovclash_suite_overlap_report_t** out_report);

/**
 * Returns the buffer view for an overlap report.
 *
 * \param report Report handle returned by ovclash_suite_create_overlap_report. Must not be null.
 * \param out_view Receives borrowed buffer pointers and copied counts, point size and transforms.
 * Must not be null. The view does not extend the report or runtime-buffer lifetime.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_get_overlap_report_view(ovclash_suite_overlap_report_t* report,
                                                                         ovclash_suite_overlap_report_view_t* out_view);

/**
 * Destroys an overlap report handle and invalidates views that reference its buffers.
 * The runtime that created the report must still be alive.
 *
 * \param report Report handle returned by ovclash_suite_create_overlap_report. May be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_destroy_overlap_report(ovclash_suite_overlap_report_t* report);

/**
 * Builds suite-level clash info and frame info records for one overlap record.
 *
 * num_records of the clash info is the engine record count of the whole overlap group, not the
 * number of records that can be reported. ovclash_suite_runtime_save_overlap_to_data persists the
 * surviving count instead, so the two disagree for an overlap that loses records. The report is
 * created with FACES0, FACES1 and OUTLINE for a mesh pair, and with POINTS0 for a point-cloud
 * pair. The returned report and the borrowed frame buffers follow the runtime and report lifetime
 * rules of ovclash_suite_create_overlap_report.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null.
 * \param overlap_index Zero-based overlap group index.
 * \param record_index Zero-based record/frame index inside the overlap group.
 * \param query_id Query identifier to store in the returned clash info.
 * \param tolerance Tolerance recorded in the returned clash info.
 * \param depth_epsilon Depth epsilon recorded in the returned clash info.
 * \param out_clash_info Receives the clash info view. Must not be null. Its strings are
 * runtime-owned: they stay valid after the report is destroyed, but only until the next
 * ovclash_suite_create_clash_info, ovclash_suite_runtime_save_overlap_to_data or fetch-and-save
 * capture on the same runtime, whether or not that call succeeds, or until the runtime is
 * destroyed. Copy them to keep them longer.
 * \param out_frame_info Receives the frame info view. Its buffers are borrowed through out_report
 * and follow the lifetime rules of ovclash_suite_create_overlap_report. Must not be null.
 * \param out_report Receives the report associated with borrowed frame buffers. Must not be null and must be
 * destroyed with ovclash_suite_destroy_overlap_report.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when the clash-info views are populated.
 *         OVCLASH_SUITE_STATUS_UNAVAILABLE when a valid record has no available report; the
 *         output views are then empty and out_report is null. Only this status permits skipping
 *         a frame. OVCLASH_SUITE_STATUS_ERROR when the runtime, an output pointer, a backend
 *         index, the report buffers or an allocation fails. ovclash_suite_get_last_error(runtime)
 *         and ovclash_suite_get_thread_last_error return the failure detail.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_create_clash_info(ovclash_suite_runtime_t* runtime,
                                uint32_t overlap_index,
                                uint32_t record_index,
                                int64_t query_id,
                                double tolerance,
                                double depth_epsilon,
                                ovclash_suite_clash_info_view_t* out_clash_info,
                                ovclash_suite_clash_frame_info_view_t* out_frame_info,
                                ovclash_suite_overlap_report_t** out_report);

/**
 * Processes and persists one runtime overlap directly into a suite data store.
 *
 * It persists one overlap through the fetch-and-save engine, without approval comparison or
 * reconciliation. It keeps the editable metadata of an existing overlap with
 * the same query_id and overlap id, and updates runtime-owned fields from the current clash
 * result. It replaces frame rows in bulk, and commits explicitly like the data-store APIs.
 *
 * A record that the engine still resolves but cannot produce an overlap report for is dropped,
 * as in the Kit extension, rather than failing the whole overlap. A record index that the engine
 * does not know still fails the save, and an overlap whose records all fail to report is an
 * error. The persisted overlap therefore describes only the surviving records. Its num_records
 * and *out_frame_count are the surviving count, and its start_time and end_time are the sample
 * times of the first and last surviving record. An *out_frame_count below the engine record
 * count of the overlap (nb_records from ovclash_suite_get_overlap_data) is thus a legitimate
 * SUCCESS outcome. A caller that needs to know whether records were lost compares the two
 * itself. ovclash_suite_create_clash_info reports the engine count in its num_records, so the two
 * APIs disagree about the same overlap once a record is dropped.
 *
 * \param runtime Runtime handle returned by ovclash_suite_create_runtime. Must not be null and must own
 * completed results. Rejected while an async pipeline or a fetch is alive on it.
 * \param data Suite data handle created by either data factory. Must not be null and must be writable.
 * Rejected while a fetch is alive on it.
 * \param overlap_index Zero-based overlap group index in runtime results.
 * \param query_id Query identifier to assign to the persisted overlap.
 * \param tolerance Tolerance stored on the persisted overlap.
 * \param depth_epsilon Depth epsilon stored on the persisted overlap.
 * \param commit Non-zero to commit after this overlap. Zero leaves the serializer transaction open
 * for a later commit.
 * \param out_identifier Receives the persisted overlap identifier. Must not be null.
 * \param out_frame_count Receives the number of persisted frame records, which can be fewer than
 * the overlap's engine record count. Must not be null.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS on success; OVCLASH_SUITE_STATUS_ERROR when arguments, runtime results,
 *         report buffers, data store state, or storage writes fail. Failure text is available through
 *         ovclash_suite_get_last_error(runtime), ovclash_suite_data_get_last_error(data), or
 *         ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_runtime_save_overlap_to_data(ovclash_suite_runtime_t* runtime,
                                                                              ovclash_suite_data_t* data,
                                                                              uint32_t overlap_index,
                                                                              int64_t query_id,
                                                                              double tolerance,
                                                                              double depth_epsilon,
                                                                              uint32_t commit,
                                                                              int64_t* out_identifier,
                                                                              uint32_t* out_frame_count);

/**
 * Begins incremental fetching, serialization and saving of the runtime's completed results into a query.
 *
 * Pass a null serializer to use the built-in SQLite or directory serializer of the live data
 * handle. For a custom destination, pass a serializer callback table and null data. Exactly one of
 * data and serializer must be non-null; both or neither returns an error. The custom table is
 * copied, and its user_data and destination must outlive the operation. The required callbacks
 * must be non-null, and prepare_frame and release_frame must be both null or both set. See the
 * callback contract in ovclash_suite_serializer.h.
 *
 * Threading: the operation uses the runtime threading settings single_threaded,
 * new_task_manager and nb_tasks: the applied ones or, before any successful
 * ovclash_suite_set_run_settings, the engine defaults. Frame preparation of the directory
 * serializer and of an optional custom serializer captures these settings at begin and may use
 * workers. Built-in SQLite frame encoding stays on the driving thread. Report extraction reads the
 * live runtime settings, so set threading before begin and leave it unchanged until destroy.
 * Serializer writes and report acquisition are serialized. On ovstage, face and outline
 * preparation can use workers on immutable captured inputs. Every started worker drains before
 * prepared buffers are published, or released after a failure or cancellation. Report release and
 * SQLite frame encoding stay on the driving thread. last_modified_by uses the user name captured
 * once at begin, refreshed for each new operation. No fetch work runs between calls.
 *
 * Reservation: the operation reserves its runtime and built-in destination until it is
 * destroyed, also after completion or failure. In that time, runtime destruction, reset and
 * settings changes are rejected, as are destination writes, reopen, save-as, close and
 * destruction. A custom destination must enforce its own lifetime and connection stability.
 * Serialize access to these handles; the reservation does not make concurrent API calls safe.
 *
 * States: existing overlap identifiers and annotations are kept. With resolve_stale nonzero,
 * re-detected normal and duplicate overlaps become ACTIVE, except an APPROVED clash whose
 * detected contents are exactly unchanged. Built-in data handles supply the comparison reader
 * automatically. A custom serializer without a reader conservatively sets re-detected clashes to
 * ACTIVE. The comparison covers type, tolerance, depth epsilon, object paths and mesh checksums,
 * aggregate results, and the complete ordered frame contents (times, matrices, metrics and
 * geometry). Floating-point equality uses no epsilon and ignores the denormal controls of the
 * caller. Signed zeros compare equal, while NaN never proves unchanged contents. Stored overlap and
 * frame row identifiers, review fields, timestamps and user penetration-depth annotations are
 * excluded. The query and the ordered pair hash establish result identity; a matching overlap
 * hash alone does not prove unchanged contents. With resolve_stale zero, stored states are kept.
 * New overlaps start NEW and are not promoted during their first fetch. Absent results are marked
 * not present in either mode, and become RESOLVED when resolve_stale is nonzero.
 *
 * Transactions: after capture, the reconciliation of absent results can span several next
 * calls, and its changes stay pending until the final commit. Destroying an unfinished operation
 * performs no further reconciliation. It keeps completed writes and earlier reconciliation
 * changes, unless the cleanup of an incomplete overlap fails and requires a full rollback.
 * Fetch-and-save is not all-or-nothing; ovclash_suite_runtime_fetch_and_save_next describes the
 * rollback rules. The storage transaction belongs to the data handle, so completion or a full
 * rollback also affects unrelated writes already pending on that handle. An initialization
 * failure does not roll back those earlier writes.
 *
 * \param runtime Runtime with completed results. Must not be null or have an active async pipeline.
 * \param data Live built-in destination when serializer is null; otherwise must be null.
 * \param serializer Optional custom serializer. Null selects the data handle's storage backend.
 * \param query_id Identifier of the owning stored query.
 * \param tolerance Tolerance recorded on normal overlaps; must be finite.
 * \param depth_epsilon Depth epsilon recorded on normal overlaps; must be finite.
 * \param include_duplicates Nonzero to persist duplicate results. Pass the setting of the owning
 *        query; it does not depend on runtime settings changed after detection.
 * \param resolve_stale Nonzero to resolve absent clashes and set re-detected normal and duplicate
 *        overlaps to ACTIVE, preserving APPROVED only when detected contents are exactly unchanged.
 *        Zero preserves stored states.
 * \param out_fetch Receives an owned operation; cleared on failure. Must not be null.
 * \return Success after initialization, or error for invalid arguments or destination reads.
 *         Error text is available through ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_runtime_fetch_and_save_begin(ovclash_suite_runtime_t* runtime,
                                           ovclash_suite_data_t* data,
                                           const ovclash_suite_fetch_serializer_t* serializer,
                                           int64_t query_id,
                                           double tolerance,
                                           double depth_epsilon,
                                           uint32_t include_duplicates,
                                           uint32_t resolve_stale,
                                           ovclash_suite_fetch_and_save_t** out_fetch);

/**
 * Begins fetching with an optional saved-data reader for a custom serializer.
 *
 * Follows the lifetime, transaction and state rules of fetch_and_save_begin. A reader enables
 * the exact comparison of approved overlaps before replacement. Built-in data supplies its own
 * reader, so pass a null reader with a built-in destination. With resolve_stale nonzero and no
 * custom reader, unchanged content cannot be proven, and re-detected approved custom overlaps
 * become ACTIVE. The reader table is copied, and both callbacks are required when it is
 * supplied. Its user_data and saved destination must outlive the operation. The reader runs on
 * the driving thread and must return the complete detected header and the ordered saved frame
 * payload. Its optional close callback releases saved-reader resources on that same thread, also
 * when an unfinished operation is destroyed. Drive and destroy synchronous fetching on a thread
 * that suits the custom reader resources. A failed close may be retried during cleanup. See
 * ovclash_suite_fetch_saved_data_reader_t for the lifetimes of callback outputs.
 *
 * \param runtime Runtime with completed results. Must not be null or have an active async pipeline.
 * \param data Live built-in destination when serializer is null; otherwise must be null.
 * \param serializer Optional custom serializer. Null selects the data handle's storage backend.
 * \param reader Optional custom saved-data reader. Must be null with a built-in data handle.
 * \param query_id Identifier of the owning stored query.
 * \param tolerance Tolerance recorded on normal overlaps; must be finite.
 * \param depth_epsilon Depth epsilon recorded on normal overlaps; must be finite.
 * \param include_duplicates Nonzero to persist the owning query's duplicate results.
 * \param resolve_stale Nonzero to update states, retaining APPROVED only for exactly unchanged
 *        detected contents. Zero preserves stored states.
 * \param out_fetch Receives an owned operation; cleared on failure. Must not be null.
 * \return Success after initialization, or error for invalid arguments or destination reads.
 *         Error text is available through ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_runtime_fetch_and_save_begin_with_reader(ovclash_suite_runtime_t* runtime,
                                                       ovclash_suite_data_t* data,
                                                       const ovclash_suite_fetch_serializer_t* serializer,
                                                       const ovclash_suite_fetch_saved_data_reader_t* reader,
                                                       int64_t query_id,
                                                       double tolerance,
                                                       double depth_epsilon,
                                                       uint32_t include_duplicates,
                                                       uint32_t resolve_stale,
                                                       ovclash_suite_fetch_and_save_t** out_fetch);

/**
 * Fetches, serializes and saves the next part of the work. Each call attempts at most 1024
 * runtime frame reports, across approval comparison and capture, and advances at most 1024
 * overlaps. It also stops when the acquired runtime report payload reaches 8 MiB; one indivisible
 * report may exceed that threshold. A long overlap resumes on a later call, with its savepoint
 * kept open. Reconciliation scans at most 1024 stored identifiers per call. These are work
 * bounds, not wall-clock guarantees; begin enumerates all existing identifiers. No borrowed report
 * or active preparation task survives a return; serializer scratch allocations may be reused.
 * Serialize access to the operation, runtime and destination; reentrant next and destroy calls are
 * rejected.
 *
 * Failure and transaction behavior, identical in single-threaded and parallel modes:
 * - A serializer callback that reports failure during saved-result reads, writes, reconciliation
 *   or transaction handling requests a ROLLBACK of the entire pending transaction. That includes
 *   the batches of earlier calls and any other uncommitted writes on the destination. Data
 *   committed earlier is kept.
 * - Extraction, frame preparation, scheduling or other native processing failures keep the
 *   pending overlaps completed earlier. ABORT_OVERLAP restores an incomplete streamed overlap. When
 *   that cleanup reports failure, the engine requests a full ROLLBACK instead.
 * - Completion reconciles absent overlaps and calls the idempotent BEGIN before COMMIT, even for
 *   empty results, which publishes other pending writes too. Stopping between calls leaves
 *   completed writes pending. Destroying the operation restores an unfinished overlap. It keeps
 *   completed writes, unless ABORT_OVERLAP fails and cleanup must fall back to a full ROLLBACK.
 *
 * Custom serializers must implement these transaction callbacks; the engine cannot guarantee
 * recovery when their rollback fails. Closing or destroying built-in data attempts to commit the
 * remaining pending writes. Cleanup after an extraction or preparation failure, or after
 * abandonment, can therefore persist partial results.
 *
 * A failed operation must be destroyed; it cannot be resumed.
 *
 * \param fetch Operation returned by ovclash_suite_runtime_fetch_and_save_begin. Must not be null.
 * \param out_progress Receives monotone progress in [0,1]. Must not be null.
 * \param out_complete Receives 1 after reconciliation and commit, otherwise 0. Must not be null.
 * \return Success after advancing, or error for invalid arguments, extraction, encoding or storage
 *         failures. Error text is available through ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_runtime_fetch_and_save_next(ovclash_suite_fetch_and_save_t* fetch,
                                                                             double* out_progress,
                                                                             uint32_t* out_complete);

/**
 * Reads the exact counts of this operation, before its first step or after a successful step.
 *
 * The counts include normal overlap groups and the duplicates selected at begin, not frames or
 * destination rows that existed before. An incomplete streamed overlap does not count as written.
 * Written results stay pending until next reports completion after reconciliation and commit, so
 * written == total alone does not imply a commit. A later failure may roll pending writes back.
 * The call performs no work. Serialize access as for next. Failed or executing operations are
 * rejected.
 *
 * \param fetch Live operation returned by begin. Must not be null, failed or executing callbacks.
 * \param out_written Receives the number of fully written result groups. Must not be null.
 * \param out_total Receives the number of result groups selected at begin. Must not be null.
 * \return Success with both counts, or error for invalid arguments or operation state.
 *         Error text is available through ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_runtime_fetch_and_save_get_counts(
    ovclash_suite_fetch_and_save_t* fetch, uint64_t* out_written, uint64_t* out_total);

/**
 * Destroys an operation and releases its reservation of the runtime and destination.
 *
 * Null is accepted. An unfinished overlap is aborted. Completed writes stay pending, unless the
 * abort fails and requires a full rollback. Destroy the operation before its runtime or
 * destination. Unlike this destructor, closing or destroying built-in data attempts to commit
 * pending writes, including results kept after a fetch failure. A cleanup failure returns an
 * error and still consumes the operation, so do not retry the destruction. A failed overlap abort
 * is reported even when the fallback rollback succeeds. The one exception is a rejected
 * reentrant destroy from a running callback, which leaves the operation alive.
 *
 * \param fetch Operation to consume. May be null. Destroy after success, failure or abandonment.
 * \return Success after release, or error for cleanup failure or rejected reentrant destruction.
 *         Read ovclash_suite_get_thread_last_error for the diagnostic.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_runtime_fetch_and_save_destroy(ovclash_suite_fetch_and_save_t* fetch);

/**
 * Starts fetch and save on one background worker, including serializer initialization and cleanup.
 *
 * This is the asynchronous equivalent of the bounded fetch-and-save API. Serializer writes stay
 * sequential; eligible frame preparation may use the existing parallel scheduler of the runtime.
 * The runtime and the built-in destination are leased exclusively until the returned handle is
 * destroyed, also after finish or cancel. Calls from other threads cannot access the state of
 * either owner, and the caller receives the rejection diagnostics. Native driver callbacks keep
 * the read access that capture needs; preparation workers do not inherit it. Owner mutations
 * still reject the lease. Finish or destroy the detection pipeline handle before launching this
 * operation.
 *
 * A custom serializer callback table is copied, but its user_data and the objects it references
 * must stay alive until destruction. Callbacks run on the background worker, and eligible
 * preparation callbacks can run on scheduler workers. Choosing this API opts those callbacks into
 * background execution. The synchronous bounded API keeps running callbacks on the calling
 * thread.
 *
 * \param runtime Runtime containing completed detection results. Must not be null.
 * \param data Built-in destination, or null when a custom serializer is supplied.
 * \param serializer Optional custom serializer callbacks. The built-in destination is used when null.
 * \param query_id Identifier of the owning stored query.
 * \param tolerance Tolerance recorded on normal overlaps; must be finite.
 * \param depth_epsilon Depth epsilon recorded on normal overlaps; must be finite.
 * \param include_duplicates Non-zero to persist duplicate results too.
 * \param resolve_stale Non-zero to reconcile stale overlap records after successful fetching.
 * \param out_async Receives the owned operation handle, or null on launch failure. Must not be null.
 * \return SUCCESS when launched; ERROR for invalid arguments, busy owners, live destination cursors,
 *         or launch failure. Worker failures are reported by finish and the operation error accessor.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_runtime_fetch_and_save_async(ovclash_suite_runtime_t* runtime,
                                           ovclash_suite_data_t* data,
                                           const ovclash_suite_fetch_serializer_t* serializer,
                                           int64_t query_id,
                                           double tolerance,
                                           double depth_epsilon,
                                           uint32_t include_duplicates,
                                           uint32_t resolve_stale,
                                           ovclash_suite_fetch_async_t** out_async);

/**
 * Starts background fetch and save with a custom saved-history reader.
 *
 * The ownership, synchronization and cleanup contracts of
 * ovclash_suite_runtime_fetch_and_save_async also apply here. A non-null reader requires a custom
 * serializer and both read callbacks. Its table is copied before launch; user_data and the
 * objects it references must stay alive until the async handle is destroyed. Saved-history reads
 * run sequentially on the background worker before an overlap is replaced. The optional
 * saved-reader close also runs on that worker, before the terminal outcome is published, also on
 * cancellation and destruction. A close failure is reported without replacing an earlier
 * processing diagnostic. A custom reader that keeps failing owns its remaining resources. With
 * resolve_stale enabled, exactly unchanged APPROVED clashes keep their state, changed detected
 * clashes become ACTIVE, and absent clashes become RESOLVED. With resolve_stale disabled, stored
 * states are kept and saved-history reads are skipped.
 *
 * \param runtime Runtime containing completed detection results. Must not be null.
 * \param data Built-in destination, or null when a custom serializer is supplied.
 * \param serializer Optional custom serializer callbacks.
 * \param reader Optional custom saved-history callbacks; null retains the reader-less behavior.
 * \param query_id Identifier of the owning stored query.
 * \param tolerance Tolerance recorded on normal overlaps; must be finite.
 * \param depth_epsilon Depth epsilon recorded on normal overlaps; must be finite.
 * \param include_duplicates Nonzero to persist duplicate results too.
 * \param resolve_stale Nonzero to reconcile clash states after successful fetching.
 * \param out_async Receives the owned handle, or null on launch failure. Must not be null.
 * \return SUCCESS when launched; ERROR for invalid arguments, busy owners, live destination cursors,
 *         or launch failure. Worker failures are reported by finish and the operation error accessor.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_runtime_fetch_and_save_async_with_reader(ovclash_suite_runtime_t* runtime,
                                                       ovclash_suite_data_t* data,
                                                       const ovclash_suite_fetch_serializer_t* serializer,
                                                       const ovclash_suite_fetch_saved_data_reader_t* reader,
                                                       int64_t query_id,
                                                       double tolerance,
                                                       double depth_epsilon,
                                                       uint32_t include_duplicates,
                                                       uint32_t resolve_stale,
                                                       ovclash_suite_fetch_async_t** out_async);

/**
 * Reads a synchronized running snapshot.
 *
 * Zero means that the worker and its cleanup have completed. Polling may overlap one finish or
 * cancel call. Serialize control calls with each other; destruction must not overlap any call on
 * the handle. The handle stays valid until it is explicitly destroyed.
 *
 * \param async Owned async fetch handle. Must not be null.
 * \param out_running Receives non-zero while the worker runs. Must not be null.
 * \return SUCCESS for a valid snapshot, or ERROR for invalid arguments.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_is_running(ovclash_suite_fetch_async_t* async,
                                                                        uint32_t* out_running);

/**
 * Reads a synchronized progress snapshot.
 *
 * name is static storage. step_index names one of three phases: initialization, fetching, and
 * reconciliation and commit; nb_steps is always 3. progress is the progress of the whole
 * operation in [0, 1], independent of the phase index; only a successful committed completion
 * reaches 1. finished is non-zero exactly when ovclash_suite_fetch_async_is_running would report
 * zero: after success, failure or cancellation in any phase. step_index then still names the phase
 * in which the operation stopped. Polling may overlap one finish or cancel call; destruction must
 * not overlap any call on the handle.
 *
 * \param async Owned async fetch handle. Must not be null.
 * \param out_step_data Receives the snapshot, including finished state. Must not be null.
 * \return SUCCESS for a valid snapshot, or ERROR for invalid arguments.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_get_step_data(ovclash_suite_fetch_async_t* async,
                                                                           ovclash_suite_step_data_t* out_step_data);

/**
 * Reads the synchronized written and total counters.
 *
 * out_initialized is zero until initialization succeeds. An initialized total of zero denotes an
 * empty result set. Output pointers must not be null. Polling may overlap one finish or cancel
 * call; destruction must not overlap any call on the handle.
 *
 * \param async Owned async fetch handle. Must not be null.
 * \param out_written Receives completed overlap and duplicate records.
 * \param out_total Receives the total overlap and duplicate record count.
 * \param out_initialized Receives non-zero once initialization succeeds.
 * \return SUCCESS for a valid snapshot, or ERROR for invalid arguments.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_get_counts(ovclash_suite_fetch_async_t* async,
                                                                        uint64_t* out_written,
                                                                        uint64_t* out_total,
                                                                        uint32_t* out_initialized);

/**
 * Reads the diagnostic of the operation.
 *
 * The string storage stays valid until the handle is destroyed. Polling may overlap one finish or
 * cancel call; destruction must not overlap any call on the handle.
 *
 * \param async Owned async fetch handle. Must not be null.
 * \param out_error Receives an empty string while running, or the terminal diagnostic. Must not be null.
 * \return SUCCESS for a valid snapshot, or ERROR for invalid arguments.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_get_last_error(ovclash_suite_fetch_async_t* async,
                                                                            ovclash_suite_string_t* out_error);

/**
 * Waits for the worker to complete and returns its outcome.
 *
 * Failures are published on the calling thread. Repeated calls return the same outcome. The owner
 * lease stays held until the handle is destroyed.
 *
 * \param async Owned async fetch handle. Must not be null; worker callback reentry is rejected.
 * \return SUCCESS after committed completion; ERROR after cancellation, processing/cleanup failure,
 *         or invalid/reentrant use. Read the operation error accessor for terminal failure detail.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_finish(ovclash_suite_fetch_async_t* async);

/**
 * Requests cooperative cancellation and waits for cleanup.
 *
 * Completed overlap records keep the partial-write semantics of the bounded API; an incomplete
 * current overlap is aborted. A successful cancellation cleanup returns success, and a later
 * finish reports the cancellation outcome. A cleanup failure returns an error. A call after
 * completion is harmless.
 *
 * \param async Owned async fetch handle. Must not be null; worker callback reentry is rejected.
 * \return SUCCESS after clean cancellation or successful completion; ERROR for processing/cleanup
 *         failure or invalid/reentrant use. Read the operation error accessor for failure detail.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_cancel(ovclash_suite_fetch_async_t* async);

/**
 * Cancels and drains unfinished work, releases both owner leases, and consumes the handle.
 *
 * The handle is consumed even when processing or cleanup failed. Destruction from the worker
 * callbacks of the operation is rejected and leaves the handle alive, because those callbacks
 * cannot join themselves. A failed wait also keeps ownership. Null is harmless. A foreign-language
 * caller that can be interrupted while it handles the return can use
 * ovclash_suite_fetch_async_destroy_owned to observe the consumption.
 *
 * \param async Owned async fetch handle to consume, or null.
 * \return SUCCESS after clean cancellation/completion or for null; ERROR for processing/cleanup
 *         failure or reentrant use. When a consumed handle failed, the detail is published to the
 *         thread-local last error of the caller.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_destroy(ovclash_suite_fetch_async_t* async);

/**
 * Cancels and drains an owned handle, and publishes the consumption through the caller-owned
 * pointer.
 *
 * Once draining succeeds and destruction is accepted, sets *async to null before it releases the
 * handle or publishes its terminal diagnostic, also when processing or cleanup failed. A rejected
 * callback reentry or a failed wait leaves *async unchanged for a later retry. A pointer to a null
 * handle succeeds. Serialize access to the pointer and the handle; destruction must not overlap
 * another call on that handle.
 *
 * \param async Caller-owned handle pointer to consume. Must not be null; *async may be null.
 * \return SUCCESS after clean cancellation/completion or for a null handle; ERROR for invalid
 *         arguments, processing/cleanup failure, a failed wait, or reentrant use. Read *async to
 *         distinguish accepted consumption from rejection before retrying destruction.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_fetch_async_destroy_owned(ovclash_suite_fetch_async_t** async);

/**
 * Formats a 128-bit integer as the hexadecimal string used by clash identifiers.
 *
 * The text is 0x followed by lowercase hexadecimal digits without leading zeros, for example 0x0
 * for zero. The locale does not affect it.
 *
 * \param hi High 64 bits of the integer.
 * \param lo Low 64 bits of the integer.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_format_int128_hex(
    uint64_t hi, uint64_t lo, char* out_buffer, size_t out_buffer_size, size_t* out_required_size);

/**
 * Escapes text for HTML output using the suite export escaping rules.
 *
 * Replaces the ampersand, less-than, greater-than, double-quote and apostrophe characters with
 * HTML character references; every other byte is copied unchanged.
 *
 * \param text Length-prefixed text to escape.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_html_escape(ovclash_suite_string_t text,
                                                             char* out_buffer,
                                                             size_t out_buffer_size,
                                                             size_t* out_required_size);

/**
 * Returns the current timestamp using the suite storage timestamp format.
 *
 * The text is the local time as YYYY-MM-DD HH:MM:SS.ffffff, with microseconds.
 *
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_current_timestamp(char* out_buffer,
                                                                   size_t out_buffer_size,
                                                                   size_t* out_required_size);

/**
 * Returns the current user name used for clash metadata.
 *
 * Resolved from LOGNAME, USER, LNAME and USERNAME, then from the OS account: the password database
 * entry of the real user ID on POSIX, GetUserNameW on Windows. A value that is not valid UTF-8 is
 * skipped. The name is empty when no value qualifies.
 *
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_current_user_name(char* out_buffer,
                                                                   size_t out_buffer_size,
                                                                   size_t* out_required_size);

/**
 * Checks whether path names a regular file, following symbolic links (os.path.isfile parity).
 *
 * \param path Length-prefixed filesystem path to test.
 * \param out_exists Receives 1 for a regular file, 0 for an empty or missing path, a directory,
 *                   another file type, or a path containing an embedded NUL character. Must not
 *                   be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_file_exists(ovclash_suite_string_t path, uint32_t* out_exists);

/**
 * Copies a regular file, or recreates a symbolic link, with shutil.copy semantics.
 *
 * When target_path names an existing directory, the copy is written into it under the file name
 * of the source. An existing target file is overwritten. The permission bits of the source are
 * then applied to the target. With follow_symlinks zero and a symbolic-link source, the link
 * itself is recreated at the target, which fails when the target already exists. Copy failures
 * are reported through out_copied, not through the result code.
 *
 * \param source_path Length-prefixed source filesystem path.
 * \param target_path Length-prefixed target filesystem path.
 * \param follow_symlinks Non-zero to copy the linked target when source_path is a symbolic link.
 * \param out_copied Receives 1 when the copy and the permission-bit copy both succeeded, otherwise
 *                   0: for an empty path, a missing or directory source, a failed copy, or a
 *                   failed permission copy, which leaves the target already written. Must not be
 *                   null.
 *
 * \return Failure when out_copied is null or a path is an invalid span, is not valid UTF-8 or
 *         contains an embedded NUL character; nothing is copied then.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_copy_file(ovclash_suite_string_t source_path,
                                                           ovclash_suite_string_t target_path,
                                                           uint32_t follow_symlinks,
                                                           uint32_t* out_copied);

/**
 * Deletes a non-directory filesystem entry without following symbolic links, with os.remove
 * semantics: a symbolic link is removed, never its target.
 *
 * \param path Length-prefixed filesystem path to delete.
 * \param out_deleted Receives 1 when an entry was removed, otherwise 0: for an empty or missing
 *                    path, a directory, or a failed removal. Must not be null.
 *
 * \return Failure when out_deleted is null or path is an invalid span, is not valid UTF-8 or
 *         contains an embedded NUL character; nothing is deleted then.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_delete_file(ovclash_suite_string_t path, uint32_t* out_deleted);

/**
 * Probes whether a file is a legacy `.clashDetection` container.
 *
 * The legacy container format starts with a 16-byte clashDetection header, followed by a SQLite
 * database payload. This function only probes the format; it neither opens the SQLite database
 * nor migrates schemas.
 *
 * \param file_path Length-prefixed filesystem path to inspect. ptr may be null only when length is zero.
 * \param out_is_legacy_container Receives 1 when the file starts with the legacy clashDetection magic, otherwise 0.
 *                                Must not be null.
 * \param out_has_sqlite_payload Receives 1 when the file is a legacy container and the embedded payload starts with
 *                               a SQLite database header, otherwise 0. Must not be null.
 *
 * \return Success when the probe completed. Missing files, empty paths, short files and
 *         non-legacy files are reported through zero output values. Failure is reserved for
 *         invalid arguments, an invalid string span, invalid UTF-8, a path containing an embedded
 *         NUL character, or a caught exception.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_probe_legacy_clash_detection_file(ovclash_suite_string_t file_path,
                                                                                   uint32_t* out_is_legacy_container,
                                                                                   uint32_t* out_has_sqlite_payload);

/**
 * Exports tabular rows to JSON.
 *
 * \param columns Column definitions, emitted in array order; each entry's order selects its cell within a row.
 *                May be null when column_count is zero.
 * \param column_count Number of entries in columns.
 * \param row_values Flattened row-major cell values. May be null only when row_value_count is zero.
 * \param row_value_count Number of entries in row_values. Must be at least row_count * column_count.
 * \param row_count Number of rows represented by row_values.
 * \param additional_info Optional name/value metadata entries. May be null when additional_info_count is zero.
 * \param additional_info_count Number of entries in additional_info.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 *
 * Error behavior: fails when an input pointer and count pair is inconsistent, when an input
 * string span has a null ptr with a non-zero length, or when a column name, row value or
 * additional-info name or value is not valid UTF-8. It also fails when a column order is out of
 * range for column_count, when row_value_count cannot cover row_count * column_count without
 * overflow, or when a non-null out_buffer is too small. A null out_buffer with size zero remains
 * the supported size query and succeeds when the export inputs are valid.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_export_rows_json(const ovclash_suite_export_column_t* columns,
                                                                  size_t column_count,
                                                                  const ovclash_suite_string_t* row_values,
                                                                  size_t row_value_count,
                                                                  size_t row_count,
                                                                  const ovclash_suite_export_info_t* additional_info,
                                                                  size_t additional_info_count,
                                                                  char* out_buffer,
                                                                  size_t out_buffer_size,
                                                                  size_t* out_required_size);

/**
 * Exports tabular rows to HTML.
 *
 * \param title Length-prefixed document title.
 * \param subtitle Length-prefixed document subtitle.
 * \param columns Column definitions, emitted in array order; each entry's order selects its cell within a row.
 *                May be null when column_count is zero.
 * \param column_count Number of entries in columns.
 * \param row_values Flattened row-major cell values. May be null only when row_value_count is zero.
 * \param row_value_count Number of entries in row_values. Must be at least row_count * column_count.
 * \param row_count Number of rows represented by row_values.
 * \param additional_info Optional name/value metadata entries. May be null when additional_info_count is zero.
 * \param additional_info_count Number of entries in additional_info.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero to query size.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when a buffer is supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 *
 * Error behavior: fails when an input pointer and count pair is inconsistent, when an input
 * string span has a null ptr with a non-zero length, or when the title, the subtitle, a column
 * name, a row value or an additional-info name or value is not valid UTF-8. It also fails when a
 * column order is out of range for column_count, when row_value_count cannot cover
 * row_count * column_count without overflow, or when a non-null out_buffer is too small. A null
 * out_buffer with size zero remains the supported size query and succeeds when the export inputs
 * are valid.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_export_rows_html(ovclash_suite_string_t title,
                                                                  ovclash_suite_string_t subtitle,
                                                                  const ovclash_suite_export_column_t* columns,
                                                                  size_t column_count,
                                                                  const ovclash_suite_string_t* row_values,
                                                                  size_t row_value_count,
                                                                  size_t row_count,
                                                                  const ovclash_suite_export_info_t* additional_info,
                                                                  size_t additional_info_count,
                                                                  char* out_buffer,
                                                                  size_t out_buffer_size,
                                                                  size_t* out_required_size);

/**
 * Exports tabular rows to JSON and returns an owned immutable text handle.
 *
 * \param columns Column definitions, emitted in array order; each entry's order selects its cell within a row.
 *                May be null when column_count is zero.
 * \param column_count Number of entries in columns.
 * \param row_values Flattened row-major cell values. May be null only when row_value_count is zero.
 * \param row_value_count Number of entries in row_values. Must be at least row_count * column_count.
 * \param row_count Number of rows represented by row_values.
 * \param additional_info Optional name/value metadata entries. May be null when additional_info_count is zero.
 * \param additional_info_count Number of entries in additional_info.
 * \param out_text Receives an owned export text handle. Must not be null and must be destroyed with
 * ovclash_suite_export_text_destroy.
 *
 * Error behavior: identical to ovclash_suite_export_rows_json, except no caller buffer is used.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_export_rows_json_text(const ovclash_suite_export_column_t* columns,
                                    size_t column_count,
                                    const ovclash_suite_string_t* row_values,
                                    size_t row_value_count,
                                    size_t row_count,
                                    const ovclash_suite_export_info_t* additional_info,
                                    size_t additional_info_count,
                                    ovclash_suite_export_text_t** out_text);

/**
 * Exports tabular rows to HTML and returns an owned immutable text handle.
 *
 * \param title Length-prefixed document title.
 * \param subtitle Length-prefixed document subtitle.
 * \param columns Column definitions, emitted in array order; each entry's order selects its cell within a row.
 *                May be null when column_count is zero.
 * \param column_count Number of entries in columns.
 * \param row_values Flattened row-major cell values. May be null only when row_value_count is zero.
 * \param row_value_count Number of entries in row_values. Must be at least row_count * column_count.
 * \param row_count Number of rows represented by row_values.
 * \param additional_info Optional name/value metadata entries. May be null when additional_info_count is zero.
 * \param additional_info_count Number of entries in additional_info.
 * \param out_text Receives an owned export text handle. Must not be null and must be destroyed with
 * ovclash_suite_export_text_destroy.
 *
 * Error behavior: identical to ovclash_suite_export_rows_html, except no caller buffer is used.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_export_rows_html_text(ovclash_suite_string_t title,
                                    ovclash_suite_string_t subtitle,
                                    const ovclash_suite_export_column_t* columns,
                                    size_t column_count,
                                    const ovclash_suite_string_t* row_values,
                                    size_t row_value_count,
                                    size_t row_count,
                                    const ovclash_suite_export_info_t* additional_info,
                                    size_t additional_info_count,
                                    ovclash_suite_export_text_t** out_text);

/**
 * Returns a borrowed UTF-8 view of an owned export text handle.
 *
 * \param text Export text handle returned by an *_text export function. Must not be null.
 * \param out_text Receives a borrowed byte span valid until text is destroyed. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_export_text_get_view(const ovclash_suite_export_text_t* text,
                                                                      ovclash_suite_string_t* out_text);

/**
 * Destroys an owned export text handle.
 *
 * \param text Export text handle returned by an *_text export function. May be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_export_text_destroy(ovclash_suite_export_text_t* text);

/** @} */
/**
 * \name Data Serialization API
 *
 * Per-function error catalogue for the data API: each data function returning
 * ovclash_suite_result_t returns OVCLASH_SUITE_STATUS_SUCCESS when the requested query, mutation,
 * migration or serialization step completed and the documented outputs are initialized. It
 * returns OVCLASH_SUITE_STATUS_ERROR when:
 * - the data handle is invalid, an output pointer is null, or a required store is not open;
 * - a string span is invalid or not UTF-8;
 * - a counted array or nested frame buffer is null with a non-zero count;
 * - a row, index or identifier is out of range;
 * - schema compatibility or migration fails, or a legacy `.clashDetection` container cannot be
 *   extracted or written;
 * - SQLite reports an error, or a filesystem operation fails;
 * - an allocation fails, or a C++ exception is caught.
 *
 * The handle-local ovclash_suite_data_get_last_error(data) and the thread-local
 * ovclash_suite_get_thread_last_error carry the concrete reason.
 *
 * Scalar overlap and frame values stored in SQLite REAL columns reject NaN with
 * OVCLASH_SUITE_STATUS_ERROR, rather than storing NULL and reading it back as zero. Infinities and
 * finite values are supported. SQLite REAL does not preserve the sign of zero. Frame matrices
 * still require finite values. Numeric BLOB arrays keep their bit-exact storage. Directory JSON
 * records encode NaN and the infinities as the string tags "NaN", "+Infinity" and "-Infinity",
 * default-time NaN values included, and keep the sign of a finite zero. Matrices require finite
 * values on both backends.
 *
 * Transactions: the storage transaction belongs to the data handle. Writes made with commit = 0
 * stay pending until ovclash_suite_data_commit, a later call with commit != 0, or close, which
 * commits them. A mutation that fails after its storage write began rolls back the whole pending
 * transaction, including earlier uncommitted writes that returned success; committed data is
 * kept. Such failures include the value checks that the store makes while writing: a NaN REAL
 * value on a SQLite store, a non-finite frame matrix on either store and, on a directory store, a
 * record string containing NUL. They also include constraint, SQLite and filesystem errors, and a
 * failed commit. Argument, open-state, fetch-reservation and not-found failures are reported
 * before the write begins and leave pending writes untouched. When earlier batched writes must
 * survive, validate the inputs, or commit, before a write that may fail.
 *
 * Threading: callers serialize all access to a shared data handle. Independent data handles may
 * be used concurrently, subject to SQLite and filesystem locking on the same database path. A
 * directory root permits one open handle at a time; opening the same root elsewhere fails until
 * the owning handle closes. For a deferred directory that does not exist yet, ownership begins at
 * the first write. A live fetch reserves its destination until it is destroyed, and rejects public
 * mutations. An asynchronous fetch also rejects reads of a built-in destination from every thread
 * but its worker. Live overlap and frame cursors, even exhausted or empty ones, prevent open, save,
 * save-as, migrate, close and destroy. Destroy dependent operations and cursors before changing
 * the store lifecycle.
 *
 * Ownership: the query, overlap, frame and point-cloud-settings getters that are not cursors
 * borrow the per-kind scratch record of the data handle. Copy a view before another getter of the
 * same kind, any mutation of the data handle, close or destroy. A cursor view belongs to its
 * cursor instead and lasts until the next advance or the destruction of that cursor. Insert,
 * update and replace functions copy all supplied views during the call.
 *
 * Example:
 * \code
 * ovclash_suite_data_t* data = 0;
 * if (ovclash_suite_data_create(&data).status == OVCLASH_SUITE_STATUS_SUCCESS) {
 *     ovclash_suite_data_open(data, (ovclash_suite_string_t){ "scene.ovclashdb", 15 });
 *     uint32_t query_count = 0;
 *     ovclash_suite_data_get_query_count(data, &query_count);
 *     ovclash_suite_data_destroy(data);
 * }
 * \endcode
 */
/** @{ */

/**
 * Creates a suite data/serialization handle backed by SQLite.
 *
 * \param out_data Receives the created data handle. Must not be null and must be destroyed with
 * ovclash_suite_data_destroy.
 *
 * \return Success when the handle is allocated. Failure when out_data is null,
 *         allocation fails, or an exception is caught.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_create(ovclash_suite_data_t** out_data);

/**
 * Creates a Suite data handle backed by a human-readable directory of JSON records.
 *
 * All data, cursor and runtime-save functions accept this handle. Open supplies the root
 * directory path; file name extensions have no special meaning for this backend. Each query,
 * point-cloud setting, overlap header and frame is stored separately. Existing SQLite stores keep
 * using ovclash_suite_data_create. A record string containing an embedded NUL is rejected on
 * write; an escaped NUL inside opaque query settings JSON is still supported. Symbolic links are
 * rejected in the normalized absolute root path and its ancestors, in record paths and in
 * reserved internal paths, also for save-as destinations. Callers may resolve trusted aliases
 * explicitly before supplying a path. Windows directory junctions and volume mount points count
 * as symbolic links, so a volume mounted only into a folder needs a drive letter to hold a store.
 * Unnamed stores also require a system temporary-directory path without symbolic-link components.
 *
 * \param out_data Receives the allocated handle. Must not be null; destroy it with
 *        ovclash_suite_data_destroy after releasing its cursors and fetch operations.
 * \return Success when the handle is allocated; failure for a null output or allocation failure.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_create_directory(ovclash_suite_data_t** out_data);

/**
 * Destroys a Suite data and serialization handle.
 *
 * Attempts to commit pending writes before it closes and releases the handle. This is not a
 * rollback operation: partial results kept after a failed or abandoned fetch can be saved.
 * Destroy any fetch operations and cursors first. A failed commit requests a rollback and returns
 * failure without freeing the data handle, so the caller can inspect it and clean up or retry.
 *
 * \param data Suite data handle created by either data factory. May be null.
 *
 * \return Success when the handle is released or data is null. Failure if a dependent
 *         operation/cursor is alive, commit/close fails, or cleanup throws. On failure the
 *         data handle remains owned by the caller; inspect its error and retry cleanup.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_destroy(ovclash_suite_data_t* data);

/**
 * Returns the last error message recorded on a data handle.
 *
 * While an async fetch owner is alive, returns the thread-local access-rejection diagnostic of
 * the caller without reading store state. ovclash_suite_fetch_async_get_last_error returns the
 * diagnostic of the operation. Destroy that owner before querying store state.
 *
 * \param data Data handle to query. May be null. A null handle, or one with no recorded
 *        error, falls back to the thread-local last error; the view is empty only when that is
 *        empty too.
 */
OVCLASH_API ovclash_suite_string_t ovclash_suite_data_get_last_error(const ovclash_suite_data_t* data);

/**
 * Returns the stored data path, a SQLite file or a directory root.
 *
 * The path can stay set after close or a failed open; ovclash_suite_data_is_open reports the
 * state. Unnamed stores return an empty path.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_file_path Receives a string view owned by the data handle, valid until the stored
 *        path changes or the handle is destroyed. Must not be null.
 *
 * \return Success when out_file_path is initialized. Failure follows the data
 *         API catalogue.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_file_path(const ovclash_suite_data_t* data,
                                                                    ovclash_suite_string_t* out_file_path);

/**
 * Returns the on-disk size of an open, named store.
 *
 * SQLite reports the size of its database file. Directory stores sum the regular files under the
 * root, including internal files and unrelated exports. Unnamed stores fail.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_file_size Receives the size in bytes. Must not be null.
 *
 * \return Success when the size is known. Failure when no named store is
 *         open, the filesystem query fails, or the data API catalogue applies.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_file_size(const ovclash_suite_data_t* data,
                                                                    uint64_t* out_file_size);

/**
 * Checks whether the opened data structures match the supported schema versions.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_compatible Receives 1 when the opened structures are compatible, otherwise 0. Must not be null.
 *
 * \return Success when compatibility state is reported. Failure follows the
 *         data API catalogue.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_structures_compatible(const ovclash_suite_data_t* data,
                                                                            uint32_t* out_compatible);

/**
 * Checks whether the opened data structures can be migrated to the latest supported schema.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_possible Receives 1 when migration is possible, otherwise 0. Must not be null.
 *
 * \return Success when migration-possible state is reported. Failure follows
 *         the data API catalogue.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_migration_to_latest_possible(const ovclash_suite_data_t* data,
                                                                                   uint32_t* out_possible);

/**
 * Queries whether file creation is deferred until the first write operation.
 *
 * A new data handle defers creation (1).
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_deferred Receives 1 when creation is deferred, otherwise 0. Must not be null.
 *
 * \return Success when the flag is reported. Failure follows the data API
 *         catalogue.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_deferred_file_creation_until_first_write_op(
    const ovclash_suite_data_t* data, uint32_t* out_deferred);

/**
 * Sets whether file creation is deferred until the first write operation.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param value Non-zero to defer file creation until first write, otherwise zero.
 *
 * \return Success when the flag is updated. Failure follows the data API
 *         catalogue.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_set_deferred_file_creation_until_first_write_op(ovclash_suite_data_t* data, uint32_t value);

/**
 * Opens or creates a suite data store.
 *
 * A SQLite handle treats a path ending in `.clashDetection`, or a file with a valid legacy
 * clashDetection header, as a legacy read container. It extracts the embedded SQLite payload to a
 * temporary `.ovclashdb` file and opens that through the normal SQLite backend. Open and commit
 * leave the original `.clashDetection` file unchanged. To write the legacy container format, pass
 * a `.clashDetection` target to ovclash_suite_data_save or ovclash_suite_data_save_as. After a
 * failed open of a `.clashDetection` path, a write fails rather than create a plain SQLite
 * database under the container name. A directory handle opens a directory of individual JSON
 * records, and suffixes such as `.clashDetection` have no special meaning for it. Neither backend
 * converts the format of the other.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param file_path Length-prefixed data file path. ptr may be null only when
 *                  length is zero.
 *
 * \return Success when the store is opened or deferred creation is active. Failure
 *         when arguments are invalid, the path string span is invalid, non-UTF-8
 *         or contains an embedded NUL character (rejected before the current store
 *         is touched), a SQLite handle's path names a directory, the legacy
 *         header is corrupt/unsupported, SQLite cannot be
 *         loaded/opened, or the schema is incompatible. Incompatible schema
 *         failures leave the handle not-open but preserve compatibility and
 *         migration-possible flags for callers that want to migrate explicitly.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_open(ovclash_suite_data_t* data,
                                                           ovclash_suite_string_t file_path);

/**
 * Checks whether the data handle has an open store.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_open Receives 1 when the data store is open, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_is_open(const ovclash_suite_data_t* data, uint32_t* out_open);

/**
 * Saves the current data store.
 *
 * SQLite handles write normal targets as `.ovclashdb` stores. A target ending in `.clashDetection`
 * is written in the legacy container format: a 16-byte clashDetection header followed by the
 * SQLite database bytes. Directory handles keep their JSON directory format for every target
 * path. A named target also persists an unnamed SQLite store, including its pending writes, and
 * becomes its current path. A legacy container target exports a snapshot and keeps the current
 * path.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param file_path Length-prefixed target file path, or an empty string to use the current path.
 *                  ptr may be null only when length is zero.
 *
 * \return Success when the current store is saved. Failure when arguments are
 *         invalid, the path string span is invalid, non-UTF-8 or contains an
 *         embedded NUL character, or a SQLite/IO operation fails.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_save(ovclash_suite_data_t* data,
                                                           ovclash_suite_string_t file_path);

/**
 * Saves the current data store to a new path.
 *
 * SQLite handles create a normal target through a database backup, including committed WAL rows
 * and pending writes. They adopt the destination only after the backup and the source close
 * succeed. Saving to the current file or to an existing hard-link alias keeps the current store
 * identity. A private database extracted from a legacy container may instead move to an unused
 * destination after closing, provided no source WAL and no destination SQLite sidecars remain. A
 * target ending in `.clashDetection` is written in the legacy container format: a 16-byte
 * clashDetection header followed by the SQLite database bytes. Directory handles keep their JSON
 * directory format for every target path.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param file_path Length-prefixed target file path. ptr may be null only when
 *                  length is zero.
 *
 * \return Success after adopting a database snapshot, preserving the current store identity,
 *         moving/reopening a private extraction, or writing the legacy container.
 *         Failure when arguments are invalid, no store is open, file_path is empty, the path
 *         string span is invalid, non-UTF-8 or contains an embedded NUL character, or a
 *         SQLite/IO operation fails.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_save_as(ovclash_suite_data_t* data,
                                                              ovclash_suite_string_t file_path);

/**
 * Closes the current data store.
 *
 * Attempts to commit pending writes, including partial results kept after a failed or abandoned
 * fetch. This is not a rollback operation. Destroy fetch operations and cursors first. A failed
 * SQLite commit requests a rollback and returns failure. A commit or close failure keeps the data
 * handle and storage backend for cleanup or retry, and never reports a successful close.
 *
 * \param data Suite data handle created by either data factory. May be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_close(ovclash_suite_data_t* data);

/**
 * Commits pending data-store changes.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_commit(ovclash_suite_data_t* data);

/**
 * Migrates a data store to the latest supported schema when possible.
 *
 * Migration of a legacy `.clashDetection` file is non-destructive. The embedded SQLite payload is
 * extracted to a temporary `.ovclashdb` file, and that temporary database is migrated and opened.
 * ovclash_suite_data_save_as persists it as a sidecar database or as a rewritten legacy
 * container. Directory handles accept an existing directory with the supported format version.
 * They reject unknown versions without converting or creating the store.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param file_path Length-prefixed data file path to migrate. ptr may be null
 *                  only when length is zero.
 *
 * \return Success when the current or supplied database is compatible or migrated. Failure when
 *         the arguments are invalid, the path string span is invalid, not UTF-8 or contains an
 *         embedded NUL character, neither a supplied nor a current data path is available, the
 *         supplied path does not exist, the legacy header is corrupt or unsupported, no schema
 *         migration path exists, the database is locked or not writable, or SQLite or IO fails.
 *         Migration never creates a missing target path. For a SQLite handle, some failures are
 *         rejected before the current store is disturbed: a call with neither a supplied nor a
 *         current path, a supplied path that is not a readable regular file, and a SQLite target
 *         that this process cannot write. The store then keeps its connection, pending writes,
 *         open state and compatibility flags, so an unnamed store keeps its private database. An
 *         extracted legacy payload is not discarded either, so the caller can still migrate in
 *         place. A legacy container, recognized by its suffix or header, is only read and need
 *         not be writable. Other target failures (a corrupt or unmigratable file, a lock, a
 *         read-only directory) are found only after the current store is closed. An invalid
 *         file-path string span, invalid UTF-8 or an embedded NUL character keeps the earlier
 *         open state. After string and path validation succeed, a SQLite migration failure
 *         leaves the handle not open. Directory migration validates the destination manifest
 *         before it closes the current store; a rejected manifest keeps the open state and the
 *         pending writes. A later close or open failure follows the matching lifecycle contract.
 *         The last-error text describes the reason.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_migrate_to_latest(ovclash_suite_data_t* data,
                                                                        ovclash_suite_string_t file_path);

/**
 * Inserts a query and commits, like ovclash_suite_data_insert_query_with_commit with commit 1.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query Query view to insert. String views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param out_identifier Receives the inserted query identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_insert_query(ovclash_suite_data_t* data,
                                                                   const ovclash_suite_query_view_t* query,
                                                                   uint32_t update_identifier,
                                                                   int64_t* out_identifier);

/**
 * Inserts a query with explicit commit control.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query Query view to insert. String views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param commit Non-zero to commit after the insert. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_identifier Receives the inserted query identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_insert_query_with_commit(ovclash_suite_data_t* data,
                                                                               const ovclash_suite_query_view_t* query,
                                                                               uint32_t update_identifier,
                                                                               uint32_t commit,
                                                                               int64_t* out_identifier);

/**
 * Updates an existing query and commits, like ovclash_suite_data_update_query_with_commit with
 * commit 1.
 *
 * Fails, before the write begins, when no query has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query Query view containing the identifier and replacement values.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_update_query(ovclash_suite_data_t* data,
                                                                   const ovclash_suite_query_view_t* query);

/**
 * Updates an existing query with explicit commit control.
 *
 * Fails, before the write begins, when no query has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query Query view containing the identifier and replacement values.
 * \param commit Non-zero to commit after the update. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_update_query_with_commit(ovclash_suite_data_t* data,
                                                                               const ovclash_suite_query_view_t* query,
                                                                               uint32_t commit);

/**
 * Finds a query by identifier.
 *
 * Fails with an empty last-error text when no record matches, which tells absence apart from
 * an error.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Query identifier to search for.
 * \param out_query Receives the found query view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_find_query(const ovclash_suite_data_t* data,
                                                                 int64_t identifier,
                                                                 ovclash_suite_query_view_t* out_query);

/**
 * Counts stored queries.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_count Receives the number of stored queries. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_query_count(const ovclash_suite_data_t* data,
                                                                      uint32_t* out_count);

/**
 * Reads a query by zero-based storage index.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param index Zero-based query index.
 * \param out_query Receives the query view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_query_at(const ovclash_suite_data_t* data,
                                                                   uint32_t index,
                                                                   ovclash_suite_query_view_t* out_query);

/**
 * Removes a query with its overlaps and their frame records, and commits, like
 * ovclash_suite_data_remove_query_with_commit with commit 1.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Query identifier to remove.
 *
 * \return Success when the query was removed. Failure when no query has the identifier, reported
 *         before the write begins so pending writes stay untouched, or as the data API catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_query(ovclash_suite_data_t* data, int64_t identifier);

/**
 * Removes a query with its overlaps and their frame records, with explicit commit control.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Query identifier to remove.
 * \param commit Non-zero to commit after removal. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 *
 * \return Success when the query was removed. Failure when no query has the identifier, reported
 *         before the write begins so pending writes stay untouched, or as the data API catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_query_with_commit(ovclash_suite_data_t* data,
                                                                               int64_t identifier,
                                                                               uint32_t commit);

/**
 * Inserts an overlap record and commits, like ovclash_suite_data_insert_overlap_with_commit with
 * commit 1.
 *
 * The store also creates overlap->frame_count placeholder frame records, replacing any frame
 * records of a reused identifier. A frame_count above 1000000 fails before the write begins.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap Overlap record view to insert. String and buffer views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param out_identifier Receives the inserted overlap identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_overlap(ovclash_suite_data_t* data,
                                  const ovclash_suite_clash_info_record_view_t* overlap,
                                  uint32_t update_identifier,
                                  int64_t* out_identifier);

/**
 * Inserts an overlap record with explicit commit control.
 *
 * The store also creates overlap->frame_count placeholder frame records, replacing any frame
 * records of a reused identifier. A frame_count above 1000000 fails before the write begins.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap Overlap record view to insert. String and buffer views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param commit Non-zero to commit after the insert. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_identifier Receives the inserted overlap identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_overlap_with_commit(ovclash_suite_data_t* data,
                                              const ovclash_suite_clash_info_record_view_t* overlap,
                                              uint32_t update_identifier,
                                              uint32_t commit,
                                              int64_t* out_identifier);

/**
 * Updates an overlap record and commits, like ovclash_suite_data_update_overlap_with_commit with
 * commit 1.
 *
 * While the overlap has no frame records the store creates overlap->frame_count placeholder
 * frame records; existing frame records are left untouched.
 * Fails, before the write begins, when no overlap has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap Overlap record view containing the identifier and replacement values.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_update_overlap(ovclash_suite_data_t* data, const ovclash_suite_clash_info_record_view_t* overlap);

/**
 * Updates an overlap record with explicit commit control.
 *
 * While the overlap has no frame records the store creates overlap->frame_count placeholder
 * frame records; existing frame records are left untouched.
 * Fails, before the write begins, when no overlap has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap Overlap record view containing the identifier and replacement values.
 * \param commit Non-zero to commit after the update. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_update_overlap_with_commit(
    ovclash_suite_data_t* data, const ovclash_suite_clash_info_record_view_t* overlap, uint32_t commit);

/**
 * Counts overlaps belonging to a query.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to filter overlaps.
 * \param out_count Receives the overlap count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_overlap_count_by_query(const ovclash_suite_data_t* data,
                                                                                 int64_t query_id,
                                                                                 uint32_t* out_count);

/**
 * Counts distinct state buckets for overlaps belonging to a query.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to filter overlaps.
 * \param out_count Receives the number of state-count rows. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_overlap_state_count_by_query(const ovclash_suite_data_t* data,
                                                                                       int64_t query_id,
                                                                                       uint32_t* out_count);

/**
 * Reads one state-count bucket for overlaps belonging to a query. Buckets are ordered by
 * ascending state value.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to filter overlaps.
 * \param index Zero-based state-count row index.
 * \param out_count Receives the state value and row count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_overlap_state_count_at(
    const ovclash_suite_data_t* data, int64_t query_id, uint32_t index, ovclash_suite_state_count_view_t* out_count);

/**
 * Reads an overlap belonging to a query by zero-based filtered index.
 *
 * On a SQLite store a call skips index rows. Past the first 1024 indices, a call for the index
 * right after that of the previous call on the same query continues from the previous row
 * instead, unless a cursor is open or the store was written in between. Prefer
 * ovclash_suite_data_get_overlaps_by_query for sequential reads.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to filter overlaps.
 * \param index Zero-based overlap index within the query.
 * \param out_overlap Receives the overlap record view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_get_overlap_by_query_at(const ovclash_suite_data_t* data,
                                           int64_t query_id,
                                           uint32_t index,
                                           ovclash_suite_clash_info_record_view_t* out_overlap);

/**
 * Opens a forward-only cursor over the overlaps of a query, ordered by overlap_id, then
 * identifier.
 *
 * The cursor reads overlap headers incrementally, including the frame count of each overlap; it
 * does not load frame payloads. The offset is skipped once for the whole range. Serialize access
 * with other calls on the same data handle. Destroy the cursor before closing, reopening or
 * destroying that handle. The visibility of writes made while a cursor is open follows the
 * backend; the cursor is not an immutable snapshot. SQLite uses one ordered statement. Directory
 * cursors keep the ordered identifiers and load each header when advanced.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to filter overlaps.
 * \param offset Initial overlap offset; negative values start at zero.
 * \param limit Maximum number of overlaps; zero means all remaining overlaps. Must be nonnegative.
 * \param out_cursor Receives an owned cursor, including for an empty range. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_get_overlaps_by_query(const ovclash_suite_data_t* data,
                                         int64_t query_id,
                                         int64_t offset,
                                         int64_t limit,
                                         ovclash_suite_overlap_cursor_t** out_cursor);

/**
 * Advances an overlap cursor by one record.
 *
 * Exhaustion succeeds with out_has_value=0 and out_view cleared, also on repeated calls. A
 * returned view borrows cursor-owned memory until the next advance or the destruction of this
 * cursor. Other cursors and data reads do not invalidate it.
 *
 * \param cursor Cursor returned by ovclash_suite_data_get_overlaps_by_query. Must not be null.
 * \param out_view Receives a borrowed overlap record view. Must not be null.
 * \param out_has_value Receives one for a row and zero for exhaustion or error. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_overlap_cursor_next(ovclash_suite_overlap_cursor_t* cursor,
                                                                     ovclash_suite_clash_info_record_view_t* out_view,
                                                                     uint32_t* out_has_value);

/**
 * Destroys an overlap cursor. Passing null succeeds and does nothing.
 *
 * \param cursor Cursor returned by ovclash_suite_data_get_overlaps_by_query.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_overlap_cursor_destroy(ovclash_suite_overlap_cursor_t* cursor);

/**
 * Finds an overlap by identifier.
 *
 * Fails with an empty last-error text when no record matches, which tells absence apart from
 * an error.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Overlap identifier to search for.
 * \param out_overlap Receives the found overlap record view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_find_overlap(const ovclash_suite_data_t* data,
                                                                   int64_t identifier,
                                                                   ovclash_suite_clash_info_record_view_t* out_overlap);

/**
 * Removes all overlaps of a query with their frame records, and commits, like
 * ovclash_suite_data_remove_overlaps_by_query_with_commit with commit 1.
 * Like any write, the removal opens a store's database or directory on demand, so a store that
 * has none yet removes nothing and reports 0.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to select overlaps for removal.
 * \param out_removed_count Receives the number of removed overlaps. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_overlaps_by_query(ovclash_suite_data_t* data,
                                                                               int64_t query_id,
                                                                               uint32_t* out_removed_count);

/**
 * Removes all overlaps of a query with their frame records, with explicit commit control.
 * Like any write, the removal opens a store's database or directory on demand, so a store that
 * has none yet removes nothing and reports 0.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param query_id Query identifier used to select overlaps for removal.
 * \param commit Non-zero to commit after removal. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_removed_count Receives the number of removed overlaps. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_overlaps_by_query_with_commit(ovclash_suite_data_t* data,
                                                                                           int64_t query_id,
                                                                                           uint32_t commit,
                                                                                           uint32_t* out_removed_count);

/**
 * Removes an overlap with its frame records, and commits, like
 * ovclash_suite_data_remove_overlap_with_commit with commit 1.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Overlap identifier to remove.
 *
 * \return Success when the overlap was removed. Failure when no overlap has the identifier, reported
 *         before the write begins so pending writes stay untouched, or as the data API catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_overlap(ovclash_suite_data_t* data, int64_t identifier);

/**
 * Removes an overlap with its frame records, with explicit commit control.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Overlap identifier to remove.
 * \param commit Non-zero to commit after removal. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 *
 * \return Success when the overlap was removed. Failure when no overlap has the identifier, reported
 *         before the write begins so pending writes stay untouched, or as the data API catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_overlap_with_commit(ovclash_suite_data_t* data,
                                                                                 int64_t identifier,
                                                                                 uint32_t commit);
/**
 * Counts frame records belonging to an overlap.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier used to filter frame records.
 * \param out_count Receives the frame count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_frame_count_by_overlap(const ovclash_suite_data_t* data,
                                                                                 int64_t overlap_identifier,
                                                                                 uint32_t* out_count);

/**
 * Reads a frame record belonging to an overlap by zero-based filtered index.
 *
 * On a SQLite store each call skips index rows, so reading the frames of an overlap one index at
 * a time is quadratic in the frame count. Use ovclash_suite_data_get_frames_by_overlap for
 * sequential reads.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier used to filter frame records.
 * \param index Zero-based frame index within the overlap.
 * \param out_frame Receives the frame record view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_get_frame_by_overlap_at(const ovclash_suite_data_t* data,
                                           int64_t overlap_identifier,
                                           uint32_t index,
                                           ovclash_suite_clash_frame_info_record_view_t* out_frame);

/**
 * Opens a forward-only cursor over the frame records of an overlap, in the same
 * timecode-then-identifier order ovclash_suite_data_get_frame_by_overlap_at reads.
 *
 * The cursor skips the requested offset once, unlike repeated indexed reads. SQLite steps one
 * ordered statement. Directory cursors keep the ordered identifiers and load one frame file per
 * advance. Changes made while the cursor is active have no guaranteed visibility; finish the
 * cursor before modifying the queried rows.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier used to filter frame records.
 * \param max_frame_count Maximum number of frames to read, or a negative value for all of them;
 *        zero reads none.
 * \param first_frame_offset Zero-based frame index to start at. Negative values start at zero.
 * \param out_cursor Receives the cursor. Must not be null. Destroy it with
 *        ovclash_suite_frame_cursor_destroy, before destroying the data handle.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_frames_by_overlap(const ovclash_suite_data_t* data,
                                                                            int64_t overlap_identifier,
                                                                            int64_t max_frame_count,
                                                                            int64_t first_frame_offset,
                                                                            ovclash_suite_frame_cursor_t** out_cursor);

/**
 * Advances a frame cursor by one record.
 *
 * Running out of frames is not an error: the call succeeds with *out_has_frame set to zero and
 * out_frame cleared. out_frame borrows cursor-owned memory, which the next
 * ovclash_suite_frame_cursor_next or ovclash_suite_frame_cursor_destroy on the same cursor
 * invalidates. Copy the needed data before advancing.
 *
 * \param cursor Cursor returned by ovclash_suite_data_get_frames_by_overlap. Must not be null.
 * \param out_frame Receives the frame record view. Must not be null.
 * \param out_has_frame Receives non-zero when a frame was read. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_frame_cursor_next(ovclash_suite_frame_cursor_t* cursor,
                                ovclash_suite_clash_frame_info_record_view_t* out_frame,
                                uint32_t* out_has_frame);

/**
 * Destroys a frame cursor. Passing null succeeds and does nothing.
 *
 * \param cursor Cursor returned by ovclash_suite_data_get_frames_by_overlap.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_frame_cursor_destroy(ovclash_suite_frame_cursor_t* cursor);

/**
 * Inserts a frame record and commits, like ovclash_suite_data_insert_frame_with_commit with
 * commit 1. frame->clash_info_id must name an existing overlap.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param frame Frame record view to insert. String and buffer views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param out_identifier Receives the inserted frame identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_frame(ovclash_suite_data_t* data,
                                const ovclash_suite_clash_frame_info_record_view_t* frame,
                                uint32_t update_identifier,
                                int64_t* out_identifier);

/**
 * Inserts a frame record with explicit commit control. frame->clash_info_id must name an
 * existing overlap.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param frame Frame record view to insert. String and buffer views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param commit Non-zero to commit after the insert. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_identifier Receives the inserted frame identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_frame_with_commit(ovclash_suite_data_t* data,
                                            const ovclash_suite_clash_frame_info_record_view_t* frame,
                                            uint32_t update_identifier,
                                            uint32_t commit,
                                            int64_t* out_identifier);

/**
 * Removes all frame records of an overlap and commits, like
 * ovclash_suite_data_remove_frames_by_overlap_with_commit with commit 1.
 * Like any write, the removal opens a store's database or directory on demand, so a store that
 * has none yet removes nothing and reports 0.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier used to select frame records for removal.
 * \param out_removed_count Receives the number of removed frame records. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_frames_by_overlap(ovclash_suite_data_t* data,
                                                                               int64_t overlap_identifier,
                                                                               uint32_t* out_removed_count);

/**
 * Removes all frame records for an overlap with explicit commit control.
 * Like any write, the removal opens a store's database or directory on demand, so a store that
 * has none yet removes nothing and reports 0.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier used to select frame records for removal.
 * \param commit Non-zero to commit after removal. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_removed_count Receives the number of removed frame records. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_frames_by_overlap_with_commit(ovclash_suite_data_t* data,
                                                                                           int64_t overlap_identifier,
                                                                                           uint32_t commit,
                                                                                           uint32_t* out_removed_count);

/**
 * Replaces all frame records for an overlap with explicit commit control.
 *
 * This is the bulk equivalent of removing frames with
 * ovclash_suite_data_remove_frames_by_overlap_with_commit, then inserting each frame with
 * ovclash_suite_data_insert_frame_with_commit. The function validates the counted frame array
 * and each nested counted buffer before it mutates the data store. All input frame views are
 * copied, and no pointer in frames is retained. Each stored frame gets overlap_identifier as its
 * clash_info_id, whatever the view holds.
 *
 * \param data Suite data handle created by either data factory. Must not be null and must be writable.
 * \param overlap_identifier Existing overlap identifier whose frame rows are replaced.
 * \param frames Array of frame record views. May be null only when frame_count is zero.
 * \param frame_count Number of frame views in frames. At most 1000000.
 * \param commit Non-zero to commit after replacement. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_inserted_count Receives the number of inserted frame records. Must not be null.
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS on success; OVCLASH_SUITE_STATUS_ERROR when data, frames, nested buffers,
 *         or overlap_identifier is invalid, or the store write fails. Failure text is available
 *         through ovclash_suite_data_get_last_error(data) and ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_replace_frames_by_overlap_with_commit(ovclash_suite_data_t* data,
                                                         int64_t overlap_identifier,
                                                         const ovclash_suite_clash_frame_info_record_view_t* frames,
                                                         size_t frame_count,
                                                         uint32_t commit,
                                                         uint32_t* out_inserted_count);

/**
 * Replaces every frame record of an overlap with placeholder frames and commits, like
 * ovclash_suite_data_set_frame_count_for_overlap_with_commit with commit 1.
 *
 * Placeholder frames carry identity matrices and no faces, outlines or points. Existing frame
 * data, including timecodes, faces, outlines and matrices, is deleted even when frame_count
 * equals the current count. Use ovclash_suite_data_replace_frames_by_overlap_with_commit to
 * write real frames.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier to update.
 * \param frame_count Number of placeholder frame records to create. At most 1000000.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_set_frame_count_for_overlap(ovclash_suite_data_t* data,
                                                                                  int64_t overlap_identifier,
                                                                                  uint32_t frame_count);

/**
 * Replaces every frame record of an overlap with placeholder frames, with explicit commit control.
 *
 * Placeholder frames carry identity matrices and no faces, outlines or points. Existing frame
 * data, including timecodes, faces, outlines and matrices, is deleted even when frame_count
 * equals the current count. Use ovclash_suite_data_replace_frames_by_overlap_with_commit to
 * write real frames.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param overlap_identifier Overlap identifier to update.
 * \param frame_count Number of placeholder frame records to create. At most 1000000.
 * \param commit Non-zero to commit after the update. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_set_frame_count_for_overlap_with_commit(
    ovclash_suite_data_t* data, int64_t overlap_identifier, uint32_t frame_count, uint32_t commit);

/**
 * Inserts point-cloud settings and commits, like
 * ovclash_suite_data_insert_point_cloud_settings_with_commit with commit 1.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param settings Point-cloud settings view to insert. String views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param out_identifier Receives the inserted settings identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_point_cloud_settings(ovclash_suite_data_t* data,
                                               const ovclash_suite_point_cloud_settings_view_t* settings,
                                               uint32_t update_identifier,
                                               int64_t* out_identifier);

/**
 * Inserts point-cloud settings with explicit commit control.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param settings Point-cloud settings view to insert. String views are copied by the data store.
 * \param update_identifier Non-zero to generate a new identifier. Zero stores the view's identifier,
 *        or generates one when it is negative.
 * \param commit Non-zero to commit after the insert. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 * \param out_identifier Receives the inserted settings identifier. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_insert_point_cloud_settings_with_commit(ovclash_suite_data_t* data,
                                                           const ovclash_suite_point_cloud_settings_view_t* settings,
                                                           uint32_t update_identifier,
                                                           uint32_t commit,
                                                           int64_t* out_identifier);

/**
 * Updates point-cloud settings and commits, like
 * ovclash_suite_data_update_point_cloud_settings_with_commit with commit 1.
 *
 * Fails, before the write begins, when no point-cloud settings record has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param settings Point-cloud settings view containing the identifier and replacement values.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_update_point_cloud_settings(
    ovclash_suite_data_t* data, const ovclash_suite_point_cloud_settings_view_t* settings);

/**
 * Updates point-cloud settings with explicit commit control.
 *
 * Fails, before the write begins, when no point-cloud settings record has the view's identifier.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param settings Point-cloud settings view containing the identifier and replacement values.
 * \param commit Non-zero to commit after the update. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_update_point_cloud_settings_with_commit(
    ovclash_suite_data_t* data, const ovclash_suite_point_cloud_settings_view_t* settings, uint32_t commit);

/**
 * Finds point-cloud settings by identifier.
 *
 * Fails with an empty last-error text when no record matches, which tells absence apart from
 * an error.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Point-cloud settings identifier to search for.
 * \param out_settings Receives the found settings view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_find_point_cloud_settings(
    const ovclash_suite_data_t* data, int64_t identifier, ovclash_suite_point_cloud_settings_view_t* out_settings);

/**
 * Counts stored point-cloud settings records.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param out_count Receives the settings record count. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_point_cloud_settings_count(const ovclash_suite_data_t* data,
                                                                                     uint32_t* out_count);

/**
 * Reads point-cloud settings by zero-based storage index.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param index Zero-based settings record index.
 * \param out_settings Receives the settings view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_get_point_cloud_settings_at(
    const ovclash_suite_data_t* data, uint32_t index, ovclash_suite_point_cloud_settings_view_t* out_settings);

/**
 * Finds point-cloud settings by source point-cloud prim path.
 *
 * Fails with an empty last-error text when no record matches, which tells absence apart from
 * an error.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param point_cloud_prim_path Length-prefixed source point-cloud prim path to search for.
 * \param out_settings Receives the found settings view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_data_find_point_cloud_settings_by_prim_path(const ovclash_suite_data_t* data,
                                                          ovclash_suite_string_t point_cloud_prim_path,
                                                          ovclash_suite_point_cloud_settings_view_t* out_settings);

/**
 * Finds point-cloud settings by generated destination path.
 *
 * Fails with an empty last-error text when no record matches, which tells absence apart from
 * an error.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param destination_path Length-prefixed destination path to search for.
 * \param out_settings Receives the found settings view. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_find_point_cloud_settings_by_destination_path(
    const ovclash_suite_data_t* data,
    ovclash_suite_string_t destination_path,
    ovclash_suite_point_cloud_settings_view_t* out_settings);

/**
 * Removes point-cloud settings and commits, like
 * ovclash_suite_data_remove_point_cloud_settings_with_commit with commit 1.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Point-cloud settings identifier to remove.
 *
 * \return Success when the settings were removed. Failure when no settings have the identifier,
 *         reported before the write begins so pending writes stay untouched, or as the data API
 *         catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_point_cloud_settings(ovclash_suite_data_t* data,
                                                                                  int64_t identifier);

/**
 * Removes point-cloud settings with explicit commit control.
 *
 * \param data Suite data handle created by either data factory. Must not be null.
 * \param identifier Point-cloud settings identifier to remove.
 * \param commit Non-zero to commit after removal. Zero leaves the write
 * pending; a later write that fails after its storage write began rolls back every pending
 * write (see Data Serialization API).
 *
 * \return Success when the settings were removed. Failure when no settings have the identifier,
 *         reported before the write begins so pending writes stay untouched, or as the data API
 *         catalogue says.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_data_remove_point_cloud_settings_with_commit(
    ovclash_suite_data_t* data, int64_t identifier, uint32_t commit);

/** @} */
/** @} */
#ifdef __cplusplus
}
#endif
