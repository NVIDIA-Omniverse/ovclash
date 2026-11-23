// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque runtime handle. Owns stage state, settings, scope and results until ovclash_suite_destroy_runtime. */
typedef struct ovclash_suite_runtime_t ovclash_suite_runtime_t;
/** Opaque async pipeline handle. Owned by the caller until ovclash_suite_async_destroy. */
typedef struct ovclash_suite_async_t ovclash_suite_async_t;
/** Opaque incremental fetch-and-save operation. Owned by the caller until ovclash_suite_runtime_fetch_and_save_destroy.
 *  Reserves its runtime and built-in store until destroyed, even after completion or failure.
 *  A custom destination must outlive it. */
typedef struct ovclash_suite_fetch_and_save_t ovclash_suite_fetch_and_save_t;

/** Opaque background fetch-and-save handle. Owned until ovclash_suite_fetch_async_destroy. */
typedef struct ovclash_suite_fetch_async_t ovclash_suite_fetch_async_t;
/** Opaque report handle. Owned by the caller until ovclash_suite_destroy_overlap_report.
 *  Borrows its runtime and may borrow that runtime's result buffers. */
typedef struct ovclash_suite_overlap_report_t ovclash_suite_overlap_report_t;
/** Opaque data-store handle for a SQLite file or a JSON directory. Destroy with ovclash_suite_data_destroy.
 *  Owns its store and the scratch records behind getter views. */
typedef struct ovclash_suite_data_t ovclash_suite_data_t;
/** Opaque forward-only cursor over the frame records of one overlap. Owned by the caller until
 *  ovclash_suite_frame_cursor_destroy. Blocks its data handle's lifecycle changes while alive,
 *  even when empty or exhausted. */
typedef struct ovclash_suite_frame_cursor_t ovclash_suite_frame_cursor_t;
/** Opaque forward-only cursor over the overlap records of one query. Owned by the caller until
 *  ovclash_suite_overlap_cursor_destroy. Blocks its data handle's lifecycle changes while alive,
 *  even when empty or exhausted. */
typedef struct ovclash_suite_overlap_cursor_t ovclash_suite_overlap_cursor_t;
/** Opaque owned ClashQuery model handle. Destroy with ovclash_suite_query_destroy. */
typedef struct ovclash_suite_query_t ovclash_suite_query_t;
/** Opaque owned PointCloudSettings model handle. Destroy with ovclash_suite_point_cloud_settings_destroy. */
typedef struct ovclash_suite_point_cloud_settings_t ovclash_suite_point_cloud_settings_t;
/** Opaque owned ClashInfo model handle. Destroy with ovclash_suite_clash_info_destroy. */
typedef struct ovclash_suite_clash_info_t ovclash_suite_clash_info_t;
/** Opaque owned ClashFrameInfo model handle. Destroy with ovclash_suite_clash_frame_info_destroy. */
typedef struct ovclash_suite_clash_frame_info_t ovclash_suite_clash_frame_info_t;
/** Opaque owned export text handle. Holds immutable UTF-8 JSON/HTML until ovclash_suite_export_text_destroy. */
typedef struct ovclash_suite_export_text_t ovclash_suite_export_text_t;

/**
 * Public C type contract.
 *
 * Ownership: every ovclash_suite_string_t that the API returns is a borrowed view, owned by the
 * handle or report that produced it. The producing API sets its lifetime. A noncursor data getter
 * also replaces the previous scratch view of the same kind on its next read. A cursor view lasts
 * until the cursor advances again or is destroyed. Copy the bytes before these events, before
 * any mutation and before the owner is destroyed. Input strings and arrays are copied unless the
 * function documents otherwise. On ovstage, report pointers can borrow runtime result storage.
 * Destroy a report before advancing another pipeline step, rerunning or resetting its runtime,
 * or invalidating that runtime's results, and always before destroying the runtime. Copying a
 * pointer does not extend its lifetime.
 *
 * Units and layout: times are in seconds unless a function names USD time codes. Matrices are
 * row-major 4x4 arrays of 16 doubles. Boolean fields use 0 for false and non-zero for true.
 * Counts are element counts, not byte counts. All paths and text are UTF-8.
 *
 * Threading: handles are not internally synchronized for concurrent mutation. Use a handle from
 * one thread at a time, and serialize access when a handle is shared. The only exception is the
 * concurrency that the async fetch snapshot and drain functions document. Independent data and
 * model handles remain subject to database and filesystem locking. Runtime calls also follow the
 * backend's shared-state limits. Serialize OpenUSD runtimes across handles, because their
 * cancellation, out-of-memory and underlying error state are process-global. Last-error text is
 * thread-local for Suite operation failures, and handle-local where a handle-specific getter
 * exists.
 *
 * Error behavior: every public function returning ovclash_suite_result_t catches C++ exceptions,
 * returns OVCLASH_SUITE_STATUS_ERROR and updates the thread-local last-error string. Destroy and
 * cleanup functions return a result too. Call them explicitly, outside object finalizers, and
 * check that result.
 */

/** Status code carried by ovclash_suite_result_t. */
typedef enum ovclash_suite_status_t
{
    /** The call completed. */
    OVCLASH_SUITE_STATUS_SUCCESS = 0,
    /** The call failed; the documented last-error getter describes why. */
    OVCLASH_SUITE_STATUS_ERROR = 1,
    /** Only report-creation APIs return this: the record exists but its report is unavailable. */
    OVCLASH_SUITE_STATUS_UNAVAILABLE = 2
} ovclash_suite_status_t;

/** Status-only result returned by Suite C ABI functions.
 *  Field catalogue:
 *  - status: OVCLASH_SUITE_STATUS_SUCCESS when the call completed. On
 *    OVCLASH_SUITE_STATUS_ERROR, read the documented handle-local or thread-local last-error
 *    accessor. Report creation may also return OVCLASH_SUITE_STATUS_UNAVAILABLE, which means an
 *    absent report, not a processing failure.
 *  The struct owns no memory and can be copied by value.
 */
typedef struct ovclash_suite_result_t
{
    ovclash_suite_status_t status;
} ovclash_suite_result_t;

/** Borrowed UTF-8 byte span.
 *  Field catalogue:
 *  - ptr: first byte of the UTF-8 span. May be null only when length is 0.
 *  - length: byte count, excluding any trailing null. It counts bytes, not Unicode scalar
 *    values.
 *  Returned non-null spans are null-terminated for C compatibility. Read them by length, which
 *  excludes the trailing null. An empty returned string may be {NULL, 0}. Input spans are copied
 *  unless the receiving function documents borrowing, and need not be null-terminated.
 *
 *  This is a transitional local type. Its layout is byte-identical to ovx_string_t of
 *  ovx/string_types.h, which the canonical ovclash/ovclash.h uses as ovclash_string_t.
 *
 *  The local type remains because the core Suite headers (ovclash_suite.h, ovclash_suite_types.h,
 *  ovclash_suite_models.h and ovclash_suite_serializer.h) install as a self-contained group that
 *  includes no ovx/ header. An SDK consumer can then use the Suite API without the producer
 *  include tree. The optional ovstage extension ovclash_suite_ovstage.h includes
 *  ovclash/ovclash_ovstage.h, so it needs the ovstage ovx/ include tree, as the canonical header
 *  does.
 *
 *  This type is to become ovx_string_t once the Suite headers can always depend on the shared
 *  ovx/ tree, as the canonical header already does. The layouts are identical, so the change is a
 *  typedef swap, not an ABI break.
 */
typedef struct ovclash_suite_string_t
{
    const char* ptr;
    size_t length;
} ovclash_suite_string_t;

/** Kind of a runtime or persisted overlap. */
typedef enum ovclash_suite_overlap_type_t
{
    /** Mesh-vs-mesh overlap. */
    OVCLASH_SUITE_OVERLAP_NORMAL = 1,
    /** Duplicate mesh pair. Runtime overlap data never uses it; fetch-and-save writes it for duplicates. */
    OVCLASH_SUITE_OVERLAP_DUPLICATE = 2,
    /** Point-cloud-vs-mesh overlap. */
    OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH = 3,
    /** Alias of OVCLASH_SUITE_OVERLAP_NORMAL. */
    OVCLASH_SUITE_OVERLAP_MESH_MESH = OVCLASH_SUITE_OVERLAP_NORMAL,
    /** A runtime pair type with no suite equivalent. */
    OVCLASH_SUITE_OVERLAP_UNDEFINED = 0x7fffffff
} ovclash_suite_overlap_type_t;

/** Review state of a persisted clash; ovclash_suite_runtime_fetch_and_save_begin lists the states fetching assigns. */
typedef enum ovclash_suite_clash_state_t
{
    OVCLASH_SUITE_CLASH_STATE_NEW = 1,
    OVCLASH_SUITE_CLASH_STATE_APPROVED = 2,
    OVCLASH_SUITE_CLASH_STATE_RESOLVED = 3,
    OVCLASH_SUITE_CLASH_STATE_CLOSED = 4,
    OVCLASH_SUITE_CLASH_STATE_INVALID = 5,
    OVCLASH_SUITE_CLASH_STATE_ACTIVE = 6
} ovclash_suite_clash_state_t;

/**
 * Identifiers of the Kit-compatible query settings. Each enumerator, minus its OVCLASH_SUITE_
 * prefix, is a key of the query-settings JSON that ovclash_suite_run_settings_to_query_settings_json
 * writes. No C function takes these values; language bindings read them through
 * ovclash_suite_get_named_constant.
 */
typedef enum ovclash_suite_setting_id_t
{
    OVCLASH_SUITE_SETTING_LOGGING = 1,
    OVCLASH_SUITE_SETTING_DYNAMIC = 2,
    OVCLASH_SUITE_SETTING_DYNAMIC_START_TIME = 3,
    OVCLASH_SUITE_SETTING_DYNAMIC_END_TIME = 4,
    OVCLASH_SUITE_SETTING_PURGE_PERMANENT_OVERLAPS = 5,
    OVCLASH_SUITE_SETTING_QUANTIZED = 6,
    OVCLASH_SUITE_SETTING_SINGLE_THREADED = 7,
    OVCLASH_SUITE_SETTING_NEW_TASK_MANAGER = 8,
    OVCLASH_SUITE_SETTING_FILTER_TEST = 9,
    OVCLASH_SUITE_SETTING_TIGHT_BOUNDS = 10,
    OVCLASH_SUITE_SETTING_COPLANAR = 11,
    OVCLASH_SUITE_SETTING_NB_TASKS = 12,
    OVCLASH_SUITE_SETTING_POSE_EPSILON = 13,
    OVCLASH_SUITE_SETTING_BOUNDS_EPSILON = 14,
    OVCLASH_SUITE_SETTING_AREA_EPSILON = 15,
    OVCLASH_SUITE_SETTING_TRIS_PER_LEAF = 16,
    OVCLASH_SUITE_SETTING_TOLERANCE = 17,
    OVCLASH_SUITE_SETTING_ANY_HIT = 18,
    OVCLASH_SUITE_SETTING_OVERLAP_CODE = 19,
    OVCLASH_SUITE_SETTING_DUP_MESHES = 20,
    OVCLASH_SUITE_SETTING_STATIC_TIME = 21,
    OVCLASH_SUITE_SETTING_COMPUTE_MAX_LOCAL_DEPTH = 22,
    OVCLASH_SUITE_SETTING_DEPTH_EPSILON = 23,
    OVCLASH_SUITE_SETTING_DISCARD_TOUCHING_CONTACTS = 24,
    OVCLASH_SUITE_SETTING_PURGE_PERMANENT_STATIC_OVERLAPS = 25,
    /** Accepted, persisted and read back, but has no effect in ovclash.
     *
     *  In the Kit extension this setting selects a Fabric/USDRT scene traversal instead of
     *  UsdStage::Traverse. Neither ovclash backend compiles that traversal. The ovstage backend
     *  cannot have an equivalent toggle. Its only scene path is ovstage population, which reads
     *  Fabric attributes, so there is no second traversal to switch to.
     *
     *  The setting is kept because it belongs to the Kit-compatible
     *  ClashDetection.DEFAULT_SETTINGS table and is serialized into the stored settings of every
     *  query. Setting it changes that serialized text and nothing else.
     *  ovclash_suite_get_run_settings, and ovclash_get_settings on the canonical ABI, report the
     *  stored value, so it round-trips without affecting clash results.
     */
    OVCLASH_SUITE_SETTING_USE_USDRT = 26,
    OVCLASH_SUITE_SETTING_TRIANGLE_LIMIT = 27,
    OVCLASH_SUITE_SETTING_IGNORE_REDUNDANT_OVERLAPS = 28,
    OVCLASH_SUITE_SETTING_IGNORE_INVISIBLE_PRIMS = 29,
    OVCLASH_SUITE_SETTING_CONTACT_CUTOFF = 30,
    OVCLASH_SUITE_SETTING_MAX_LOCAL_DEPTH_MODE = 31,
    OVCLASH_SUITE_SETTING_ENABLE_POINT_CLOUDS = 32,
    OVCLASH_SUITE_SETTING_POINT_SIZE_OVERRIDE = 33
} ovclash_suite_setting_id_t;

/**
 * Buffers requested from ovclash_suite_create_overlap_report, equal to the canonical
 * OVCLASH_OVERLAP_REPORT_FLAG_* values. FACES0, FACES1 and OUTLINE apply to mesh pairs and
 * POINTS0 to point-cloud pairs; a flag that does not apply to the pair type is ignored.
 */
typedef enum ovclash_suite_overlap_report_flag_t
{
    /** Requests nothing; ovclash_suite_create_overlap_report fills no buffer for it. */
    OVCLASH_SUITE_OVERLAP_REPORT_DEFAULT = 0,
    /** Legacy single-mesh face flag of the canonical API. ovclash_suite_create_overlap_report ignores it. */
    OVCLASH_SUITE_OVERLAP_REPORT_FACES = (1u << 0),
    /** Collision outline segments of a mesh pair, from hard intersections only. */
    OVCLASH_SUITE_OVERLAP_REPORT_OUTLINE = (1u << 1),
    /** Authored USD face indices of mesh 0. */
    OVCLASH_SUITE_OVERLAP_REPORT_FACES0 = (1u << 2),
    /** Authored USD face indices of mesh 1. */
    OVCLASH_SUITE_OVERLAP_REPORT_FACES1 = (1u << 3),
    /** Point indices of a point-cloud pair, together with point_size0. */
    OVCLASH_SUITE_OVERLAP_REPORT_POINTS0 = (1u << 4)
} ovclash_suite_overlap_report_flag_t;

/** Runtime clash settings.
 *  Field catalogue:
 *  - enable_log, enable_full_scene, dynamic, purge_permanent_dynamic_overlaps,
 *    purge_permanent_static_overlaps, only_find_duplicates,
 *    ignore_redundant_overlaps, enable_point_clouds, ignore_invisible_prims,
 *    use_usdrt, single_threaded, new_task_manager, coplanar, any_hit,
 *    quantized, tight_bounds, subcomponent_filtering,
 *    compute_max_local_depth, discard_touching_contacts,
 *    set_times: boolean flags; 0 is false and non-zero is true. dynamic is query metadata that
 *    the native runtime does not read; see the time fields below. set_times only enables the
 *    end_time >= start_time check. It does not decide whether start_time and end_time apply.
 *  - max_local_depth_mode: a mode selector, not a flag, although it sits between two flags in
 *    the struct. It selects one of three depth algorithms and must be in [0, 2]: 0 is minimal,
 *    the legacy fast estimate; 1 is medium, the default; 2 is high, medium refined with 64
 *    sampling directions. Every accepted value computes a depth. The three algorithms can return
 *    different depths for the same pair, so "non-zero is true" does not hold for this field.
 *    It is read only while compute_max_local_depth is non-zero. That flag is the on/off gate,
 *    and this scale has no "disabled" value. The scale is therefore offset by one from
 *    ovclash_max_local_depth_mode_t in ovclash.h, where 0 is DISABLED and 1, 2 and 3 are
 *    MINIMAL, MEDIUM and HIGH. The two scales are not interchangeable: a canonical enumerator
 *    passed here selects the next higher algorithm, and 3 is rejected. The Suite maps between
 *    the two scales explicitly.
 *  - nb_tasks, tris_per_leaf: non-negative task and element counts stored as uint32_t. Their
 *    validation differs, so neither is a precedent for the other.
 *    tris_per_leaf must be in [1, 15]; other values are rejected. Default 15. The value goes to
 *    PxBVH34MidphaseDesc::numPrimsPerLeaf, which PhysX accepts in [2, 15]. Mesh cooking retries
 *    with a larger value, which recovers 1. A value above 15 would fail every mesh cook, and the
 *    run would report zero clashes instead of an error.
 *    nb_tasks accepts any value up to 1000000 and rejects larger ones. The backend then clamps
 *    it to its 256-task maximum without notice. The settings read back hold the value passed in,
 *    not the clamped one: a request for 5000 reads back as 5000 while canonical storage holds
 *    256. Only the legacy task manager uses this cap, and ovstage further bounds its workers by
 *    hardware concurrency and available work. The new task manager ignores nb_tasks, and
 *    single_threaded overrides both managers. Default 128.
 *    Fetch-and-save frame preparation for directory stores and for custom serializers with
 *    prepare_frame follows the same settings on both backends. The new manager uses Carbonite's
 *    worker budget. The legacy manager uses std::thread workers bounded by nb_tasks, hardware
 *    concurrency and available work. single_threaded keeps preparation serial. Built-in SQLite
 *    frame encoding and all serializer writes stay on the driving thread. On ovstage, Suite face
 *    and outline preparation also follows the live runtime threading settings. Its workers drain
 *    before buffers are published or released.
 *  - overlap_code: an algorithm selector, not a count. Only 1, 2 and 3 select an algorithm. 0
 *    and every other value, including values above 3, behave identically. Neither backend
 *    validates it, so an out-of-range value is accepted without an error. Default 3.
 *  - triangle_limit: the maximum number of triangles reported per overlapping pair. 0 means
 *    unlimited, not none. any_hit overrides it to 1. No maximum is validated. Default 0,
 *    unlimited.
 *  - dynamic_start_time, dynamic_end_time, static_time, start_time, end_time: source sample
 *    times in seconds. USD time-code conversion uses the stage's effective rate, the
 *    time_codes_per_second of ovclash_suite_stage_timeline_data_t. Only start_time, end_time
 *    and time_codes_per_second select the sampled range, as startTime and endTime do in
 *    ovclash_settings_t. start_time == end_time is a single static sample at that time;
 *    otherwise the run samples [start_time, end_time]. dynamic, static_time,
 *    dynamic_start_time and dynamic_end_time are Kit-compatible query metadata. They are
 *    validated as finite, persisted into the query-settings JSON and round-tripped by
 *    ovclash_suite_get_run_settings, but the native runtime does not read them to choose
 *    samples. Resolve them into start_time and end_time before applying the settings, as the
 *    Python layer and the C++ sample do.
 *  - time_codes_per_second: sampling frequency in samples per second, independent of the
 *    stage's effective rate. Zero keeps the single-sample fallback of legacy settings. Positive
 *    values set the spacing of multiple samples. Negative values are rejected.
 *  - tolerance, pose_epsilon, bounds_epsilon, depth_epsilon, contact_cutoff,
 *    point_size_override: finite tolerances or point sizes in source scene units.
 *    area_epsilon is an area in squared scene units. Finite negative values are accepted. Their
 *    meaning depends on the setting; a negative value is not a general unset sentinel. The
 *    engine stores these values as float, so each must fit the float range (depth_epsilon only
 *    while discard_touching_contacts is set).
 *  ovclash_suite_set_run_settings rejects a struct with any out-of-range field. It then keeps the
 *  previous settings and names the field in the last-error text.
 */
typedef struct ovclash_suite_run_settings_t
{
    uint32_t enable_log;
    uint32_t enable_full_scene;
    uint32_t dynamic;
    uint32_t purge_permanent_dynamic_overlaps;
    uint32_t purge_permanent_static_overlaps;
    uint32_t only_find_duplicates;
    uint32_t ignore_redundant_overlaps;
    uint32_t enable_point_clouds;
    uint32_t ignore_invisible_prims;
    uint32_t use_usdrt;
    uint32_t single_threaded;
    uint32_t new_task_manager;
    uint32_t coplanar;
    uint32_t any_hit;
    uint32_t quantized;
    uint32_t tight_bounds;
    uint32_t subcomponent_filtering;
    uint32_t compute_max_local_depth;
    uint32_t discard_touching_contacts;
    uint32_t max_local_depth_mode;
    uint32_t nb_tasks;
    uint32_t tris_per_leaf;
    uint32_t overlap_code;
    uint32_t triangle_limit;
    uint32_t set_times;
    double dynamic_start_time;
    double dynamic_end_time;
    double static_time;
    double start_time;
    double end_time;
    double time_codes_per_second;
    double tolerance;
    double pose_epsilon;
    double bounds_epsilon;
    double area_epsilon;
    double depth_epsilon;
    double contact_cutoff;
    double point_size_override;
} ovclash_suite_run_settings_t;

/** Pipeline or async fetch progress snapshot.
 *  Field catalogue for the pipeline getters ovclash_suite_get_pipeline_step_data and
 *  ovclash_suite_async_get_step_data:
 *  - name: borrowed UTF-8 step name owned by the runtime/async handle.
 *  - step_index: zero-based active step index.
 *  - nb_steps: total step count for the prepared pipeline.
 *  - progress: normalized progress in [0, 1] when known. On the OpenUSD backend an
 *    ovclash_suite_async_get_step_data poll may transiently report a value outside [0, 1] or a
 *    non-finite one; clamp it and treat a non-finite value as unknown (see
 *    ovclash_get_pipeline_step_data_async).
 *  - finished: non-zero when the reported step is the pipeline's last step, which can be while
 *    it is still executing; use ovclash_suite_async_is_running to detect completion.
 *  ovclash_suite_fetch_async_get_step_data documents its own name, nb_steps and finished.
 */
typedef struct ovclash_suite_step_data_t
{
    ovclash_suite_string_t name;
    uint32_t step_index;
    uint32_t nb_steps;
    float progress;
    uint32_t finished;
} ovclash_suite_step_data_t;

/** Raw overlap summary returned from the runtime.
 *  Field catalogue:
 *  - name0/name1: borrowed source object paths owned by the runtime.
 *  - clash_hash, mesh_hash0, mesh_hash1: legacy 128-bit identifiers as
 *    [high64, low64].
 *  - nb_records: number of time/frame records in this overlap group.
 *  - nb_elements: collided element count reported by the backend.
 *  - type: ovclash_suite_overlap_type_t value: OVCLASH_SUITE_OVERLAP_NORMAL,
 *    OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH or OVCLASH_SUITE_OVERLAP_UNDEFINED.
 *  - time: source sample time in seconds.
 *  - min_distance, max_local_depth: source scene-unit distances/depths.
 */
typedef struct ovclash_suite_overlap_data_t
{
    ovclash_suite_string_t name0;
    ovclash_suite_string_t name1;
    uint64_t clash_hash[2];
    uint64_t mesh_hash0[2];
    uint64_t mesh_hash1[2];
    uint32_t nb_records;
    uint32_t nb_elements;
    uint32_t type;
    double time;
    double min_distance;
    double max_local_depth;
} ovclash_suite_overlap_data_t;

/** Duplicate mesh summary returned from the runtime.
 *  Field catalogue:
 *  - name0/name1: borrowed source object paths owned by the runtime.
 *  - clash_hash, mesh_hash0, mesh_hash1: legacy 128-bit identifiers as
 *    [high64, low64].
 *  - nb_tris: triangle count for the duplicate mesh pair.
 *  - time: source sample time in seconds.
 *  - matrix0/matrix1: reserved and always all-zero -- the engine reports no per-prim transform
 *    for a duplicate pair; use ovclash_suite_get_prim_matrix when one is needed.
 */
typedef struct ovclash_suite_duplicate_mesh_data_t
{
    ovclash_suite_string_t name0;
    ovclash_suite_string_t name1;
    uint64_t clash_hash[2];
    uint64_t mesh_hash0[2];
    uint64_t mesh_hash1[2];
    uint32_t nb_tris;
    double time;
    double matrix0[16];
    double matrix1[16];
} ovclash_suite_duplicate_mesh_data_t;

/** Depenetration result.
 *  Field catalogue:
 *  - dir: unit xyz direction of the depenetration displacement when depth is
 *    positive, otherwise all zero.
 *  - depth: length of the depenetration displacement in source scene units;
 *    -1.0 after a failed ovclash_suite_compute_penetration_depth, except for the early
 *    rejections that function lists.
 *  The struct has no owned memory.
 */
typedef struct ovclash_suite_depenetration_data_t
{
    double dir[3];
    double depth;
} ovclash_suite_depenetration_data_t;

/** Runtime query statistics.
 *  Field catalogue:
 *  - nb_meshes: discovered mesh prim count.
 *  - nb_shared_meshes: detected shared/instanced mesh count.
 *  - nb_cooked_meshes: mesh count cooked for clash execution.
 *  - nb_point_clouds: point-cloud prim count considered by the runtime.
 */
typedef struct ovclash_suite_query_stats_t
{
    uint32_t nb_meshes;
    uint32_t nb_shared_meshes;
    uint32_t nb_cooked_meshes;
    uint32_t nb_point_clouds;
} ovclash_suite_query_stats_t;

/** Root-layer timeline metadata of the open stage, as USD resolves it.
 *  Field catalogue:
 *  - start_time_code: stage startTimeCode (0.0 when unauthored).
 *  - end_time_code: stage endTimeCode (0.0 when unauthored).
 *  - time_codes_per_second: effective stage rate, resolving authored timeCodesPerSecond, then
 *    authored framesPerSecond, then 24.0. Seconds = time code divided by this rate, which is the
 *    conversion ovclash_suite_run_settings_t times use. The OpenUSD backend can report a
 *    nonpositive authored rate; reject a nonpositive or nonfinite rate before converting.
 *  - frames_per_second: stage framesPerSecond (24.0 when unauthored).
 *  For a Stage attached with ovclash_suite_attach_ovstage, the fields are the start_time_code,
 *  end_time_code, time_codes_per_second and frames_per_second of its binding, as supplied.
 */
typedef struct ovclash_suite_stage_timeline_data_t
{
    double start_time_code;
    double end_time_code;
    double time_codes_per_second;
    double frames_per_second;
} ovclash_suite_stage_timeline_data_t;

/** Borrowed overlap report buffers.
 *  Field catalogue:
 *  - nb_colliding_faces0/colliding_faces0: authored USD face-index buffer for object 0.
 *    The pointer may be null only when nb_colliding_faces0 is 0.
 *  - nb_colliding_faces1/colliding_faces1: authored USD face-index buffer for object 1.
 *    The pointer may be null only when nb_colliding_faces1 is 0.
 *  - nb_colliding_segments/colliding_segments_xyzxyz: collision-outline segments
 *    from hard triangle pairs only; clearance near-misses add none. The buffer
 *    is flat, in xyzxyz order. The count is in segments of six floats each, so
 *    the buffer holds nb_colliding_segments * 6 floats. In contrast,
 *    ovclash_suite_clash_frame_info_view_t::nb_collision_outline_values counts
 *    floats. The pointer may be null only when the count is 0.
 *  - nb_colliding_points0/colliding_points0: point indices for point-cloud
 *    clashes; pointer may be null only when the count is 0.
 *  - point_size0: source point size used for point-cloud report generation.
 *  - matrix0/matrix1: row-major 4x4 local-to-world transforms.
 *  Pointer fields are borrowed through the report and may reference runtime-owned
 *  storage. Destroy the report before advancing another pipeline step, rerunning or
 *  resetting the runtime, invalidating its results, or destroying it. Matrix values
 *  are copied into this view.
 */
typedef struct ovclash_suite_overlap_report_view_t
{
    uint32_t nb_colliding_faces0;
    const uint32_t* colliding_faces0;

    uint32_t nb_colliding_faces1;
    const uint32_t* colliding_faces1;

    uint32_t nb_colliding_segments;
    const float* colliding_segments_xyzxyz;

    uint32_t nb_colliding_points0;
    float point_size0;
    const uint32_t* colliding_points0;

    double matrix0[16];
    double matrix1[16];
} ovclash_suite_overlap_report_view_t;

/** Processed clash-info view for one runtime overlap record, filled by ovclash_suite_create_clash_info.
 *  Field catalogue:
 *  - identifier: always -1; the view is not persisted.
 *  - query_id: the query identifier passed to ovclash_suite_create_clash_info.
 *  - overlap_id: borrowed legacy overlap hash string (ovclash_suite_format_int128_hex of clash_hash).
 *  - overlap_type: ovclash_suite_overlap_type_t value.
 *  - present: always 1.
 *  - min_distance, max_local_depth: source scene-unit distance and depth of the record.
 *  - depth_epsilon, tolerance: the values passed to ovclash_suite_create_clash_info.
 *  - object_a_path/object_b_path: borrowed source prim paths.
 *  - object_a_mesh_crc/object_b_mesh_crc: borrowed legacy mesh hash strings.
 *  - start_time/end_time: both the record's source sample time in seconds.
 *  - num_records: number of frame records the engine reports for the overlap group.
 *    ovclash_suite_create_clash_info copies that count unchanged, so it can exceed
 *    the number of records that actually produce an overlap report.
 *  - overlap_elements: backend collided element count.
 *  - state: always OVCLASH_SUITE_CLASH_STATE_NEW.
 *  String fields from ovclash_suite_create_clash_info are runtime-owned. They stay valid after
 *  the overlap report is destroyed. They become invalid at the next
 *  ovclash_suite_create_clash_info, ovclash_suite_runtime_save_overlap_to_data or
 *  fetch-and-save capture on the same runtime, whether or not that call succeeds, and when the
 *  runtime is destroyed.
 */
typedef struct ovclash_suite_clash_info_view_t
{
    int64_t identifier;
    int64_t query_id;
    ovclash_suite_string_t overlap_id;
    uint32_t overlap_type;
    uint32_t present;
    double min_distance;
    double max_local_depth;
    double depth_epsilon;
    double tolerance;
    ovclash_suite_string_t object_a_path;
    ovclash_suite_string_t object_a_mesh_crc;
    ovclash_suite_string_t object_b_path;
    ovclash_suite_string_t object_b_mesh_crc;
    double start_time;
    double end_time;
    uint32_t num_records;
    uint32_t overlap_elements;
    uint32_t state;
} ovclash_suite_clash_info_view_t;

/** Processed per-frame clash details.
 *  Field catalogue:
 *  - timecode: source sample time in seconds.
 *  - min_distance, max_local_depth: source scene-unit distances/depths.
 *  - overlap_elements: collided element count for this frame.
 *  - nb_usd_faces0/usd_faces0 and nb_usd_faces1/usd_faces1: authored USD face-index
 *    buffers; pointer may be null only when the matching count is 0.
 *  - nb_usd_points0/usd_points0: point index buffer for point-cloud clashes.
 *  - nb_collision_outline_values/collision_outline_xyzxyz: outline float
 *    values in xyzxyz order, from hard triangle pairs only; clearance near-misses
 *    add none. The pointer may be null only when count is 0.
 *  - point_size0: source point size for point-cloud clashes.
 *  - object_0_matrix/object_1_matrix: row-major 4x4 local-to-world transforms
 *    copied from the report.
 *  Pointer fields are borrowed from the generated report.
 */
typedef struct ovclash_suite_clash_frame_info_view_t
{
    double timecode;
    double min_distance;
    double max_local_depth;
    uint32_t overlap_elements;

    uint32_t nb_usd_faces0;
    const uint32_t* usd_faces0;
    uint32_t nb_usd_faces1;
    const uint32_t* usd_faces1;
    uint32_t nb_usd_points0;
    const uint32_t* usd_points0;
    uint32_t nb_collision_outline_values;
    const float* collision_outline_xyzxyz;
    float point_size0;

    double object_0_matrix[16];
    double object_1_matrix[16];
} ovclash_suite_clash_frame_info_view_t;

/** Persistent query record view.
 *  Field catalogue:
 *  - identifier: persistent query id, or -1 before insert.
 *  - query_name: user-facing query name.
 *  - object_a_path/object_b_path: stored first/second scope path strings.
 *  - settings_json: legacy clash settings JSON text; any syntactically valid
 *    JSON value, or an empty string, is accepted on input.
 *  - creation_timestamp/last_modified_timestamp: legacy storage timestamp
 *    strings preserved by the native serializer.
 *  - last_modified_by: user string for the last mutation.
 *  - comment: user comment.
 *  - roi_include_path/roi_exclude_path: persisted ROI include/exclude strings.
 *  String fields are borrowed on output and copied on input.
 */
typedef struct ovclash_suite_query_view_t
{
    int64_t identifier;
    ovclash_suite_string_t query_name;
    ovclash_suite_string_t object_a_path;
    ovclash_suite_string_t object_b_path;
    ovclash_suite_string_t settings_json;
    ovclash_suite_string_t creation_timestamp;
    ovclash_suite_string_t last_modified_timestamp;
    ovclash_suite_string_t last_modified_by;
    ovclash_suite_string_t comment;
    ovclash_suite_string_t roi_include_path;
    ovclash_suite_string_t roi_exclude_path;
} ovclash_suite_query_view_t;

/** Persistent point-cloud settings view.
 *  Field catalogue:
 *  - identifier: persistent settings id, or -1 before insert.
 *  - point_cloud_prim_path: source point-cloud prim path.
 *  - destination_path: generated destination/output prim path.
 *  - save_to_session_layer, skip_rendering: 0/1 flags.
 *  - max_lod_level: maximum level of detail recorded for point-cloud extraction; ovclash
 *    stores it verbatim and gives no value, -1 included, a special meaning. The default,
 *    OVCLASH_SUITE_POINT_CLOUD_SETTINGS_MAX_LOD_LEVEL (1000000, see
 *    ovclash_suite_get_named_constant), is effectively unlimited.
 *  String fields are borrowed on output and copied on input.
 */
typedef struct ovclash_suite_point_cloud_settings_view_t
{
    int64_t identifier;
    ovclash_suite_string_t point_cloud_prim_path;
    ovclash_suite_string_t destination_path;
    uint32_t save_to_session_layer;
    uint32_t skip_rendering;
    int64_t max_lod_level;
} ovclash_suite_point_cloud_settings_view_t;

/** Persistent clash-info database record view.
 *  Field catalogue:
 *  - identifier: persistent overlap id, or -1 before insert.
 *  - query_id: owning query id.
 *  - overlap_id: legacy overlap hash string.
 *  - overlap_type: ovclash_suite_overlap_type_t value.
 *  - present: 0/1 presence flag.
 *  - min_distance, max_local_depth, depth_epsilon, tolerance: source scene-unit
 *    distances/depths/tolerances.
 *  - object_a_path/object_b_path: persisted source prim paths.
 *  - object_a_mesh_crc/object_b_mesh_crc: persisted mesh hash strings.
 *  - start_time/end_time: first/last source sample time in seconds.
 *  - num_records, overlap_elements: frame/element counts. Overlaps written by
 *    ovclash_suite_runtime_save_overlap_to_data store the number of records that
 *    actually reported, which can be fewer than the engine's record count.
 *  - state: ovclash_suite_clash_state_t value.
 *  - priority: legacy priority integer.
 *  - person_in_charge, creation_timestamp, last_modified_timestamp,
 *    last_modified_by, comment: user/editable metadata strings.
 *  - penetration_depth_px/nx/py/ny/pz/nz: per-axis source scene-unit
 *    penetration depths.
 *  - frame_count: on output, the number of stored frame records. On input, the
 *    number of placeholder frame rows that ovclash_suite_data_insert_overlap creates.
 *    ovclash_suite_data_update_overlap also creates them while the overlap has no
 *    frame rows. A count above 1000000 that would be created is rejected.
 *  String fields are borrowed on output and copied on input.
 */
typedef struct ovclash_suite_clash_info_record_view_t
{
    int64_t identifier;
    int64_t query_id;
    ovclash_suite_string_t overlap_id;
    uint32_t overlap_type;
    uint32_t present;
    double min_distance;
    double max_local_depth;
    double depth_epsilon;
    double tolerance;
    ovclash_suite_string_t object_a_path;
    ovclash_suite_string_t object_a_mesh_crc;
    ovclash_suite_string_t object_b_path;
    ovclash_suite_string_t object_b_mesh_crc;
    double start_time;
    double end_time;
    uint32_t num_records;
    uint32_t overlap_elements;
    uint32_t state;
    uint32_t priority;
    ovclash_suite_string_t person_in_charge;
    ovclash_suite_string_t creation_timestamp;
    ovclash_suite_string_t last_modified_timestamp;
    ovclash_suite_string_t last_modified_by;
    ovclash_suite_string_t comment;
    double penetration_depth_px;
    double penetration_depth_nx;
    double penetration_depth_py;
    double penetration_depth_ny;
    double penetration_depth_pz;
    double penetration_depth_nz;
    uint32_t frame_count;
} ovclash_suite_clash_info_record_view_t;

/** Persistent clash-frame database record view.
 *  Field catalogue:
 *  - identifier: persistent frame id, or -1 before insert.
 *  - clash_info_id: owning overlap id.
 *  - timecode: source sample time in seconds.
 *  - min_distance, max_local_depth: source scene-unit distances/depths.
 *  - overlap_elements: collided element count for this frame.
 *  - nb_usd_faces0/usd_faces0 and nb_usd_faces1/usd_faces1: authored USD face-index
 *    buffers; pointer may be null only when the matching count is 0.
 *  - nb_collision_outline_values/collision_outline_xyzxyz: outline float
 *    values in xyzxyz order, from hard triangle pairs only; clearance near-misses
 *    add none. The pointer may be null only when count is 0.
 *  - object_0_matrix/object_1_matrix: row-major 4x4 local-to-world transforms
 *    with 16 values. In-memory frame models copy these values without finite
 *    validation; JSON persistence rejects non-finite values.
 *  - penetration_depth_px/nx/py/ny/pz/nz: per-axis source scene-unit
 *    penetration depths.
 *  - nb_usd_points0/usd_points0: point index buffer for point-cloud clashes.
 *  - nb_usd_points0_sizes/usd_points0_sizes: point-size buffer with its own count;
 *    the pointer may be null only when this count is 0. Runtime-generated frames
 *    store one cloud-wide width when point indices are present. That width is the
 *    0.0 sentinel when the scalar width varies or is unavailable. Imported records
 *    may carry per-point widths, so never infer this count from nb_usd_points0.
 *  Pointer fields are borrowed on output and copied on input.
 */
typedef struct ovclash_suite_clash_frame_info_record_view_t
{
    int64_t identifier;
    int64_t clash_info_id;
    double timecode;
    double min_distance;
    double max_local_depth;
    uint32_t overlap_elements;

    uint32_t nb_usd_faces0;
    const uint32_t* usd_faces0;
    uint32_t nb_usd_faces1;
    const uint32_t* usd_faces1;
    uint32_t nb_collision_outline_values;
    const float* collision_outline_xyzxyz;
    double object_0_matrix[16];
    double object_1_matrix[16];

    double penetration_depth_px;
    double penetration_depth_nx;
    double penetration_depth_py;
    double penetration_depth_ny;
    double penetration_depth_pz;
    double penetration_depth_nz;

    uint32_t nb_usd_points0;
    const uint32_t* usd_points0;
    uint32_t nb_usd_points0_sizes;
    const float* usd_points0_sizes;
} ovclash_suite_clash_frame_info_record_view_t;

/** Clash-state aggregate count for one query.
 *  Field catalogue:
 *  - state: ovclash_suite_clash_state_t bucket value.
 *  - count: number of overlaps in that state for the queried query id.
 */
typedef struct ovclash_suite_state_count_view_t
{
    uint32_t state;
    uint32_t count;
} ovclash_suite_state_count_view_t;

/** Export column definition.
 *  Field catalogue:
 *  - order: zero-based index of this column's cell within each row. JSON and
 *    HTML export both read it as row_values[row * column_count + order]. Must be
 *    less than column_count. Columns are emitted in array order; order does not
 *    reorder them. Several columns may name the same cell.
 *  - name: copied input column label. HTML export renders every cell of a column
 *    named exactly Image as an image whose source is the cell value.
 *  - right_aligned: non-zero right-aligns the column in HTML export; JSON export
 *    ignores it.
 */
typedef struct ovclash_suite_export_column_t
{
    uint32_t order;
    ovclash_suite_string_t name;
    uint32_t right_aligned;
} ovclash_suite_export_column_t;

/** Name/value metadata item passed to export functions.
 *  Field catalogue:
 *  - name: copied UTF-8 metadata key.
 *  - value: copied UTF-8 metadata value.
 *  Strings are copied by the exporter during the call.
 */
typedef struct ovclash_suite_export_info_t
{
    ovclash_suite_string_t name;
    ovclash_suite_string_t value;
} ovclash_suite_export_info_t;

#ifdef __cplusplus
}
#endif
