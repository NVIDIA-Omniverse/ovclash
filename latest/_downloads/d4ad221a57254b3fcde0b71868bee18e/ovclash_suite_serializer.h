// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ovclash_suite/ovclash_suite_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Transaction operations that the fetch engine requests through the transaction callback.
 * An overlap written before all its reports are captured is called streamed. BEGIN_OVERLAP and
 * END_OVERLAP bracket each streamed overlap.
 *
 * Fetch-and-save is not all-or-nothing:
 * - When a serializer or saved-data reader callback reports failure while the fetch advances,
 *   the engine requests ROLLBACK. A prepare_frame failure is the exception; it counts as a
 *   processing failure. ROLLBACK discards earlier fetch batches and unrelated writes already
 *   pending on the destination. Previously committed data is retained.
 * - On an extraction, frame preparation or other native processing failure, the engine requests
 *   ABORT_OVERLAP, but only while a streamed overlap is incomplete. ABORT_OVERLAP restores that
 *   overlap and keeps earlier completed pending writes. A failed ABORT_OVERLAP leads to ROLLBACK.
 *   Without an incomplete overlap, these failures leave pending writes unchanged.
 * - A failure during begin or initialization leaves pre-existing pending writes unchanged.
 * - Destroying an operation aborts an incomplete overlap in the same way, with the same ROLLBACK
 *   fallback. Completed writes stay pending. Destroy never commits.
 * - Successful finalization requests BEGIN before COMMIT, even when no results were written.
 * - Reconciliation may span several calls. mark_absent changes already applied stay pending when
 *   the operation is abandoned.
 *
 * ROLLBACK is requested at most once per operation and need not be idempotent. A custom
 * destination must implement these semantics, because the engine cannot undo writes when the
 * destination's ROLLBACK fails. Built-in data close and destroy attempt to commit retained pending
 * writes. */
typedef enum ovclash_suite_serializer_transaction_t
{
    /** Starts the pending transaction or reuses the open one; the callback must be idempotent. */
    OVCLASH_SUITE_SERIALIZER_BEGIN = 1,
    /** Publishes all pending writes. Requested only when the fetch completes successfully. */
    OVCLASH_SUITE_SERIALIZER_COMMIT = 2,
    /** Discards the entire pending transaction, including unrelated pending writes. */
    OVCLASH_SUITE_SERIALIZER_ROLLBACK = 3,
    /** Sets a restore point before a streamed overlap is replaced. */
    OVCLASH_SUITE_SERIALIZER_BEGIN_OVERLAP = 4,
    /** Releases the restore point after the final header of the streamed overlap is written. */
    OVCLASH_SUITE_SERIALIZER_END_OVERLAP = 5,
    /** Restores the incomplete streamed overlap to its restore point and keeps other pending writes. */
    OVCLASH_SUITE_SERIALIZER_ABORT_OVERLAP = 6
} ovclash_suite_serializer_transaction_t;

/** Engine-owned callback that a visit_existing implementation calls for each stored identifier.
 * Call it once for each stored overlap identifier of the requested query. Every signed int64
 * value, including -1, is a valid identifier. Duplicates are allowed; the engine removes them.
 * Pass visitor_data unchanged. Call the visitor synchronously on the thread running
 * visit_existing, and do not retain the function or visitor_data after visit_existing returns.
 * The visitor reports no status and never throws across this ABI.
 * \param visitor_data Engine-owned context supplied to visit_existing; pass it unchanged.
 * \param identifier One stored signed int64 identifier of the requested query.
 */
typedef void (*ovclash_suite_visit_identifier_fn)(void* visitor_data, int64_t identifier);

/** Persistence callback table for native incremental fetch-and-save, usable from C or C++.
 *
 * The table is copied at synchronous begin or at async launch. user_data must outlive the
 * operation. Inputs are borrowed for the duration of a callback unless a member documents a
 * longer lifetime.
 *
 * Completion: before each callback the engine sets out_success to 0. A callback writes exactly 1
 * only after all its work and output construction succeed; any other value means failure.
 * Callbacks return void, so out_success is the only completion signal. No callback may throw
 * across this ABI. The engine ignores the outputs of a failed callback, except that it takes
 * ownership of a nonnull prepare_frame output. Reporting failure cannot undo side effects that a
 * callback already published. A destination whose COMMIT or ROLLBACK fails may need
 * application-level recovery.
 *
 * Threading: every callback except prepare_frame runs on the driving thread, one call at a time.
 * With the synchronous bounded API, the driving thread is the thread calling
 * ovclash_suite_runtime_fetch_and_save_begin, next or destroy; no callback runs between those
 * calls. With ovclash_suite_runtime_fetch_and_save_async, it is the operation's background worker.
 * Storage writes follow runtime result order. The order of release_frame calls is unspecified.
 */
typedef struct ovclash_suite_fetch_serializer_t
{
    /** Caller-owned context passed unchanged as the first argument of every callback.
     * May be null when the implementation needs no context. The table is copied, but this pointer
     * and the state it names must stay valid until destroying the operation has drained all
     * callbacks.
     */
    void* user_data;

    /** Required: enumerates the stored identifiers of query_id once, during fetch initialization.
     * Runs on the driving thread before any fetch write. Include every stored row of the query,
     * including absent and resolved rows and separate rows that share an overlap hash. Frames are
     * not needed. Call visitor(visitor_data, identifier) once per row, in any order. The engine
     * deduplicates and sorts the identifiers for the later stale-row reconciliation. Do not retain
     * visitor or visitor_data. Set out_success to 1 only after the enumeration completes. A failure
     * here does not request ROLLBACK and leaves pre-existing pending writes unchanged.
     * \param user_data Caller-owned context from this callback table.
     * \param query_id Query whose stored overlap identifiers are enumerated.
     * \param visitor Engine-owned callback to call synchronously for each identifier.
     * \param visitor_data Engine-owned context to pass unchanged to visitor.
     * \param out_success Receives exactly 1 after a successful enumeration; initially 0.
     */
    void (*visit_existing)(void* user_data,
                           int64_t query_id,
                           ovclash_suite_visit_identifier_fn visitor,
                           void* visitor_data,
                           uint32_t* out_success);

    /** Required: looks up one stored header by query_id and the borrowed UTF-8 overlap hash overlap_id.
     * Runs on the driving thread before the first header write of an overlap, and again before the
     * final header of a streamed overlap. The engine zero-initializes out_record and out_found.
     * Set out_success to 1 for a successful lookup, including when no row exists. Set out_found to
     * 0 when the row is absent, or to nonzero when it is found. The engine reads out_record only for
     * a found row whose lookup succeeded. Its strings must be valid UTF-8 spans that stay alive
     * until the next serializer callback; the engine copies them. No frame reads or writes are
     * needed here.
     *
     * From a found row the engine keeps identifier, state, priority, person_in_charge,
     * creation_timestamp, comment and penetration_depth_px/nx/py/ny/pz/nz, and regenerates every
     * other field. Filling only these fields, with the rest zero-initialized, is enough. state is
     * required: the engine writes it back, and a found APPROVED state enables the approval
     * comparison described below.
     *
     * With resolve_stale zero, the stored state is kept. Otherwise a re-detected normal or duplicate
     * overlap becomes ACTIVE. The exception is an APPROVED clash whose detected header and complete
     * ordered frame contents match through the optional saved-data reader; it stays APPROVED.
     * Without that reader, a custom fetch makes every re-detected clash ACTIVE. A new header starts
     * NEW and is not promoted during its first fetch. Its final streamed header keeps any state that
     * the serializer applied on insertion.
     * \param user_data Caller-owned context from this callback table.
     * \param query_id Query of the stored header.
     * \param overlap_id Borrowed UTF-8 overlap hash of the stored header.
     * \param out_record Receives the found header; initially zero-initialized.
     * \param out_found Receives 0 when the row is absent, or nonzero when it is found; initially 0.
     * \param out_success Receives exactly 1 when the lookup succeeds, including absence; initially 0.
     */
    void (*find_overlap)(void* user_data,
                         int64_t query_id,
                         ovclash_suite_string_t overlap_id,
                         ovclash_suite_clash_info_record_view_t* out_record,
                         uint32_t* out_found,
                         uint32_t* out_success);

    /** Required: inserts or updates the borrowed overlap header record.
     * Runs on the driving thread after find_overlap. Write only the header. Do not commit, delete
     * existing frames, or create placeholder frames from num_records or frame_count. On success,
     * publish the row's stable signed int64 identifier through out_identifier and set out_success
     * to 1. Every signed value, including -1, is a valid identifier. The engine sets out_identifier
     * to 0 first and ignores it on failure. Do not retain record or its borrowed strings.
     * A streamed overlap gets an initial header before clear_frames and append_frame, and a final
     * refreshed header after its last captured frame. Frame batches in between reuse the stable
     * identifier.
     * \param user_data Caller-owned context from this callback table.
     * \param record Borrowed overlap header to insert or update, without frame contents.
     * \param out_identifier Receives the stable stored identifier on success; initially 0.
     * \param out_success Receives exactly 1 after the header write succeeds; initially 0.
     */
    void (*write_overlap)(void* user_data,
                          const ovclash_suite_clash_info_record_view_t* record,
                          int64_t* out_identifier,
                          uint32_t* out_success);

    /** Required: removes all stored frame records of identifier.
     * Keep the overlap's identity and editable annotations, and do not commit. Runs on the driving
     * thread once per written overlap, after its initial write_overlap and before its first
     * append_frame. A streamed overlap is cleared inside its restore point. Set out_success to 1
     * after the deletion succeeds. Failure requests ROLLBACK.
     * \param user_data Caller-owned context from this callback table.
     * \param identifier Stable identifier returned by write_overlap.
     * \param out_success Receives exactly 1 after the existing frames are removed; initially 0.
     */
    void (*clear_frames)(void* user_data, int64_t identifier, uint32_t* out_success);

    /** Required: appends the borrowed frame view to the overlap named by identifier.
     * Runs on the driving thread in runtime result order, after clear_frames. Do not commit.
     * prepared is the matching output of a successful prepare_frame. It is null when preparation
     * is not used or produced no payload. prepared stays engine-owned; do not free or retain it.
     * Copy the view and its arrays if storage needs them after this callback. Set out_success to 1
     * only after the frame is stored. Failure requests ROLLBACK. The order in which preparations
     * complete never changes the append order. release_frame runs only after the payload is used.
     * \param user_data Caller-owned context from this callback table.
     * \param identifier Stable identifier returned by write_overlap.
     * \param frame Borrowed runtime frame view and arrays to append.
     * \param prepared Matching engine-owned encoding from prepare_frame, or null.
     * \param out_success Receives exactly 1 after the frame is stored; initially 0.
     */
    void (*append_frame)(void* user_data,
                         int64_t identifier,
                         const ovclash_suite_clash_frame_info_view_t* frame,
                         const void* prepared,
                         uint32_t* out_success);

    /** Required: marks the stored row identifier absent, and also RESOLVED when resolve is nonzero.
     * Leave its frame records intact and do not commit. Runs on the driving thread after the result
     * writes, once for each initially enumerated identifier that no completed overlap write
     * matched, in ascending identifier order. Reconciliation may span several next calls; changes
     * already applied stay pending if the operation is abandoned. Set out_success to 1 on success.
     * Failure requests ROLLBACK.
     * \param user_data Caller-owned context from this callback table.
     * \param identifier Initially enumerated stored row that no completed write matched.
     * \param resolve Nonzero requests RESOLVED state as well as absence; zero keeps the state.
     * \param out_success Receives exactly 1 after the absence update succeeds; initially 0.
     */
    void (*mark_absent)(void* user_data, int64_t identifier, uint32_t resolve, uint32_t* out_success);

    /** Required: applies one ovclash_suite_serializer_transaction_t operation on the driving thread.
     * BEGIN must be idempotent. It precedes each write batch and each reconciliation batch, so
     * COMMIT always follows a BEGIN, even for an empty fetch. COMMIT is requested only when the
     * fetch completes. It publishes every pending write, including unrelated ones. ROLLBACK is
     * requested at most once per operation, after a persistence failure. It discards the whole
     * pending transaction, including earlier batches and unrelated pending writes.
     * BEGIN_OVERLAP sets a restore point before a streamed overlap is replaced. END_OVERLAP
     * releases it after the final header of that overlap is written. ABORT_OVERLAP restores only
     * the incomplete overlap, after a processing failure or on abandonment, and keeps earlier
     * completed pending writes. A failed ABORT_OVERLAP requests ROLLBACK. An initialization failure
     * leaves existing pending writes unchanged. Destruction never commits. Set out_success to 1
     * only after the operation succeeds. A failed COMMIT or ROLLBACK cannot undo side effects
     * already published and may need application-level recovery.
     * \param user_data Caller-owned context from this callback table.
     * \param operation One OVCLASH_SUITE_SERIALIZER_* transaction operation.
     * \param out_success Receives exactly 1 after the operation succeeds; initially 0.
     */
    void (*transaction)(void* user_data, uint32_t operation, uint32_t* out_success);

    /** Optional, set together with release_frame: encodes the borrowed frame view for a later append_frame.
     * Calls may run concurrently on preparation workers, as the runtime threading settings allow,
     * or serially on the driving thread, for example in single-threaded mode. Access only immutable
     * input and the output of this call. Do not use a connection or any other shared mutable state,
     * including serializer diagnostics. Do not retain the pointer to the view struct. A copy of the
     * view may keep its borrowed geometry arrays until the matching release_frame, which runs
     * before the report is released. The engine sets out_prepared to null first. Publish an
     * optional owned encoding there, then set out_success to 1. The engine takes every nonnull
     * published payload, even on failure, and releases it exactly once. A failed preparation is
     * never appended. It gets a generic diagnostic, and last_error is not queried. A successful
     * null payload is allowed.
     * \param user_data Caller-owned context; access only immutable state during preparation.
     * \param frame Borrowed frame view. Do not retain this pointer.
     * \param out_prepared Receives an optional owned encoding; initially null.
     * \param out_success Receives exactly 1 after a successful preparation; initially 0.
     */
    void (*prepare_frame)(void* user_data,
                          const ovclash_suite_clash_frame_info_view_t* frame,
                          void** out_prepared,
                          uint32_t* out_success);

    /** Optional, set together with prepare_frame: releases one nonnull prepared payload on the driving thread.
     * Runs before the runtime report that the payload borrows from is released. It is called
     * exactly once for every published nonnull payload, including failed preparations and work
     * that was abandoned or never appended. Release order is unspecified. Release all ownership
     * even when reporting failure, and set out_success to 1 only after cleanup succeeds. The engine
     * never retries this callback. A failed release gets a generic diagnostic and requests ROLLBACK
     * once the remaining cleanup has drained.
     * \param user_data Caller-owned context from this callback table.
     * \param prepared One nonnull encoding whose ownership is released.
     * \param out_success Receives exactly 1 after cleanup succeeds; initially 0.
     */
    void (*release_frame)(void* user_data, void* prepared, uint32_t* out_success);

    /** Optional: publishes the serializer's diagnostic through out_error on the driving thread.
     * The engine sets the view empty and out_success to 0 first. Set out_success to 1 after
     * publishing a valid borrowed UTF-8 view. The engine copies a nonempty message before the next
     * callback. Empty text, a null callback or a failed acknowledgement yields a generic
     * diagnostic, and the engine does not read failed outputs. This callback may follow a failed
     * storage action on the driving thread. It is not queried for prepare_frame or release_frame
     * failures, nor during secondary cleanup that preserves an earlier failure. Cleanup that is
     * itself the first failure may query it. Do not throw across this ABI.
     * \param user_data Caller-owned context from this callback table.
     * \param out_error Receives the borrowed UTF-8 diagnostic view; initially empty.
     * \param out_success Receives exactly 1 when the diagnostic is published; initially 0.
     */
    void (*last_error)(void* user_data, ovclash_suite_string_t* out_error, uint32_t* out_success);
} ovclash_suite_fetch_serializer_t;

/** Optional saved-data reader for exact comparison of previously approved custom overlaps.
 *
 * Pass this table to ovclash_suite_runtime_fetch_and_save_begin_with_reader or
 * ovclash_suite_runtime_fetch_and_save_async_with_reader. The table is copied at synchronous begin
 * or at async launch. user_data and the saved destination must outlive the operation. read_overlap
 * and read_frame are required; close is optional. All three run on the driving thread. The read
 * callbacks run before the overlap's header and frames are replaced. Set out_success to exactly 1
 * only after building complete outputs, and never throw across this ABI. Calls and borrowed
 * outputs follow the same serialized access rule as the fetch operation.
 *
 * The query and the ordered pair hash establish result identity. The comparison covers type,
 * tolerance, depth epsilon, paths, mesh checksums, aggregates and the ordered runtime frame
 * contents. It excludes stored overlap and frame row identifiers, review fields, timestamps and
 * user penetration-depth annotations. Floating-point equality uses no epsilon and ignores the
 * caller's denormal controls. Signed zeros compare equal, and a NaN never proves contents
 * unchanged.
 *
 * The reader is a separate table so that ovclash_suite_fetch_serializer_t keeps its ABI. Without a
 * reader, a nonzero resolve_stale makes every re-detected approved custom clash ACTIVE, because
 * unchanged detected contents cannot be proven.
 */
typedef struct ovclash_suite_fetch_saved_data_reader_t
{
    /** Caller-owned context passed unchanged as the first argument of every reader callback. */
    void* user_data;

    /** Required: reads the stored header of identifier into out_overlap.
     * identifier is the stable identifier that find_overlap returned. Fill every detected header
     * field and the actual saved frame_count; unlike find_overlap, do not leave detected fields
     * zero. Output strings stay valid until the next reader callback.
     */
    void (*read_overlap)(void*,
                         int64_t identifier,
                         ovclash_suite_clash_info_record_view_t* out_overlap,
                         uint32_t* out_success);

    /** Required: reads the complete saved frame at zero-based frame_index of identifier into out_frame.
     * Frames are indexed in persisted order. identifier is the one that find_overlap returned, not
     * the identifier inside the header from read_overlap. Output strings and arrays stay valid until
     * the next reader callback.
     */
    void (*read_frame)(void*,
                       int64_t identifier,
                       uint32_t frame_index,
                       ovclash_suite_clash_frame_info_record_view_t* out_frame,
                       uint32_t* out_success);

    /** Optional: releases any reader-owned cursor.
     * Runs on the driving thread after each overlap comparison that read saved data, before the
     * overlap is replaced, and when an unfinished fetch is destroyed. It must succeed when no
     * cursor is open. A failed close is reported and may be retried on the same thread during
     * failure or destruction cleanup; a successful close is not retried. Async fetching closes on
     * its original worker before it publishes its terminal state. Synchronous callers must drive
     * and destroy the operation on a thread compatible with the reader's resources. A reader whose
     * close keeps failing remains responsible for its resources.
     */
    void (*close)(void*, uint32_t* out_success);
} ovclash_suite_fetch_saved_data_reader_t;

#ifdef __cplusplus
}
#endif
