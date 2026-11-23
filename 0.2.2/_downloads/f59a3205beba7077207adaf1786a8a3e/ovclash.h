// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

// OVCLASH_API is defined only in ovclash/ovclash_export.h.
#include "ovclash/ovclash_export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__has_include)
    #if __has_include(<ovx/string_types.h>)
        #include <ovx/string_types.h>
    #elif __has_include(<ovx/types.h>)
        #include <ovx/types.h>
    #else
        #error "ovclash requires ovx/string_types.h or ovx/types.h"
    #endif
#else
    #include <ovx/types.h>
#endif

#if OVCLASH_STANDALONE
// DLPack provides the zero-copy array interop that the Omniverse library guidelines require. Like
// <ovx/string_types.h>, the header belongs to the ovstage SDK, and ovclash keeps no copy of it. The
// public ABI reaches it, so the ovclash install step ships it in the SDK include tree. It is the
// only OVX header that ovclash installs.
    #include <ovx/dlpack/dlpack.h>
#endif // OVCLASH_STANDALONE

/**
 * @defgroup ovclash_c_api Canonical clash C API
 *
 * The lower-level public C ABI: context lifetime, settings, scope, pipeline execution and
 * result retrieval. `ovclash_suite` is the product-level layer built on top of this one.
 * Consumers pick a layer by need; neither is deprecated.
 *
 * Every entry point in this group returns ovclash_result_t, except three string getters:
 * ovclash_get_result_string(), ovclash_get_last_error() and ovclash_get_version_string(). The
 * first two are the error-reporting channel itself, so giving them a status to check would be
 * circular. The third takes no context and cannot fail. Values travel in out-parameters.
 *
 * Test a result as `result.status == OVCLASH_STATUS_SUCCESS`. OVCLASH_STATUS_SUCCESS is 0, so a
 * truthiness test of a status means the opposite of what it reads as.
 *
 * @{
 */

typedef ovx_string_t ovclash_string_t;

// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_status_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_STATUS_SUCCESS = 0,
    OVCLASH_STATUS_ERROR = 1
} ovclash_status_t;

// Capability and load-path information about the active backend. Both backends provide it, so it
// sits outside the OVCLASH_STANDALONE fence. backend_name borrows a static library-owned literal.
typedef struct ovclash_backend_info_t
{
    ovclash_string_t backend_name;
    uint32_t native_openusd_linked;
    uint32_t ovpopulation_runtime_loaded;
    uint32_t ovstage_cuda_enabled;
} ovclash_backend_info_t;

/**
 * @brief Result of a canonical ovclash entry point.
 *
 * The struct holds one ovclash_status_t field named status. Test it as
 * `result.status == OVCLASH_STATUS_SUCCESS`. OVCLASH_STATUS_SUCCESS is 0, so a truthiness test of
 * the status reverses the success check.
 *
 * The struct owns no memory and is safe to copy by value. It carries no detail: where a context
 * exists, ovclash_get_last_error() returns the diagnostic. ovclash_suite_result_t has the same
 * shape, so both public C layers report results the same way.
 */
typedef struct ovclash_result_t
{
    ovclash_status_t status;
} ovclash_result_t;

// Canonical clash C ABI. The OpenUSD backend routes it through IClashDetection2; the ovstage
// backend implements it directly.

// Engine report flags use OVCLASH_OVERLAP_REPORT_FLAG_* names. The Suite and Python aliases
// without the FLAG infix are Suite named constants, not members of this engine enum.

typedef struct ovclash_context_t ovclash_context_t;
typedef struct ovclash_async_t ovclash_async_t;

typedef enum ovclash_settings_flag_t
{
    OVCLASH_SETTINGS_FLAG_LOG = (1 << 0),
    OVCLASH_SETTINGS_FLAG_COPLANAR = (1 << 1),
    OVCLASH_SETTINGS_FLAG_ANY_HIT = (1 << 2),
    OVCLASH_SETTINGS_FLAG_QUANTIZED = (1 << 3),
    OVCLASH_SETTINGS_FLAG_TIGHT_BOUNDS = (1 << 4),
    OVCLASH_SETTINGS_FLAG_FULL_SCENE = (1 << 5),
    OVCLASH_SETTINGS_FLAG_ONLY_FIND_DUPLICATES = (1 << 6),
    OVCLASH_SETTINGS_FLAG_IGNORE_REDUNDANT_OVERLAPS = (1 << 7),
    OVCLASH_SETTINGS_FLAG_IGNORE_INVISIBLE_PRIMS = (1 << 8),
    OVCLASH_SETTINGS_FLAG_ENABLE_POINT_CLOUDS = (1 << 9),
    OVCLASH_SETTINGS_FLAG_ENABLE_POINT_CLOUDS_EXPERIMENTAL = (1 << 10),
    OVCLASH_SETTINGS_FLAG_SINGLE_THREADED = (1 << 11),
    OVCLASH_SETTINGS_FLAG_NEW_TASK_MANAGER = (1 << 12),
    OVCLASH_SETTINGS_FLAG_PURGE_PERMANENT_DYNAMIC_OVERLAPS = (1 << 13),
    OVCLASH_SETTINGS_FLAG_PURGE_PERMANENT_STATIC_OVERLAPS = (1 << 14),
    OVCLASH_SETTINGS_FLAG_SUBCOMPONENT_FILTERING = (1 << 15),
    OVCLASH_SETTINGS_FLAG_USDRT = (1 << 16),

    // Mirrors the Kit product default table, ClashDetection.DEFAULT_SETTINGS in
    // physxclashdetectioncore/clash_detect.py of the omni.physx.clashdetection.core extension.
    // Kit turns on COPLANAR, TIGHT_BOUNDS, IGNORE_INVISIBLE_PRIMS, NEW_TASK_MANAGER,
    // PURGE_PERMANENT_STATIC_OVERLAPS and USE_USDRT. Kit turns off LOGGING, SINGLE_THREADED,
    // IGNORE_REDUNDANT_OVERLAPS, PURGE_PERMANENT_OVERLAPS (the dynamic one), ANY_HIT, QUANTIZED,
    // DUP_MESHES, ENABLE_POINT_CLOUDS and FILTER_TEST. FULL_SCENE is an ovclash scoping flag with
    // no Kit counterpart; it is on. ovclash_create_context() seeds a fresh context from this
    // table, so the two agree.
    OVCLASH_SETTINGS_FLAG_DEFAULT = OVCLASH_SETTINGS_FLAG_COPLANAR | OVCLASH_SETTINGS_FLAG_TIGHT_BOUNDS |
                                    OVCLASH_SETTINGS_FLAG_FULL_SCENE | OVCLASH_SETTINGS_FLAG_IGNORE_INVISIBLE_PRIMS |
                                    OVCLASH_SETTINGS_FLAG_NEW_TASK_MANAGER |
                                    OVCLASH_SETTINGS_FLAG_PURGE_PERMANENT_STATIC_OVERLAPS | OVCLASH_SETTINGS_FLAG_USDRT
} ovclash_settings_flag_t;

// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_max_local_depth_mode_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_MAX_LOCAL_DEPTH_MODE_DISABLED, // Max local depth computation is disabled.
    OVCLASH_MAX_LOCAL_DEPTH_MODE_MINIMAL, // Original fast estimate.
    OVCLASH_MAX_LOCAL_DEPTH_MODE_MEDIUM, // Refined estimate, medium accuracy; slower than MINIMAL.
    OVCLASH_MAX_LOCAL_DEPTH_MODE_HIGH, // Refined estimate, higher accuracy; slower than MEDIUM.
} ovclash_max_local_depth_mode_t;

typedef struct ovclash_settings_t
{
    uint32_t flags;
    uint32_t nbTasks;
    uint32_t sortMode;
    uint32_t overlapCode;
    uint32_t trisPerLeaf;
    uint32_t triangleLimit;
    uint32_t broadphaseLimit;
    float poseEpsilon;
    float boundsEpsilon;
    float areaEpsilon;
    float contactEpsilon;
    float contactCutoff;
    float tolerance;
    float pointSize;
    ovclash_max_local_depth_mode_t maxLocalDepthMode;
    double startTime; // Seconds, not USD time codes. Pose reads use the stage's rate for conversion.
    double endTime; // Seconds. startTime == endTime means a static (single-sample) query.
    double timeCodesPerSecond; // Query samples per second, not the stage's USD time-code rate.
} ovclash_settings_t;

typedef struct ovclash_float3_t
{
    float x; //!< x-component
    float y; //!< y-component
    float z; //!< z-component
} ovclash_float3_t;

typedef struct ovclash_segment_t
{
    ovclash_float3_t p0;
    ovclash_float3_t p1;
} ovclash_segment_t;

typedef struct ovclash_duplicate_mesh_data_t
{
    ovclash_string_t name0; // Name/path of first mesh
    ovclash_string_t name1; // Name/path of second mesh
    uint64_t meshHash0[2]; // Hash of first colliding mesh (hash of the mesh data)
    // Equals meshHash0 for a genuine duplicate pair.
    uint64_t meshHash1[2]; // Hash of second colliding mesh (hash of the mesh data)
    uint64_t clashHash[2]; // Hash of the clash itself (hash of the meshes' names)
    uint32_t nbTris; // Total number of triangles in the duplicated mesh
    double time; // Overlap time on timeline
} ovclash_duplicate_mesh_data_t;

typedef struct ovclash_depth_data_t
{
    // Maximum local depth recorded between pairs of triangles, in stage units (metersPerUnit
    // applies). -1.0f means "not computed". ovclash_reset_depth_data writes -1.0f. No entry point
    // fills this struct; ovclash_compute_max_local_depth() writes a bare float. A computed value is
    // never negative.
    float maxLocalDepth;
} ovclash_depth_data_t;

typedef struct ovclash_depenetration_data_t
{
    ovclash_float3_t dir; // Depenetration direction
    float depth; // Penetration depth
} ovclash_depenetration_data_t;

typedef enum ovclash_overlap_pair_type_t
{
    OVCLASH_OVERLAP_PAIR_TYPE_MESH_VS_MESH = 0, // Mesh-vs-mesh pair
    OVCLASH_OVERLAP_PAIR_TYPE_POINT_VS_MESH = 1, // Point-cloud-vs-mesh pair

    // Test a pair type as "== OVCLASH_OVERLAP_PAIR_TYPE_POINT_VS_MESH", never as
    // "!= OVCLASH_OVERLAP_PAIR_TYPE_MESH_VS_MESH": the undefined value is not zero.
    OVCLASH_OVERLAP_PAIR_TYPE_UNDEFINED = 0x7fffffff
} ovclash_overlap_pair_type_t;

typedef struct ovclash_overlap_data_t
{
    // The same for every record of the pair.
    ovclash_string_t name0; // Name/path of first prim (point-cloud or mesh)
    ovclash_string_t name1; // Name/path of second prim (mesh)
    uint64_t meshHash0[2]; // Hash of first colliding mesh (hash of the mesh data)
    uint64_t meshHash1[2]; // Hash of second colliding mesh (hash of the mesh data)
    uint64_t clashHash[2]; // Hash of the clash itself (hash of the meshes' names)
    uint32_t nbRecords; // Number of frames the pair overlaps (always 1 for static clash detection)
    uint32_t type; // ovclash_overlap_pair_type_t

    // Specific to one record.
    uint32_t nbElements; // Number of overlapping elements (triangles or points) for current frame
    double time; // Overlap time on the timeline, in seconds (not USD time codes)
    // Minimum distance recorded between pairs of triangles, in stage units (metersPerUnit
    // applies). 0 when the triangles touch or interpenetrate.
    float minDistance;
    // Maximum local depth recorded between pairs of triangles, in stage units (metersPerUnit
    // applies). -1.0f means "not computed". For a mesh-vs-mesh overlap both backends leave -1.0f
    // unless ovclash_set_compute_max_local_depth selected a mode above DISABLED, the default. A
    // computed value is never negative. A point-vs-mesh overlap always reports 0.0f, in every
    // mode, because depth is mesh-only.
    float maxLocalDepth;
} ovclash_overlap_data_t;

typedef struct ovclash_step_data_t
{
    ovclash_string_t name; // Name of the step
    uint32_t stepIndex; // Current step
    uint32_t nbSteps; // Number of steps
    // Pipeline progress, nominally 0.0..1.0. An OpenUSD async poll may briefly report a value
    // outside that range or a non-finite one. Consumers clamp it and treat a non-finite value as
    // unknown.
    float progress;
    float stepProgress; // Step progress, nominally 0.0..1.0, with the same OpenUSD async caveat.
    bool finished; // True for the last step
} ovclash_step_data_t;

typedef struct ovclash_timeline_data_t
{
    // Start time, in seconds like the Kit timeline this struct mirrors. Both standalone backends
    // return fixed values: 0.0 here and in the next two fields, 24.0 in timeCodesPerSecond.
    double startTime;
    double endTime; // End time (fixed 0.0).
    double currentTime; // Current time (fixed 0.0).
    double timeCodesPerSecond; // Time codes per second (fixed 24.0), the conversion to time codes
} ovclash_timeline_data_t;

typedef struct ovclash_stage_timeline_data_t
{
    double startTimeCode; // Stage startTimeCode as USD resolves it (0.0 when unauthored)
    double endTimeCode; // Stage endTimeCode as USD resolves it (0.0 when unauthored)
    double timeCodesPerSecond; // Effective stage rate: timeCodesPerSecond, then framesPerSecond, then 24.0
    double framesPerSecond; // Stage framesPerSecond as USD resolves it (24.0 when unauthored)
} ovclash_stage_timeline_data_t;

typedef struct ovclash_stats_t
{
    uint32_t nbMeshes;
    uint32_t nbSharedMeshes;
    /**
     * Unique source meshes successfully cooked for this run; on OpenUSD, the number of cooks
     * attempted. Shared meshes are cooked once, so a healthy run reports nbMeshes minus
     * nbSharedMeshes. **A lower nbCookedMeshes means geometry is missing from the results.** A
     * mesh that PhysX cannot cook, for example because of degenerate triangles, is dropped from
     * every phase after cooking. The pipeline step does not fail, so one unusable mesh does not
     * discard the rest. Such a run still reports success and still returns overlaps. The overlaps
     * of the dropped meshes are absent, which looks exactly like a clean scene.
     *
     * The comparison is an ovstage signal: on OpenUSD a failed cook never lowers this count. On
     * ovstage, compare the counts, then read ovclash_get_last_error(). A mesh that PhysX could not
     * cook records the number of dropped meshes and the first of them, whatever the logging flag.
     * A mesh whose triangulation produced no triangles is dropped with only a log line. Enable
     * logging to name every dropped mesh.
     */
    uint32_t nbCookedMeshes;
    uint32_t nbPointClouds;
} ovclash_stats_t;

// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_mesh_index_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_MESH_INDEX_0, // First mesh of the pair
    OVCLASH_MESH_INDEX_1 // Second mesh of the pair
} ovclash_mesh_index_t;

typedef enum ovclash_overlap_report_flag_t
{
    OVCLASH_OVERLAP_REPORT_FLAG_DEFAULT =
        0, // ovclash_create_overlap_report reads 0 as FACES; the _ex variant ignores it
    OVCLASH_OVERLAP_REPORT_FLAG_FACES =
        (1 << 0), // Report USD indices of colliding faces for mesh specified by mesh index (legacy)
    OVCLASH_OVERLAP_REPORT_FLAG_OUTLINE = (1 << 1), // Report collision outline segments (hard intersections only)
    OVCLASH_OVERLAP_REPORT_FLAG_FACES0 =
        (1 << 2), // Report USD indices of colliding faces for mesh 0 (mesh index is ignored)
    OVCLASH_OVERLAP_REPORT_FLAG_FACES1 =
        (1 << 3), // Report USD indices of colliding faces for mesh 1 (mesh index is ignored)
    OVCLASH_OVERLAP_REPORT_FLAG_POINTS0 = (1 << 4), // Report USD indices of colliding points (for point cloud 0)
} ovclash_overlap_report_flag_t;

typedef struct ovclash_overlap_report_t
{
    // Collision faces, filled when OVCLASH_OVERLAP_REPORT_FLAG_FACES or
    // OVCLASH_OVERLAP_REPORT_FLAG_FACES0 is set, or when flags is 0 in
    // ovclash_create_overlap_report.
    uint32_t nbCollidingFaces0; // Number of colliding faces
    const uint32_t* collidingFaces0; // Indices of colliding faces (USD indices)

    // Collision faces, filled when OVCLASH_OVERLAP_REPORT_FLAG_FACES1 is set.
    uint32_t nbCollidingFaces1; // Number of colliding faces
    const uint32_t* collidingFaces1; // Indices of colliding faces (USD indices)

    // Collision outline, filled when OVCLASH_OVERLAP_REPORT_FLAG_OUTLINE is set. Segments come only
    // from hard triangle pairs, which touch or interpenetrate at zero distance. Tolerance
    // near-misses add faces and elements but no segments, so an overlap made only of clearance
    // reports nbCollidingSegments == 0. Each segment points along a geometric interval axis whose
    // first nonzero x, y or z component is negative. Its endpoints belong to no mesh side and
    // follow no winding.
    uint32_t nbCollidingSegments; // Number of colliding segments in outline
    const ovclash_segment_t* collidingSegments; // Collision outline

    // Clashing points, filled when OVCLASH_OVERLAP_REPORT_FLAG_POINTS0 is set.
    // pointSize0 is the first of these that applies:
    // - a copy of the explicit size passed to ovclash_set_point_size();
    // - a copy of the single point width authored in the USD data of the point cloud;
    // - 0.0 when the point cloud authors per-point widths. The caller then reads each width from
    //   the USD data by the point indices in collidingPoints0. ovstage collapses a uniform
    //   per-point array back to its single value, where the OpenUSD parity backend reports 0.0;
    // - a point size computed automatically for the cloud with the formula of the renderer.
    // The value never includes scaling from the prim transform, because that scaling cannot apply
    // to the per-point widths, which the report does not return.
    uint32_t nbCollidingPoints0; // Number of colliding points
    float pointSize0; // Point size for collidingPoints0 (twice the radius, does not include scaling)
    const uint32_t* collidingPoints0; // Indices of colliding points (USD indices)
} ovclash_overlap_report_t;

// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_depenetration_axis_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_DEPENETRATION_AXIS_WORLD_X, // World-space X axis (1;0;0)
    OVCLASH_DEPENETRATION_AXIS_WORLD_Y, // World-space Y axis (0;1;0)
    OVCLASH_DEPENETRATION_AXIS_WORLD_Z, // World-space Z axis (0;0;1)
    OVCLASH_DEPENETRATION_AXIS_WORLD_NEG_X, // World-space negative X axis (-1;0;0)
    OVCLASH_DEPENETRATION_AXIS_WORLD_NEG_Y, // World-space negative Y axis (0;-1;0)
    OVCLASH_DEPENETRATION_AXIS_WORLD_NEG_Z, // World-space negative Z axis (0;0;-1)
    OVCLASH_DEPENETRATION_AXIS_MOTION_VECTOR, // Estimated motion vector (for dynamic clashes)
    OVCLASH_DEPENETRATION_AXIS_USER_DEFINED // User-defined (explicit) axis
} ovclash_depenetration_axis_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Returns a static, library-owned name for a result status.
 *
 * Maps OVCLASH_STATUS_SUCCESS to "OVCLASH_STATUS_SUCCESS" and every other value, including values
 * outside the enum, to "OVCLASH_STATUS_ERROR". Both backends return byte-identical literals. The
 * text names the status and does not describe a failure. ovclash_get_last_error() returns the
 * concrete reason; it exists only in OVCLASH_STANDALONE builds.
 *
 * @param result [in] Status to describe.
 * @return Borrowed view over a static string literal. Never null and never empty.
 *
 * @note Lifetime: valid for the process lifetime; must not be freed or modified.
 * @note Threading: safe to call concurrently; reads no context and no mutable global state.
 */
OVCLASH_API ovclash_string_t ovclash_get_result_string(ovclash_result_t result);

/**
 * @brief Reports which clash backend this library was compiled with, plus its capabilities.
 *
 * The OpenUSD backend writes backend_name "openusd", native_openusd_linked 1,
 * ovpopulation_runtime_loaded 0, ovstage_cuda_enabled 0. The ovstage backend writes "ovstage",
 * 0, 1, and ovstage_cuda_enabled 1 only when built with OVCLASH_OVSTAGE_CUDA_ENABLED. Every
 * value is a compile-time constant: nothing is probed at run time and no context is needed.
 *
 * @param out_info [out] Receives the backend name and capability flags. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if out_info is null or, on ovstage, if
 *         the call runs inside an owner-operation callback on the same thread (see
 *         ovclash_ovstage_run_owner_operation). Nothing is written on error.
 *
 * @note Lifetime: backend_name borrows a static literal valid for the process lifetime.
 * @note Threading: safe to call concurrently.
 */
OVCLASH_API ovclash_result_t ovclash_get_backend_info(ovclash_backend_info_t* out_info);

/**
 * @brief Fills a settings struct with the Kit-compatible ovclash default table.
 *
 * Writes flags = OVCLASH_SETTINGS_FLAG_DEFAULT, nbTasks 128, sortMode 0, overlapCode 3,
 * trisPerLeaf 15, triangleLimit 0, broadphaseLimit 0xffffffff, poseEpsilon 1e-6f,
 * boundsEpsilon 0.01f, areaEpsilon 1e-6f, contactEpsilon -1.0f, contactCutoff -1.0f,
 * tolerance 0.0f, pointSize 0.0f, maxLocalDepthMode DISABLED and all three times 0.0. Both
 * backends produce identical values.
 *
 * This is the Kit product default table, ClashDetection.DEFAULT_SETTINGS in the
 * omni.physx.clashdetection.core extension. A fresh context holds exactly these values, because
 * ovclash_create_context() seeds it from this function. The settings of a fresh context and a
 * reset struct therefore agree field for field. This function touches no context.
 *
 * @param settings [out] Struct to fill. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if settings is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through settings.
 */
OVCLASH_API ovclash_result_t ovclash_reset_settings(ovclash_settings_t* settings);
/**
 * @brief Clears a duplicate-mesh record to a defined empty state.
 *
 * Sets both name views to {null, 0} and zeroes meshHash0, meshHash1, clashHash, nbTris and time.
 * Identical in both backends. The name views are borrowed from a context, so this only drops the
 * views and never frees anything.
 *
 * @param data [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if data is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through data.
 */
OVCLASH_API ovclash_result_t ovclash_reset_duplicate_mesh_data(ovclash_duplicate_mesh_data_t* data);
/**
 * @brief Clears a depth record, setting maxLocalDepth to -1.0f ("not computed").
 *
 * Identical in both backends. No entry point in this header consumes or produces an
 * ovclash_depth_data_t: ovclash_compute_max_local_depth() writes a bare float. This helper
 * therefore serves a struct that the ABI does not otherwise use.
 *
 * @param data [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if data is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through data.
 */
OVCLASH_API ovclash_result_t ovclash_reset_depth_data(ovclash_depth_data_t* data);
/**
 * @brief Clears a depenetration record: dir to (0,0,0) and depth to -1.0f ("not computed").
 *
 * Identical in both backends. No entry point in this header consumes or produces an
 * ovclash_depenetration_data_t: ovclash_depenetrate_meshes() and ovclash_imm_depenetrate_meshes()
 * write an ovclash_float3_t displacement. This helper therefore serves a struct that the ABI does
 * not otherwise use.
 *
 * @param data [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if data is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through data.
 */
OVCLASH_API ovclash_result_t ovclash_reset_depenetration_data(ovclash_depenetration_data_t* data);
/**
 * @brief Clears an overlap record to a defined empty state.
 *
 * Clears both name views and all three hashes, and zeroes nbRecords, nbElements, time and
 * minDistance. Sets maxLocalDepth to -1.0f ("not computed") and type to
 * OVCLASH_OVERLAP_PAIR_TYPE_UNDEFINED (0x7fffffff, not zero). Identical in both backends. Call
 * this before ovclash_get_overlap_data(). That function leaves *data untouched when it fails, so
 * a struct that was not reset cannot be told apart from a filled one.
 *
 * @param data [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if data is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through data.
 */
OVCLASH_API ovclash_result_t ovclash_reset_overlap_data(ovclash_overlap_data_t* data);
/**
 * @brief Clears a pipeline step record.
 *
 * Sets name to {null, 0}, stepIndex and nbSteps to 0, progress and stepProgress to 0.0f, and
 * finished to **true**. The cleared state therefore reads as "finished", not "not started", so
 * `finished` must not be used as an initialized-or-not test. Identical in both backends.
 *
 * @param data [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if data is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through data.
 */
OVCLASH_API ovclash_result_t ovclash_reset_step_data(ovclash_step_data_t* data);
/**
 * @brief Zeroes a query-statistics record (meshes, shared meshes, cooked meshes, point clouds).
 *
 * Identical in both backends. It is only a struct initializer and clears no counter held by a
 * context. ovclash_reset_context() does that.
 *
 * @param stats [out] Record to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if stats is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through stats.
 */
OVCLASH_API ovclash_result_t ovclash_reset_stats(ovclash_stats_t* stats);
/**
 * @brief Clears an overlap-report view without releasing anything.
 *
 * Zeroes the four counts and pointSize0 and nulls the four buffer pointers. It never frees
 * memory, which means different things on the two backends. On OpenUSD the engine allocates the
 * buffers, and only ovclash_release_overlap_report() frees them. Calling this on a filled report
 * leaks them, and releasing the context does not free them either. On ovstage the context owns
 * the buffers and the report only borrows them, so dropping the view is harmless. Use this
 * function to initialize a report before ovclash_create_overlap_report().
 *
 * @param report [out] Report view to clear. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if report is null (nothing written).
 *
 * @note Threading: safe to call concurrently; writes only through report.
 */
OVCLASH_API ovclash_result_t ovclash_reset_overlap_report(ovclash_overlap_report_t* report);

/**
 * @brief Creates a clash context, the owner of settings, scope, results and error text.
 *
 * The OpenUSD backend bootstraps Carbonite before it creates the engine instance, because report
 * creation dereferences the cached tasking interface. The ovstage backend initializes Carbonite
 * lazily, when a phase needs parallel work with the new task manager. Both backends seed a fresh
 * context from ovclash_reset_settings(). OpenUSD pushes that table through
 * ovclash_set_settings(); ovstage calls it from the context constructor. A fresh context
 * therefore holds exactly the ovclash_reset_settings() table. In particular,
 * OVCLASH_SETTINGS_FLAG_SINGLE_THREADED is off and OVCLASH_SETTINGS_FLAG_NEW_TASK_MANAGER is on.
 *
 * @param out_context [out] Receives the new context. Must not be null; free with
 *                          ovclash_release_context().
 * @return OVCLASH_STATUS_SUCCESS on success. OVCLASH_STATUS_ERROR if out_context is null, the
 *         Carbonite bootstrap fails (OpenUSD) or an allocation fails. On failure *out_context is
 *         left untouched, so initialize it to null before calling.
 *
 * @note Lifetime: releasing the context invalidates its borrowed views, async handles and
 *       results. Release every overlap report first; OpenUSD report buffers require explicit
 *       release and are not reclaimed with the context.
 * @note Threading: drive one context from one thread at a time. On ovstage, independent contexts
 *       may be created and used concurrently. On the OpenUSD parity backend they may not:
 *       cancellation and the out-of-memory condition are process-global there (see
 *       ovclash_cancel_pipeline and ovclash_is_out_of_memory), so concurrent contexts interfere
 *       and must be serialized. That backend also shares one process-wide Carbonite bootstrap,
 *       which the first context creates and which lasts for the process.
 */
OVCLASH_API ovclash_result_t ovclash_create_context(ovclash_context_t** out_context);
/**
 * @brief Destroys a context and everything it owns.
 *
 * A null context returns OVCLASH_STATUS_ERROR. While an async pipeline handle is outstanding, the
 * call blocks until the run completes. The OpenUSD backend finishes the run and frees the handle;
 * the ovstage backend joins the worker thread in the context destructor. A release therefore
 * never races a running pipeline, but it can block for the rest of the run. The OpenUSD backend
 * also removes the stage of the context from the standalone stage registry and keeps the
 * process-wide Carbonite bootstrap. Releasing any OpenUSD context also clears the process-global
 * cancellation and out-of-memory flags of that backend, including for runs of other contexts.
 *
 * @param context [in] Context from ovclash_create_context(), or null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null.
 *
 * @note Errors: the status is the only signal -- after the call there is no context left to query.
 * @note Lifetime: invalidates every borrowed string view, async handle and result of this
 *       context. On the OpenUSD backend this call does NOT free overlap-report buffers. Call
 *       ovclash_release_overlap_report() for each report first, or the memory is lost.
 * @note Threading: no other call on this context may be in flight.
 */
OVCLASH_API ovclash_result_t ovclash_release_context(ovclash_context_t* context);
/**
 * @brief Drops all results while keeping the context, its settings, scope, opened stage and
 * built pipeline.
 *
 * Both backends release meshes, cooked data, the BVH, broadphase pairs, overlaps, duplicates and
 * statistics, and clear the out-of-memory and cancelled flags. ovstage also resets its
 * completed-step counter and drops the populated ovstage scene, which the next pipeline run
 * repopulates from the stored stage path. ovstage also releases a stage borrowed with
 * ovclash_ovstage_attach_stage. Such a stage has no stored path, so attach it again before the
 * next run. Neither backend resets settings, scope paths or the opened stage; use
 * ovclash_set_settings() and ovclash_set_scope*() for that. The call is rejected while an async
 * pipeline handle is active.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or the call is
 *         rejected.
 *
 * @note Errors: rejection records text for ovclash_get_last_error()
 *       (OVCLASH_STANDALONE only; a successful call to this function preserves it).
 * @note Lifetime: invalidates every string view, overlap index and report obtained from earlier
 *       results. OpenUSD report buffers still need their own ovclash_release_overlap_report().
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_reset_context(ovclash_context_t* context);

/**
 * @brief Applies a whole settings struct at once, equivalent to calling every individual setter.
 *
 * Both backends apply the same validation. poseEpsilon, boundsEpsilon, areaEpsilon,
 * contactEpsilon, contactCutoff, tolerance, pointSize, startTime, endTime and timeCodesPerSecond
 * must be finite. timeCodesPerSecond must be >= 0, trisPerLeaf in 1..15 (PhysX itself validates
 * the BVH34 leaf count against [2,15]) and maxLocalDepthMode <= HIGH. On a violation nothing is
 * stored and the call fails. The OpenUSD backend then applies the struct through the individual
 * engine setters. Both backends clamp nbTasks to 256. Both also clear any scope paths set earlier;
 * the OpenUSD path goes through setFullScene(), which releases them whatever the flag value.
 *
 * @param context [in] Context from ovclash_create_context(). Must not be null.
 * @param settings [in] Settings to apply. Must not be null; the struct is copied.
 * @return OVCLASH_STATUS_SUCCESS on success. OVCLASH_STATUS_ERROR if context or settings is
 *         null, an async pipeline handle is active, or validation fails (both backends).
 *
 * @note Errors: an active async handle or a validation failure records its reason for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky); null arguments record nothing.
 * @note Results: changing settings neither recomputes nor discards completed results. The
 *       threading settings also govern later report extraction, so they can change after the
 *       pipeline finishes and before fetching. Finish an active async handle first.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_settings(ovclash_context_t* context, const ovclash_settings_t* settings);
/**
 * @brief Enables the backend's per-phase timing/progress logging to stdout.
 *
 * Both backends honor the flag and write with printf to stdout. There is no log callback and no
 * way to redirect the output. Default on a fresh context: disabled. The backends emit different
 * text ("--Serial: ..." and "--ovstage: ..."), so the output is not a stable format.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero enables logging.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_log(ovclash_context_t* context, bool flag);
/**
 * @brief Controls whether coplanar triangle pairs count as overlaps.
 *
 * Both backends map this onto the PhysX mesh-mesh query flags. Enabled selects
 * PxMeshMeshQueryFlag::eDEFAULT. Disabled adds eDISCARD_COPLANAR, so coplanar faces that touch
 * exactly are ignored. Default on a fresh context: enabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero keeps coplanar contacts; zero discards them.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_coplanar(ovclash_context_t* context, bool flag);
/**
 * @brief Stops each mesh pair at the first colliding triangle instead of enumerating all of them.
 *
 * Both backends force the per-pair triangle result limit to 1, which overrides
 * ovclash_set_triangle_limit(). Overlaps are still reported, but nbElements and the face and
 * outline report buffers then hold one representative hit. Derived values such as maxLocalDepth
 * come from that one pair only. Default on a fresh context: disabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero enables any-hit (first-hit) mode.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_any_hit(ovclash_context_t* context, bool flag);
/**
 * @brief Selects the quantized BVH34 midphase when cooking triangle meshes.
 *
 * Both backends set PxCookingParams::midphaseDesc.mBVH34Desc.quantized. Quantized trees use less
 * memory at some cost in accuracy and speed. Default on a fresh context: disabled. The cooking
 * phase reads the value, so it only affects meshes cooked by the next pipeline run.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero cooks quantized midphase trees.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_quantized(ovclash_context_t* context, bool flag);
/**
 * @brief Requests tight (rotated-mesh-aware) world bounds instead of cheap loose bounds.
 *
 * Both backends map this to PxMeshGeometryFlag::eTIGHT_BOUNDS when they compute per-mesh world
 * bounds. Tight bounds shrink the broadphase candidate set for rotated meshes, at some extra cost
 * in bounds computation. Default on a fresh context: enabled. The value is read while bounds are
 * computed, so it applies from the next pipeline run.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero enables tight bounds.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_tight_bounds(ovclash_context_t* context, bool flag);
/**
 * @brief Skips clash pairs whose two prims belong to the same USD subcomponent.
 *
 * Both backends drop a broadphase pair when the two meshes resolve to the same subcomponent root
 * prim, so parts of one assembly do not clash with each other. Two meshes outside any
 * subcomponent resolve to the same empty root and are dropped too. Enabling the filter therefore
 * suppresses every pair between meshes that sit outside any subcomponent.
 *
 * OpenUSD resolves the root with UsdUtils::getSubComponentRoot() behind
 * SUPPORT_EXPERIMENTAL_FILTERING, which the ovclash build turns on by default. ovstage resolves
 * the nearest ancestor with `kind = "subcomponent"` from the composed model kinds that it
 * populates from the opened file. Kinds authored in referenced, sublayered or binary layers, or
 * in a selected variant, therefore count as they do on OpenUSD. A mesh of an instanced asset
 * resolves per placement, like an OpenUSD instance proxy: from the asset content up through its
 * instance root and the ancestors of that root. A stage attached with
 * ovclash_ovstage_attach_stage() is read as its owner populated it. When the owner population
 * holds no `kind` (or `usd-metadata:kind`) values, every mesh resolves to the empty root, and
 * enabling the filter drops every pair.
 *
 * Default on a fresh context: disabled. Applies from the next pipeline run. On ovstage, a call
 * between the steps of a run leaves the rest of that run unchanged, because ovstage fixes the
 * value when the run selects meshes. OpenUSD applies it to the remaining steps of the run.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero enables subcomponent filtering.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: a null context is reported through the status and records no text anywhere; while
 *       an async pipeline handle is active the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only).
 *       A successful call to this function leaves that message unchanged.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_subcomponent_filtering(ovclash_context_t* context, bool flag);
/**
 * @brief Stores the USDRT flag, which selects nothing in either ovclash backend.
 *
 * In the Kit extension this bit selects the USDRT/Fabric stage traversal. In ovclash it is inert.
 * The only reader on the OpenUSD path sits inside `#if !defined(OVCLASH_STANDALONE)` and is
 * compiled out. The ovstage backend has no second traversal to switch to, because ovstage
 * population is its only scene path, so the flag has no reader there at all. Toggling it
 * produces byte-identical results on both backends. Default on a fresh context: enabled. The
 * flag remains because it belongs to the Kit-compatible settings table that persisted query
 * settings serialize. ovclash_get_settings() reports the stored bit, so the only observable
 * effects are that read-back and the serialized text, never the clash results.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Stored in the settings bitmask; otherwise ignored.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_use_usdrt(ovclash_context_t* context, bool flag);
/**
 * @brief Forces serial execution of the clash phases.
 *
 * Both backends honor it. On OpenUSD it feeds MultithreadParams::mSingleThreaded, and
 * multi-threaded detection also requires the Carbonite tasking interface. On ovstage it
 * overrides both task managers: geometry preparation, mesh cooking, pose and bounds computation,
 * broad phase and narrow phase run serially. It also turns off sampled-matrix prefetch and the
 * numeric-population helper thread described below.
 *
 * With the flag off, ovclash_set_new_task_manager() selects Carbonite tasking or bounded
 * std::thread workers on ovstage. Eligible dynamic playback can then prepare a future matrix
 * window while collision work reads the current one. Source updates and numeric composition stay
 * sequential, with one source owner at a time. The one exception is a file-backed static run: it
 * can open its numeric transform population on one helper std::thread while mesh selection keeps
 * extracting geometry. Neither task manager nor ovclash_set_nb_tasks() governs that thread. The
 * thread holds the process-wide ovstage lifecycle gate for the population. It is joined before
 * those transforms are read and before the selection is replaced, reset or released.
 *
 * On ovstage, pairs are tested independently, and their overlaps are sorted back into canonical
 * mesh-path order after parallel collection. Counts, membership, per-pair data and even the
 * ovclash_get_overlap_data() index order are therefore identical to a single-threaded run.
 * Default on a fresh context: disabled on both backends, as in the Kit product table
 * (SETTING_SINGLE_THREADED: False). A standalone OpenUSD caller that hits instability in the
 * carb-tasking paths of the engine can turn it on. The flag also makes later report extraction
 * run serially. It can change after detection finishes, without rerunning the pipeline or
 * discarding its results.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero forces serial execution.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_single_threaded(ovclash_context_t* context, bool flag);
/**
 * @brief Selects the task-manager implementation for multi-threaded phases.
 *
 * On ovstage, true selects the statically linked Carbonite tasking for geometry preparation,
 * mesh cooking, pose and bounds computation, broad phase, narrow phase and eligible
 * sampled-matrix prefetch. It uses the worker budget of the scheduler and ignores
 * ovclash_set_nb_tasks(). False selects std::thread workers bounded by the task count, the
 * hardware concurrency and the available work (see ovclash_set_nb_tasks()).
 * ovclash_set_single_threaded() overrides either choice. Carbonite initializes lazily when
 * parallel work is needed. When initialization fails, the phase fails with context error text
 * and no other scheduler is selected. Default on a fresh context: enabled.
 *
 * On OpenUSD, MultithreadParams::mUseNewTaskManager chooses between the two broadphase and
 * narrowphase task managers of the engine. Both detection managers use Carbonite. Standalone
 * report extraction uses Carbonite when the flag is true and bounded std::thread workers when it
 * is false. Changing the flag after detection affects later extraction without recomputing the
 * results.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero selects the newer task manager.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Results: the flag selects an implementation, not a policy. It does not change WHICH
 *       pairs overlap or what is reported about them. ovstage keeps the same ordered reports
 *       under either manager. On OpenUSD the newer manager hands pairs to workers from a shared
 *       counter, so the ovclash_get_overlap_data() index order depends on the run, and repeated
 *       runs at identical settings can differ. Index order is never part of the contract (see
 *       ovclash_get_overlap_data); this setting is the one most likely to show that.
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_new_task_manager(ovclash_context_t* context, bool flag);
/**
 * @brief Drops overlaps that are present in every sampled frame of a time-sampled query.
 *
 * Hides pairs that interpenetrate permanently, such as modelling artifacts, rather than clashing
 * because of motion. Both backends honor it, and it affects only dynamic (time-sampled) queries;
 * see ovclash_set_times(). ovstage removes any accumulated overlap whose record count equals the
 * sampled frame count. Default on a fresh context: disabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero purges permanently-overlapping dynamic pairs.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_purge_permanent_dynamic_overlaps(ovclash_context_t* context, bool flag);
/**
 * @brief Drops overlaps between two transform-static meshes during a time-sampled query.
 *
 * Both backends honor it. A dynamic (time-sampled) run classifies each mesh as static or
 * animated, and suppresses pairs whose two sides are static. Classification follows active
 * authored transform animation on the prim or its ancestors. Several effective samples can mark a
 * prim animated even when every sampled value is identical. A single sample is not animation by
 * itself. When ovstage cannot resolve the initial value of an active operation, the mesh stays
 * conservatively non-static, unless another active operation proves it animated. The sampled
 * fallback validates numeric matrices and may cache them, but derives no animation flag from pose
 * equality or text-layer hints. Only conclusive raw static flags allow static-pair suppression or
 * freezing a future pose. Because of this conservative rule, ovstage does not claim full
 * authored-animation parity for every composed input. The flag has no effect on a static
 * (startTime == endTime) query. Default on a fresh context: enabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero purges static-vs-static pairs in dynamic runs.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_purge_permanent_static_overlaps(ovclash_context_t* context, bool flag);
/**
 * @brief Selects how (and whether) maximum local penetration depth is computed per overlap.
 *
 * Valid values are the four ovclash_max_local_depth_mode_t enumerators, in increasing cost:
 * - DISABLED computes no depth, so a mesh-vs-mesh overlap_data.maxLocalDepth stays -1.0f (see the
 *   point-vs-mesh note below);
 * - MINIMAL is the original fast estimate;
 * - MEDIUM and HIGH refine the estimate, and HIGH adds 64 sampling directions.
 *
 * Both backends implement all four modes. Both use contactEpsilon and contactCutoff to bound the
 * refinement and, while depth computation is on, to cull overlaps shallower than contactEpsilon.
 * Default on a fresh context: DISABLED. A mode greater than OVCLASH_MAX_LOCAL_DEPTH_MODE_HIGH is
 * rejected on both backends: nothing is stored, the previous mode stands, and the reason is
 * recorded for ovclash_get_last_error().
 *
 * The setting is mesh-only. A point-vs-mesh overlap reports maxLocalDepth 0.0f in every mode,
 * DISABLED included, and never the -1.0f "not computed" value. A 0.0f on a point-cloud overlap
 * is therefore not a measured zero-depth contact.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param mode [in] One of the ovclash_max_local_depth_mode_t enumerators.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or mode is greater than OVCLASH_MAX_LOCAL_DEPTH_MODE_HIGH (nothing is
 *         stored, the previous mode stands, and the reason is recorded for
 *         ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when mode is greater than
 *       OVCLASH_MAX_LOCAL_DEPTH_MODE_HIGH, the call is rejected and text is recorded for
 *       ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A rejected call stores nothing
 *       and leaves the previous mode in place. A null context is reported through the status
 *       alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_compute_max_local_depth(ovclash_context_t* context,
                                                                 ovclash_max_local_depth_mode_t mode);
/**
 * @brief Sets how many worker tasks the multi-threaded phases split their work into.
 *
 * Both backends clamp the value to 256 (MultiThreader::MaxNbTasks and kMaxNbTasks); 0 is
 * accepted. On ovstage the value controls only the std::thread manager
 * (ovclash_set_new_task_manager(false)). The parallel mesh preparation and collision phases then
 * use at most min(max(nbTasks, 1), hardware concurrency, available work) workers. An unknown
 * hardware concurrency counts as one. A running sampled-matrix prefetch callback reserves one
 * slot of the std::thread budget; keeping its completed window reserves none.
 *
 * In OpenUSD detection the value controls only the task split of the legacy Carbonite manager.
 * There 0 selects one task when work is available, while the stored and read-back setting stays
 * 0. An empty work range schedules no tasks. Standalone OpenUSD report extraction bounds its
 * std::thread workers with the same formula as ovstage, where 0 selects one worker. Both backends
 * ignore the value while single-threaded mode or the new task manager is on; the new manager uses
 * the worker budget of the Carbonite scheduler. Default on a fresh context: 128, stored but
 * unused by the default new-manager configuration.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param nbTasks [in] Desired task count; silently clamped to 256.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Results: the task count is a performance knob only. It changes neither which pairs
 *       overlap nor what is reported about them. ovstage collects parallel results without
 *       per-pair slots and restores canonical mesh-path order before reporting. The OpenUSD
 *       legacy manager concatenates contiguous task slices. Changing the value therefore keeps
 *       the serial order wherever the value applies. The OpenUSD new manager ignores it and keeps
 *       its run-dependent order.
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_nb_tasks(ovclash_context_t* context, uint32_t nbTasks);
/**
 * @brief Tolerance used when comparing two world transforms for equality.
 *
 * Both backends use it in the same places: detecting shared/instanced meshes with identical
 * poses, deciding that a mesh pair is a duplicate rather than a clash, and short-circuiting the
 * immediate-mode depth queries when both matrices are the same. Units are those of the matrix
 * components (stage units for translation), compared per element. Default on a fresh context:
 * 1e-6f. A non-finite value is rejected on both backends: nothing is stored, the previous value
 * stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param epsilon [in] Per-component comparison tolerance. Negative values make the comparison
 *                     never match.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or epsilon is not finite (nothing is stored, the previous value
 *         stands, and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when epsilon is not finite, the call
 *       is rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_pose_epsilon(ovclash_context_t* context, float epsilon);
/**
 * @brief Inflation applied when computing each mesh's world bounds.
 *
 * Both backends pass it to PxGeometryQuery::computeGeomBounds as both the additive offset
 * (stage/world units) and the scale factor 1 + epsilon. Nonnegative values widen the broadphase
 * and can only add candidate pairs; the narrow phase still decides. Finite negative values are
 * accepted and can shrink bounds, so that monotonicity guarantee does not apply. Default on a
 * fresh context: 0.01f. A non-finite value is rejected on both backends: nothing is stored, the
 * previous value stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param epsilon [in] Bounds inflation in stage units; 0 disables inflation.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or epsilon is not finite (nothing is stored, the previous value
 *         stands, and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when epsilon is not finite, the call
 *       is rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_bounds_epsilon(ovclash_context_t* context, float epsilon);
/**
 * @brief Minimum triangle area kept during mesh cooking; smaller triangles are discarded.
 *
 * Both backends pass it to PxCookingParams::meshAreaMinLimit, so the unit is squared stage units
 * and 0 disables the filter. Discarded triangles never appear in results, so a larger value both
 * speeds up cooking and hides slivers. The cooking phase reads the value, so it applies to meshes
 * cooked by the next pipeline run, not to meshes already cooked. Default on a fresh context:
 * 1e-6f. A non-finite value is rejected on both backends: nothing is stored, the previous value
 * stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param epsilon [in] Minimum triangle area in squared stage units; 0 keeps all triangles.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or epsilon is not finite (nothing is stored, the previous value
 *         stands, and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when epsilon is not finite, the call
 *       is rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_area_epsilon(ovclash_context_t* context, float epsilon);
/**
 * @brief Minimum penetration depth for an overlap to be reported, in stage units.
 *
 * Active only while max-local-depth computation is enabled (see
 * ovclash_set_compute_max_local_depth); with depth disabled the value is ignored. Both backends
 * behave identically. A negative value disables the feature: no culling and no depth cutoff.
 * Otherwise overlaps whose computed maxLocalDepth is below the value are dropped, and the value
 * serves as the minimum-depth cutoff of the refinement. Default on a fresh context: -1.0f
 * (disabled). A non-finite value is rejected on both backends: nothing is stored, the previous
 * value stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param value [in] Minimum depth in stage units; any negative value disables culling.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or value is not finite (nothing is stored, the previous value stands,
 *         and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when value is not finite, the call is
 *       rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_contact_epsilon(ovclash_context_t* context, float value);
/**
 * @brief Upper depth cutoff for the refined max-local-depth search, in stage units.
 *
 * Both backends use it identically, and only in the refined depth path. It is the maximum-depth
 * cutoff while contactEpsilon-based culling is active (contactEpsilon >= 0) and the
 * max-local-depth mode is above MINIMAL. A negative value leaves the cutoff unset. The setting is
 * ignored while contactEpsilon is negative or depth computation is disabled. Default on a fresh
 * context: -1.0f. A non-finite value is rejected on both backends: nothing is stored, the
 * previous value stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param value [in] Maximum depth in stage units; negative leaves the cutoff unset.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or value is not finite (nothing is stored, the previous value stands,
 *         and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when value is not finite, the call is
 *       rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_contact_cutoff(ovclash_context_t* context, float value);
/**
 * @brief Clearance distance, in stage units: prims closer than this count as clashing.
 *
 * Both backends inflate broadphase bounds by the tolerance and pass it to the PhysX mesh-mesh
 * queries, so a positive value reports mesh near-misses as overlaps. The point-vs-mesh narrow
 * phase ignores it on both backends, so near-miss points are not reported. Both backends clamp a
 * negative value to 0; there is no "shrink" mode. Default on a fresh context: 0.0f (touching
 * geometry only). A non-finite value is rejected on both backends: nothing is stored, the
 * previous value stands, and the reason is recorded for ovclash_get_last_error().
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param value [in] Clearance distance in stage units; negative is treated as 0.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or value is not finite (nothing is stored, the previous value stands,
 *         and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when value is not finite, the call is
 *       rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_tolerance(ovclash_context_t* context, float value);
/**
 * @brief Selects the sort key stored with each overlap, and whether duplicate triangles are counted.
 *
 * The backends differ sharply. On OpenUSD the value picks the per-overlap sort key: 0 = number
 * of overlapping elements, 1 = internal narrow-phase cost, 2 = max local depth. Any other value
 * leaves the key at 0. 0xffffffff also suppresses the duplicate-mesh triangle count, so nbTris
 * reads 0. On ovstage only the 0xffffffff case exists: it zeroes duplicate nbTris, and no other
 * value changes anything. Neither backend validates the value. Default on a fresh context: 0.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param mode [in] 0, 1, 2 or 0xffffffff. Other values leave OpenUSD's sort key at zero;
 *        on ovstage only 0xffffffff changes behavior.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_sort_mode(ovclash_context_t* context, uint32_t mode);
/**
 * @brief Chooses which triangle-triangle intersection kernel PhysX uses.
 *
 * Both backends map the value identically onto PxMeshMeshQueryFlag: 0 = the regular kernel,
 * 1 = eRESERVED1 (Moller variant), 2 = eRESERVED2 (SAT), 3 = eRESERVED3 (SIMD SAT). Any other
 * value silently behaves like 0. The kernels differ in speed and can differ marginally in how
 * they classify borderline hits. Default on a fresh context: 3. Neither backend validates the
 * value.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param code [in] 0-3; other values behave like 0.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_overlap_code(ovclash_context_t* context, uint32_t code);
/**
 * @brief Number of triangles per BVH34 leaf used when cooking meshes.
 *
 * Both backends pass it to PxCookingParams::midphaseDesc.mBVH34Desc.numPrimsPerLeaf. Larger
 * leaves cook faster and use less memory; smaller leaves query faster. The cooking phase reads
 * the value, so it applies to meshes cooked by the next pipeline run. Default on a fresh context:
 * 15, which is also the maximum.
 *
 * The accepted range is 1..15. PhysX validates numPrimsPerLeaf against [2,15], so 15 is a hard
 * ceiling, not a tuning suggestion. A larger value would fail every mesh cook. A failed cook does
 * not fail a pipeline step, so such a run would report success with zero overlaps on a model full
 * of them. Values outside 1..15 are rejected on both backends: nothing is stored, the previous
 * value stands, and the reason is recorded for ovclash_get_last_error(). A value of 1 is accepted
 * and reported back by ovclash_get_settings(). PhysX rejects 1 too, so both backends retry the
 * cook at 15 and build the mesh with 15 triangles per leaf. The reported overlaps do not change;
 * only cooking time and memory do.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param trisPerLeaf [in] Triangles per leaf; must be in 1..15.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or trisPerLeaf is outside 1..15 (nothing is stored, the previous value
 *         stands, and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when trisPerLeaf is outside 1..15, the
 *       call is rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE
 *       only, sticky). A rejected call stores nothing and leaves the previous value in place. A
 *       null context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_tris_per_leaf(ovclash_context_t* context, uint32_t trisPerLeaf);
/**
 * @brief Caps how many colliding triangle pairs are recorded per mesh pair.
 *
 * Both backends treat 0 as unlimited (internally 0xffffffff), and on both ovclash_set_any_hit()
 * overrides the value with 1. The limit truncates the recorded results, nbElements and the face
 * and outline report buffers. It does not stop a pair from being reported. Default on a fresh
 * context: 0 (unlimited). Neither backend validates the value.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param limit [in] Maximum triangle pairs per mesh pair; 0 means unlimited.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_triangle_limit(ovclash_context_t* context, uint32_t limit);
/**
 * @brief Caps the number of candidate pairs the broadphase is allowed to produce.
 *
 * A guard against combinatorial explosion on pathological scenes. Once the broadphase has
 * collected all of its candidate pairs, both backends compare their count against this limit.
 * When the count exceeds it, they cancel the run rather than silently truncate results. The pair
 * array is not bounded while it is collected. Default on a fresh context: 0xffffffff, effectively
 * unlimited. 0 is a real limit, not "unlimited". Neither backend validates the value.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param limit [in] Maximum broadphase pair count; 0xffffffff is the practical "no limit" value.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: the status covers only this call. Exceeding the limit requests cancellation
 *       instead of failing the phase that exceeded it. In a static stepped run both backends
 *       report success for the broadphase step (index 7). The next step is rejected for pending
 *       cancellation and, like every such rejection, records no last-error text. An ovstage
 *       time-sampled frame step that exceeds the limit fails itself. The cancellation stays
 *       pending, per context on ovstage and process-global on OpenUSD. Later
 *       ovclash_run_pipeline_step calls fail, even step 0 of a new pipeline, until
 *       ovclash_reset_context clears it (see ovclash_cancel_pipeline). An async run reports it as
 *       OVCLASH_STATUS_ERROR from ovclash_finish_pipeline, which clears it. While an async handle
 *       is active this call is rejected and text is recorded for ovclash_get_last_error()
 *       (OVCLASH_STANDALONE only, sticky).
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_broadphase_limit(ovclash_context_t* context, uint32_t limit);
/**
 * @brief Selects a static or time-sampled query and its sample spacing, in seconds.
 *
 * startTime == endTime selects a single-sample (static) query. Otherwise the run is dynamic: it
 * starts at exactly startTime and adds 1.0/timeCodesPerSecond in double precision until the next
 * time exceeds endTime. There is no endpoint clamp, so rounding may exclude an endpoint that is
 * exact in theory. Both backends disable the duplicate-mesh search for dynamic runs, and use one
 * startTime sample for a zero rate or a descending range. Both backends require all three values
 * to be finite and timeCodesPerSecond >= 0. Otherwise nothing is stored and the reason is
 * recorded for ovclash_get_last_error(). Defaults on a fresh context: 0, 0, 0. ovstage also reads
 * "all three zero" as "no times requested".
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param startTime [in] First sampled time, in seconds (the USD time code is the value multiplied
 *        by the stage's timeCodesPerSecond).
 * @param endTime [in] Sampling endpoint, in seconds; equal to startTime means a static query.
 * @param timeCodesPerSecond [in] Samples per second; 0 collapses the run to one sample.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or startTime, endTime or timeCodesPerSecond is not finite or
 *         timeCodesPerSecond is negative (nothing is stored, the previous values stand, and the
 *         reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when startTime, endTime or
 *       timeCodesPerSecond is not finite or timeCodesPerSecond is negative, the call is rejected
 *       and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A
 *       rejected call stores nothing and leaves the previous values in place. A null context is
 *       reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_times(ovclash_context_t* context,
                                               double startTime,
                                               double endTime,
                                               double timeCodesPerSecond);
/**
 * @brief Selects whole-stage traversal instead of a scoped prim set.
 *
 * Both backends clear any scope paths set earlier, whatever the flag value. OpenUSD does it
 * through ClashDetectionParams::releasePaths(), ovstage through clearScopePaths(). To keep a
 * non-empty explicit scope, call this before ovclash_set_scope() or ovclash_set_scope_paths(). A
 * set scope wins. With no scope and this flag cleared, both standalone backends select nothing at
 * all, because the Kit "current selection" path is compiled out. On both backends
 * ovclash_set_scope_paths() with a non-empty scope also clears this flag; the legacy
 * ovclash_set_scope() does not. Default on a fresh context: enabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero traverses the whole stage.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_full_scene(ovclash_context_t* context, bool flag);
/**
 * @brief Runs duplicate-mesh detection only, skipping clash detection entirely.
 *
 * Both backends honor it. The OpenUSD narrow phase stops after the duplicate test, and ovstage
 * skips the BVH, broad-phase and narrow-phase steps after its duplicate search.
 * ovclash_get_overlap_count() then reports 0, and only ovclash_get_duplicate_mesh_count() and
 * ovclash_get_duplicated_meshes_data() return anything. Duplicates are pairs of prims with
 * identical mesh data and equal poses (within poseEpsilon). Default on a fresh context:
 * disabled. Both backends turn duplicate detection off for time-sampled runs, so combining this
 * flag with a dynamic ovclash_set_times() yields no results.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero restricts the run to duplicate search.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_only_find_duplicates(ovclash_context_t* context, bool flag);
/**
 * @brief Suppresses overlaps that merely repeat an already reported identical-geometry pair.
 *
 * Both backends honor it after the broad phase. They drop pairs whose meshes and relative poses
 * make the contact a repeat of one already recorded, typically instanced or shared geometry laid
 * out identically. Default on a fresh context: disabled. During a time-sampled query that purges
 * permanent static overlaps, identical static copies still share one representative contact. The
 * representative is an actual eligible pair, so scope or subcomponent exclusions do not remove
 * the only remaining contact.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero drops redundant overlaps.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_ignore_redundant_overlaps(ovclash_context_t* context, bool flag);
/**
 * @brief Excludes invisible prims from mesh and point-cloud selection.
 *
 * Both backends honor it during scene traversal and population, so invisible prims never reach
 * cooking or the broadphase. Selection reads the value, so a change takes effect on the next
 * pipeline run. Default on a fresh context: enabled, so invisible geometry does not clash by
 * default.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero skips invisible prims.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_ignore_invisible_prims(ovclash_context_t* context, bool flag);
/**
 * @brief Enables UsdGeomPoints point-cloud vs mesh clash detection.
 *
 * Both backends implement point-vs-mesh and gate it on this flag; with the flag off, point-cloud
 * prims are not even selected. Results are overlaps of type
 * OVCLASH_OVERLAP_PAIR_TYPE_POINT_VS_MESH. Their element indices are point indices, available
 * through OVCLASH_OVERLAP_REPORT_FLAG_POINTS0. Selection reads the value, so it applies from the
 * next pipeline run. Default on a fresh context: disabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero enables point-cloud queries.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_point_clouds(ovclash_context_t* context, bool flag);
/**
 * @brief Selects the experimental point-cloud narrow-phase implementation.
 *
 * The backends differ. On OpenUSD the flag picks an alternative multi-threaded point-vs-mesh
 * pipeline, and is ignored unless multi-threading is active (see ovclash_set_single_threaded).
 * On ovstage the bit is stored but never read, so it does nothing. It also matters only while
 * ovclash_set_point_clouds() is enabled. Default on a fresh context: disabled.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param flag [in] Non-zero selects the experimental path.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: while an async pipeline handle is active the call is rejected and text is
 *       recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only, sticky). A null context
 *       is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_point_clouds_experimental(ovclash_context_t* context, bool flag);
/**
 * @brief Overrides the point diameter used for point-cloud clash tests, in stage units.
 *
 * The value is a width (twice the radius), not a radius, and excludes any scaling from the prim
 * transform. Both backends treat a value > 0 as an explicit override for every point, and report
 * it back as ovclash_overlap_report_t::pointSize0. A value <= 0 falls back, in order, to a single
 * authored USD width, to per-point authored widths, or to an automatically derived size. With
 * per-point widths pointSize0 is 0.0 and the caller reads the widths per index. ovstage collapses
 * a uniform per-point array back to its single value, where the OpenUSD parity backend reports
 * 0.0. The value matters only while ovclash_set_point_clouds() is enabled. Default on a fresh
 * context: 0.0f. A non-finite value is rejected on both backends: nothing is stored and the
 * reason is recorded for ovclash_get_last_error(). A call between the steps of a run changes the
 * size that ovstage tests and reports from the next sample of that run on. OpenUSD keeps the size
 * it computed before the first sample of the run.
 *
 * @param context [in] Context from ovclash_create_context(). A null context returns OVCLASH_STATUS_ERROR.
 * @param size [in] Point width in stage units; <= 0 restores automatic sizing.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null, an async pipeline
 *         handle is active, or size is not finite (nothing is stored, the previous value stands,
 *         and the reason is recorded for ovclash_get_last_error()).
 *
 * @note Errors: while an async pipeline handle is active, or when size is not finite, the call is
 *       rejected and text is recorded for ovclash_get_last_error() (OVCLASH_STANDALONE only,
 *       sticky). A rejected call stores nothing and leaves the previous value in place. A null
 *       context is reported through the status alone and records no text.
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_set_point_size(ovclash_context_t* context, float size);

/**
 * @brief Replaces the query scope using encoded absolute prim paths.
 *
 * This is the legacy low-level counterpart of ovclash_set_scope_paths(). Every path value must
 * come from ovclash_sdf_path_to_int(). With both arrays empty the engine falls back to
 * OVCLASH_SETTINGS_FLAG_FULL_SCENE, which is enabled on a fresh context; with the flag cleared,
 * nothing is selected. With only paths1 populated the query searches that set. With both
 * populated it searches paths1 against paths2. The arrays are resolved and copied during this
 * call.
 *
 * The backends handle stageId differently. OpenUSD resolves the stage from the standalone process
 * registry. It reports an invalid id through the engine error callback, but the legacy C wrapper
 * does not propagate that failure. The id is resolved during this call only. When nothing is
 * registered under it at that moment, pipeline steps, an async run and both immediate queries
 * fail with "no stage is open". They keep failing even after a stage is registered under that
 * id, until ovclash_open_stage, ovclash_set_scope_paths() or another ovclash_set_scope() call
 * supplies a stage. ovstage ignores stageId and uses the stage already opened on context.
 * Standalone callers should prefer ovclash_set_scope_paths(), which supplies the stage identity
 * of the context and validates UTF-8 paths directly.
 *
 * @param context [in] Context from ovclash_create_context(). Must not be null.
 * @param stageId [in] OpenUSD standalone stage-registry id; ignored by ovstage.
 * @param paths1 [in] First encoded-path array, or null when nbPaths1 is zero.
 * @param nbPaths1 [in] Number of entries in paths1.
 * @param paths2 [in] Second encoded-path array, or null when nbPaths2 is zero.
 * @param nbPaths2 [in] Number of entries in paths2.
 * @return OVCLASH_STATUS_SUCCESS when the request is accepted; OVCLASH_STATUS_ERROR when context
 *         is null/invalid, an async pipeline handle is active, an array is null with a non-zero
 *         count, or (on ovstage) an encoded id is unknown.
 *
 * @note Preconditions: paths2 without paths1 is not a documented scope shape.
 * @note Side effects: replaces the context's previous scope. On ovstage, validation completes
 *       before replacement, so a rejected request preserves the previous scope.
 * @note Lifetime: context copies the scope; the caller retains ownership of both arrays and may
 *       release them after this function returns.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_set_scope(ovclash_context_t* context,
                                               uint64_t stageId,
                                               const uint64_t* paths1,
                                               uint32_t nbPaths1,
                                               const uint64_t* paths2,
                                               uint32_t nbPaths2);

/**
 * @brief Builds the clash pipeline for the current settings and reports its step count.
 *
 * A valid context always yields at least 8 steps: 11 for a static query (start time == end
 * time), otherwise 7 plus one step per sampled time, and a sampled range always includes its start
 * time. A successful call therefore never reports "no work to do". Call ovclash_set_times or
 * ovclash_set_settings first, because the step count derives from startTime, endTime and
 * timeCodesPerSecond. Neither backend
 * checks that a stage is open or a scope is set; those failures surface later from
 * ovclash_run_pipeline_step. OpenUSD rebuilds and stores the concrete phase list and discards any
 * previous one. ovstage builds an immutable timing snapshot and sample-time vector, installs them
 * only after construction succeeds, and resets its completed-step counter to 0. Later changes of
 * the timing settings apply to the next pipeline, not to the installed snapshot. ovstage rejects
 * a dynamic schedule that does not progress, and a step count that does not fit in uint32_t
 * together with the seven fixed phases. An allocation failure may also reject a large schedule.
 * Such a failure keeps the previous pipeline. These safeguards are ovstage-only and leave the
 * OpenUSD engine unchanged.
 *
 * @param context [in] Context from ovclash_create_context. Mutated by this call.
 * @param out_step_count [out] Receives the number of pipeline steps (>= 8 on success). Left
 *        untouched on failure. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context or out_step_count is null,
 *         the context has no engine, an async pipeline handle is live, or a C++ exception was
 *         caught.
 *
 * @note Errors: the status says only that the call failed. ovclash_get_last_error, which exists
 *       only in OVCLASH_STANDALONE builds, returns the reason. A null context records no text at
 *       all.
 * @note Threading: serialize all calls on one context. Rejected while an async handle is live.
 */
OVCLASH_API ovclash_result_t ovclash_create_pipeline(ovclash_context_t* context, uint32_t* out_step_count);
/**
 * @brief Describes one planned pipeline step (name, index, count, progress, finished flag).
 *
 * Describes the plan built by ovclash_create_pipeline, not live progress. Both backends set
 * progress = index / nbSteps, stepProgress = 0.0f and finished = (index == nbSteps - 1). For live
 * progress during an async run, use ovclash_get_pipeline_step_data_async. On failure *data is
 * left completely untouched, so check the returned status.
 *
 * @param context [in] Context that owns a pipeline created by ovclash_create_pipeline.
 * @param data [out] Receives the step description. Must not be null.
 * @param index [in] Zero-based step index, in [0, step count from ovclash_create_pipeline() - 1].
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when data or context is null, the
 *         context has no engine, an async handle is live, no pipeline has been created, or index
 *         is out of range.
 *
 * @note Lifetime: data->name borrows a static, library-owned string literal that is valid for the
 *       process lifetime; it is never freed and is not invalidated by later calls.
 * @note Threading: serialize on one context. Rejected while an async handle is live.
 */
OVCLASH_API ovclash_result_t ovclash_get_pipeline_step_data(ovclash_context_t* context,
                                                            ovclash_step_data_t* data,
                                                            uint32_t index);
/**
 * @brief Runs one pipeline step synchronously on the calling thread.
 *
 * Steps carry state and must run in ascending order 0..count-1. A run out of order is not
 * diagnosed. Both backends execute each non-final step by index against whatever state exists.
 * The final ovstage step reports success without doing the work when the preceding steps have not
 * completed. OpenUSD reports OVCLASH_STATUS_SUCCESS as soon as it has invoked the phase function.
 * Per-phase failures are invisible here, so check ovclash_is_out_of_memory and the result counts.
 * ovstage reports OVCLASH_STATUS_ERROR when the phase itself fails, and records the reason as the
 * last error of the context.
 *
 * @param context [in] Context with a pipeline created by ovclash_create_pipeline.
 * @param index [in] Zero-based step index in [0, step count from ovclash_create_pipeline() - 1].
 * @return OVCLASH_STATUS_SUCCESS when the step ran (see caveat above), or OVCLASH_STATUS_ERROR
 *         when context is null or has no engine, an async handle is live, index is out of range,
 *         a nonzero step follows a stage replacement in a started pipeline, cancellation is
 *         pending, no stage is open (OpenUSD rejects every step of a created pipeline, ovstage the
 *         first step that reads the stage), or (OpenUSD) the process-global out-of-memory flag is
 *         set. No stage is open before ovclash_open_stage succeeds and after a failed
 *         ovclash_open_stage drops the previous one. On ovstage, a successful
 *         ovclash_ovstage_attach_stage also opens one, and ovclash_ovstage_detach_stage or
 *         ovclash_reset_context of an attached stage leaves none.
 *
 * @note Errors: ovclash_get_last_error (OVCLASH_STANDALONE only) carries ovstage phase failures.
 *       On ovstage, a rejected index (out of range, or before ovclash_create_pipeline) records
 *       its own text; OpenUSD records none for it. A rejection for pending cancellation records
 *       nothing on either backend. It is not a misuse, and the last-error string stays unchanged
 *       across it. On ovstage, a failure while a sampled frame runs discards its current and
 *       accumulated overlaps. Restart at step 0 before treating results as a complete query.
 *       After a stage replacement during a started pipeline, both backends reject nonzero steps
 *       with "stage changed; restart the pipeline at step 0". The rejection lasts until step 0
 *       restarts execution, an async run starts the pipeline again, or ovclash_create_pipeline
 *       installs a new pipeline.
 * @note Lifetime: release overlap reports before advancing to another step. Sampled ovstage steps
 *       can replace borrowed result buffers, even within the same pipeline run.
 * @note Lifetime: on ovstage, index 0 clears the last error of the context, so a message read
 *       after a run belongs to that run. That makes the report of a successful run, described on
 *       ovclash_get_last_error, safe to attribute; see also ovclash_stats_t::nbCookedMeshes.
 * @note Threading: serialize on one context; the step itself may fan out to internal worker tasks.
 */
OVCLASH_API ovclash_result_t ovclash_run_pipeline_step(ovclash_context_t* context, uint32_t index);
/**
 * @brief Starts the prepared pipeline on a background worker and returns an async handle.
 *
 * At most one async handle exists per context; a second call while one is live is rejected. While
 * a handle is live, almost every other context entry point (settings setters, scope, stepping,
 * result getters, reset) is rejected with "not allowed while an async pipeline handle is active".
 * OpenUSD immediately starts a run of the phase list on the PhysX task manager. ovstage starts the
 * whole pipeline on a std::thread, without an intentional startup delay. A call without
 * ovclash_create_pipeline is not an error on either backend: the worker performs no work and
 * reports success.
 *
 * On ovstage, the worker runs with a snapshot, taken at launch, of the floating-point controls of
 * the calling thread: rounding, denormal handling and exception masks. Later changes on the
 * calling thread do not affect that run. Launch and finish keep the controls of the caller. The
 * worker restores its own previous controls on every exit, cancellation and failure included.
 *
 * @param context [in] Context to run. Mutated by the worker thread until the pipeline finishes.
 * @param out_async [out] Receives the owned async handle. Left untouched on failure. Must not be
 *        null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context or out_async is null, the
 *         context has no engine, a handle is already live, (OpenUSD) a created pipeline would run
 *         with no stage open, the engine failed to start the run, or a C++ exception was caught.
 *         ovstage launches without a stage and ovclash_finish_pipeline then reports it missing.
 *
 * @note Lifetime: there is no ovclash_async_destroy. ovclash_finish_pipeline destroys the handle,
 *       and the pointer dangles after it returns. ovclash_release_context also reclaims a live
 *       handle: OpenUSD finishes it first, ovstage joins the worker. ovclash_reset_context is
 *       rejected while a handle is live.
 * @note Threading: until the run is finished, poll only through ovclash_is_pipeline_running,
 *       ovclash_get_pipeline_step_data_async, ovclash_async_is_cancelled and
 *       ovclash_cancel_pipeline. On ovstage the worker also writes the last-error string of the
 *       context. The access is synchronized, but which message ovclash_get_last_error returns
 *       during the run is nondeterministic. On OpenUSD, while the worker runs, these calls access
 *       the volatile process-global cancel flag and the unsynchronized progress state of the
 *       shared engine. That gives no C++ thread-safety guarantee; see the note of each call.
 */
OVCLASH_API ovclash_result_t ovclash_run_pipeline_async(ovclash_context_t* context, ovclash_async_t** out_async);
/**
 * @brief Polls whether the asynchronous pipeline is still executing.
 *
 * Both backends report not-running as soon as cancellation is requested, before the worker has
 * actually stopped. Do not use the answer to decide that the handle is safe to drop.
 * ovclash_finish_pipeline is the only call that joins the worker and releases the handle, and it
 * must be called on both backends either way.
 *
 * Without cancellation the contract is the same on both backends. It concerns convergence, not
 * any single reading: poll until the call reports false; once it does, it stays false. Finish the
 * handle before reading results, because the live-handle guard stays closed until
 * ovclash_finish_pipeline returns. Do not assume a value from the first poll. A small fixture can
 * complete before the first poll, so expecting either "running" or "not running" there is a
 * race. This holds even when ovclash_create_pipeline was never called, because the call still
 * starts a zero-work run. The backends reach the answer differently: ovstage reads its own worker
 * flag, and OpenUSD asks the engine whether results are available. That is mechanism, not
 * contract.
 *
 * Do not derive the answer from the `finished` flag of ovclash_get_pipeline_step_data_async. That
 * flag means "this is the last step index", so it turns true when the final step starts, not when
 * the run completes. On OpenUSD an empty handle reports finished while this call still reports
 * running. Treating finished as done would read results before the worker published them.
 *
 * @param async [in] Handle from ovclash_run_pipeline_async. Must not be null.
 * @param out_running [out] Receives true while the run is considered active; false when it has
 *        completed or as soon as cancellation was requested. Left untouched on failure.
 *        Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when async or out_running is null.
 *
 * @note Lifetime: async must not have been passed to ovclash_finish_pipeline yet; the handle is
 *       destroyed there and polling afterwards is a use-after-free.
 * @note Errors: a null handle is reported through the status alone and records no error text,
 *       because the owning context is unreachable through it.
 * @note Threading: may be polled from another thread while the worker runs, but not concurrently
 *       with or after ovclash_finish_pipeline on the same handle. ovstage reads atomic flags.
 *       OpenUSD reads the shared engine's volatile process-global cancel flag, which provides no
 *       C++ thread-safety guarantee, and then tests the engine's completion event.
 */
OVCLASH_API ovclash_result_t ovclash_is_pipeline_running(ovclash_async_t* async, bool* out_running);
/**
 * @brief Snapshots live step/progress information from a running asynchronous pipeline.
 *
 * Both backends refine progress with the counted work of the current step. OpenUSD uses the
 * work-item counter of the shared engine. ovstage snapshots per-context operation labels and
 * counted work under a mutex. stepProgress advances through reserved portions of the step, and
 * progress includes that fraction. Opaque operations report a label without estimating
 * completion. ovstage async names may append the current operation to the canonical phase name;
 * synchronous plan names stay unchanged. Neither fractional progress nor an operation label is a
 * time estimate. Before ovclash_create_pipeline, ovstage clears data through
 * ovclash_reset_step_data, which sets finished to **true** rather than zero-filling it. A poll
 * made before the pipeline exists therefore reports a finished step. As on
 * ovclash_reset_step_data, finished is no test of whether the record is initialized.
 *
 * @param async [in] Handle from ovclash_run_pipeline_async. A null handle returns OVCLASH_STATUS_ERROR.
 * @param data [out] Receives the current step snapshot. A null pointer returns OVCLASH_STATUS_ERROR.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when async or data is null.
 *
 * @note Errors: with a null handle the owning context is unreachable, so not even
 *       ovclash_get_last_error (OVCLASH_STANDALONE only) shows anything. *data is left untouched,
 *       and the status is the only signal.
 * @note Lifetime: data->name borrows a static library-owned literal valid for the process
 *       lifetime. The handle must not already have been passed to ovclash_finish_pipeline.
 * @note Threading: never call it concurrently with or after ovclash_finish_pipeline on the same
 *       handle. ovstage supports polling while its worker runs, and reports a locked snapshot
 *       clamped to the counted work. OpenUSD still reads the shared-engine progress without
 *       synchronization. Its mutex-protected copy of the step record does not make polling fully
 *       safe. A poll that races the work-counter reset of a phase can briefly report progress or
 *       stepProgress outside [0.0, 1.0], or non-finite. Consumers clamp the values and treat a
 *       non-finite value as unknown. Clash results are unaffected.
 */
OVCLASH_API ovclash_result_t ovclash_get_pipeline_step_data_async(ovclash_async_t* async, ovclash_step_data_t* data);
/**
 * @brief Waits for the asynchronous pipeline to complete and destroys the async handle.
 *
 * Call it exactly once per handle, also after ovclash_cancel_pipeline: cancel only requests a
 * stop and never joins or frees. On return the cancel flag is cleared, the process-global one on
 * OpenUSD and the per-context one on ovstage. The active-async slot of the context is released,
 * which enables the rest of the API again. OpenUSD maps the fetch status of the task system to
 * the result: OVCLASH_STATUS_SUCCESS means the worker was joined cleanly, and per-phase detection
 * failures are invisible here. ovstage maps the success flag of its worker to the result, and
 * reports OVCLASH_STATUS_ERROR when a pipeline phase failed. Both backends also report
 * OVCLASH_STATUS_ERROR when cancellation was requested before finish, even after the worker
 * completed, and even for a zero-work run without a created pipeline. The stop that the engine
 * requests itself when the broadphase limit is exceeded counts as cancellation, whether or not
 * the caller polled ovclash_is_pipeline_running first. A failed ovstage sampled frame discards
 * the current and accumulated overlaps, as described on ovclash_run_pipeline_step. Start a new
 * run before treating results as a complete query.
 *
 * @param async [in] Handle from ovclash_run_pipeline_async. May be null.
 * @return OVCLASH_STATUS_SUCCESS when the run completed, or OVCLASH_STATUS_ERROR when async is
 *         null, the worker reported failure, or cancellation was requested (including by the
 *         broadphase limit). An uncancelled zero-work run without a created pipeline succeeds.
 *         For a valid handle, even an error outcome joins the worker, frees the handle and leaves
 *         the context reusable, so the status never means "finishing failed".
 *
 * @note Lifetime: the handle is freed here. Any later use, including a second
 *       ovclash_finish_pipeline, ovclash_is_pipeline_running or ovclash_cancel_pipeline, is a
 *       use-after-free. Set the caller's pointer to null right away.
 * @note Threading: blocks the calling thread until the worker exits; call it from one thread only.
 */
OVCLASH_API ovclash_result_t ovclash_finish_pipeline(ovclash_async_t* async);
/**
 * @brief Requests cancellation of a running asynchronous pipeline.
 *
 * Returns immediately. It neither joins the worker nor frees the handle, so the caller must still
 * call ovclash_finish_pipeline afterwards. The scope of the request differs by backend. OpenUSD
 * sets a process-global volatile flag that aborts every in-flight pipeline in the process;
 * ovstage sets a per-context atomic. On both backends a set flag also makes later
 * ovclash_run_pipeline_step calls fail, on OpenUSD for every context in the process.
 * ovclash_finish_pipeline or ovclash_reset_context clears the flag. On OpenUSD,
 * ovclash_release_context of any context clears it too.
 *
 * @param async [in] Handle from ovclash_run_pipeline_async. A null handle returns OVCLASH_STATUS_ERROR.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when async is null.
 *
 * @note Errors: with a null handle no error text is recorded either, because the owning context
 *       is unreachable; the status is the only signal.
 * @note Lifetime: async must not already have been passed to ovclash_finish_pipeline.
 * @note Threading: may be called from another thread while the worker runs, but not concurrently
 *       with or after ovclash_finish_pipeline on the same handle. It cannot interrupt a finish
 *       already in progress, so cancel first, then finish. ovstage sets an atomic per-context
 *       flag. OpenUSD writes the volatile process-global flag of the shared engine, which worker
 *       threads read without synchronization; that gives no C++ thread-safety guarantee.
 */
OVCLASH_API ovclash_result_t ovclash_cancel_pipeline(ovclash_async_t* async);

/**
 * @brief Returns the number of duplicate mesh pairs found by the last pipeline run.
 *
 * Duplicates are pairs of meshes with identical mesh data at equal poses (within poseEpsilon);
 * see ovclash_set_only_find_duplicates. The count is 0 before any run. A query interrupted by
 * cancellation can return 0 with OVCLASH_STATUS_SUCCESS, and on OpenUSD so can a native
 * out-of-memory. Zero therefore does not by itself mean "no duplicates". Cancellation requested
 * after completion does not erase the results produced. For async runs, check the finish
 * outcome, and note the cancellation state before finish clears the flag and consumes the
 * handle. On ovstage, a stage replacement or a direct mesh-selection step can keep this aggregate
 * count while its table-indexed details turn stale; ovclash_get_duplicated_meshes_data then
 * fails.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param out_count [out] Receives the duplicate-pair count. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR if context or out_count is
 *         null, or an async pipeline handle is still active on this context.
 *
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_get_duplicate_mesh_count(ovclash_context_t* context, uint32_t* out_count);
/**
 * @brief Reads names, hashes, triangle count and time for one duplicate mesh pair.
 *
 * index must be < ovclash_get_duplicate_mesh_count. For a genuine duplicate pair
 * data->meshHash0 equals data->meshHash1; clashHash hashes the two prim paths. *data is written
 * only on success. The ovstage backend also fails when the extraction generation of the pair
 * differs from the current selected-object table, or when the stored mesh indices are otherwise
 * invalid. The generation changes, for example, after any stage replacement, successful or
 * failed, and after a direct mesh-selection step. The OpenUSD backend fails when one of the two
 * prims has been deleted.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param data [out] Receives the duplicate-pair record. Must not be null.
 * @param index [in] Zero-based duplicate-pair index.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or data,
 *         an out-of-range index, stale/invalid results, a deleted prim, or an active
 *         async pipeline.
 *
 * @note Lifetime: data->name0 and data->name1 borrow backend-owned path storage. The next
 *       pipeline run, ovclash_release_memory, ovclash_reset_context and ovclash_release_context
 *       invalidate them. On ovstage, every call that retires the selected-object table
 *       invalidates them too: an ovclash_open_stage call that gets past argument validation,
 *       whether or not the open succeeds; a successful ovclash_ovstage_attach_stage;
 *       ovclash_ovstage_detach_stage; ovclash_ovstage_set_matrix_decomposition with a changed
 *       mode (see ovclash_ovstage.h); and an ovclash_get_prim_matrix failure that retires the
 *       selection population. The aggregate counts that survive do not keep them alive. Copy the
 *       bytes to keep a name. Never free them.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_get_duplicated_meshes_data(ovclash_context_t* context,
                                                                ovclash_duplicate_mesh_data_t* data,
                                                                uint32_t index);
/**
 * @brief No-op in ovclash: does not delete duplicate meshes on either backend.
 *
 * The stage-editing implementation lives in the Kit-only KitClash override, which ovclash does
 * not compile. The OpenUSD backend therefore reaches an empty base-class virtual, and the ovstage
 * backend has an explicitly empty body. The call still validates the context and is rejected
 * while an async pipeline is active, but it never changes results, the stage or any counter.
 * Duplicate records stay visible through ovclash_get_duplicate_mesh_count afterwards.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or the call is
 *         rejected.
 *
 * @note Errors: a rejected call also records text retrievable through ovclash_get_last_error,
 *       which itself exists only under OVCLASH_STANDALONE.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_delete_duplicate_meshes(ovclash_context_t* context);

/**
 * @brief Returns the number of detected overlap groups (one group per colliding prim pair).
 *
 * Each group holds ovclash_overlap_data_t::nbRecords per-frame records, always 1 for a static
 * query. The count is 0 before any pipeline has run. A query interrupted by cancellation can
 * return 0 with OVCLASH_STATUS_SUCCESS, and OpenUSD also reports 0 after a native out-of-memory.
 * A zero count alone therefore does not prove that the scene is clash-free. Cancellation
 * requested after completion does not erase the results produced. For async runs, check the
 * finish outcome, and note ovclash_async_is_cancelled before finish clears the flag and consumes
 * the handle. Also check ovclash_is_out_of_memory. On ovstage, a stage replacement or a direct
 * mesh-selection step can keep this aggregate count while the table-indexed record details turn
 * stale; ovclash_get_overlap_data then fails.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param out_count [out] Receives the overlap-group count. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR if context or out_count is
 *         null, or an async pipeline handle is still active on this context.
 *
 * @note Threading: serialize all calls on one context. Rejected while a pipeline started by
 *       ovclash_run_pipeline_async has not been finished. Cancellation still requires
 *       ovclash_finish_pipeline to release the active handle.
 */
OVCLASH_API ovclash_result_t ovclash_get_overlap_count(ovclash_context_t* context, uint32_t* out_count);
/**
 * @brief Reads names, hashes, element count, time and depth for one overlap record.
 *
 * index selects the overlap group (< ovclash_get_overlap_count), and recordIndex the frame inside
 * that group (< the nbRecords of the group). Both backends range-check both indices. On
 * failure *data is left completely untouched, and an unwritten data->type is not
 * OVCLASH_OVERLAP_PAIR_TYPE_UNDEFINED, so always check the result code. minDistance and
 * maxLocalDepth are in stage linear units. On a mesh-vs-mesh overlap maxLocalDepth stays -1.0f
 * unless max-local-depth computation ran. A point-vs-mesh overlap always reports 0.0f, because
 * depth is mesh-only (see ovclash_set_compute_max_local_depth). The ovstage backend also fails
 * when the extraction generation of the record differs from the current selected-object table.
 * That happens after a stage replacement, successful or failed, and after a direct step-1
 * re-selection. The check keeps an old clash from being relabelled with new paths.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param data [out] Receives the overlap record. Must not be null.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or data,
 *         an out-of-range index or recordIndex, stale results, an active async
 *         pipeline, or a backend fetch failure (for example a deleted prim).
 *
 * @note Lifetime: data->name0 and data->name1 borrow backend-owned path storage and must not be
 *       freed. The next pipeline run, ovclash_release_memory, ovclash_reset_context and
 *       ovclash_release_context invalidate them. On ovstage, every call that retires the
 *       selected-object table invalidates them too: an ovclash_open_stage call that gets past
 *       argument validation, whether or not the open succeeds; a successful
 *       ovclash_ovstage_attach_stage; ovclash_ovstage_detach_stage;
 *       ovclash_ovstage_set_matrix_decomposition with a changed mode (see ovclash_ovstage.h);
 *       and an ovclash_get_prim_matrix failure that retires the selection population. The
 *       aggregate counts that survive do not keep them alive. Copy the bytes to keep a name.
 * @note Ordering: index is an identifier, not a rank. Which group sits at which index is NOT part
 *       of the contract. Two runs over the same stage with the same settings report the same set
 *       of groups with the same data, but not necessarily in the same order. On the OpenUSD
 *       backend, ovclash_set_new_task_manager() makes the order genuinely run-dependent. Match
 *       groups by name0 and name1 (or clashHash) rather than by index, and sort in the consumer
 *       when a deterministic order matters.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_get_overlap_data(ovclash_context_t* context,
                                                      ovclash_overlap_data_t* data,
                                                      uint32_t index,
                                                      uint32_t recordIndex);
/**
 * @brief Recomputes the maximum local penetration depth for an already detected record.
 *
 * Mesh-vs-mesh records only; point-vs-mesh records fail on both backends. The value is in stage
 * linear units. It is 0.0f when the record has no hard triangle pairs, which touch or
 * interpenetrate at zero distance, so a record made only of tolerance near-misses reports 0.0f.
 * The mode set through ovclash_set_compute_max_local_depth affects only how far the SAT estimate
 * is refined. Unlike ovclash_imm_compute_max_local_depth, this call is NOT rejected in mode
 * OVCLASH_MAX_LOCAL_DEPTH_MODE_DISABLED; it returns the cheap estimate. *depth is left untouched
 * on failure.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param depth [out] Receives the maximum local depth. Must not be null.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or depth,
 *         an out-of-range index/recordIndex, a point-cloud overlap, stale results, an
 *         active async pipeline, or a backend computation failure.
 *         On stale failure *depth remains untouched.
 *
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 *       The computation itself is synchronous and can be expensive on dense pairs.
 */
OVCLASH_API ovclash_result_t ovclash_compute_max_local_depth(ovclash_context_t* context,
                                                             float* depth,
                                                             uint32_t index,
                                                             uint32_t recordIndex);
/**
 * @brief Estimates the world-space relative motion between the two prims of a dynamic record.
 *
 * Meaningful only for time-sampled queries. Every record of a static query fails, and the first
 * sampled frame of a run yields a zero or degenerate motion vector rather than failing.
 * Mesh-vs-mesh records only; point-vs-mesh records fail on both backends. *motionVector is left
 * untouched on failure. ovclash_depenetrate_meshes normalizes the result and uses it as the
 * direction of OVCLASH_DEPENETRATION_AXIS_MOTION_VECTOR.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param motionVector [out] Receives the estimated motion vector. Must not be null.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or
 *         motionVector, an out-of-range index/recordIndex, a record without previous-frame
 *         transforms, a point-cloud overlap, stale results, or an active async pipeline.
 *         Both backends check staleness before record-type/previous-frame state and leave the
 *         output untouched on failure.
 *
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_estimate_motion_vector(ovclash_context_t* context,
                                                            ovclash_float3_t* motionVector,
                                                            uint32_t index,
                                                            uint32_t recordIndex);
/**
 * @brief Computes the displacement that separates the two meshes of a record along an axis.
 *
 * Mesh-vs-mesh records only; point-vs-mesh records fail on both backends. userDir is required,
 * and must be non-null, exactly for OVCLASH_DEPENETRATION_AXIS_USER_DEFINED; other axes ignore
 * it. OVCLASH_DEPENETRATION_AXIS_MOTION_VECTOR fails unless the record carries previous-frame
 * transforms. Both backends silently treat an axis value outside the enum as the zero direction,
 * and still return success with a meaningless displacement. disp is a world-space vector in stage
 * linear units and is untouched on failure.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param disp [out] Receives the separating displacement. Must not be null.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @param axis [in] Depenetration direction selector.
 * @param userDir [in] Direction for OVCLASH_DEPENETRATION_AXIS_USER_DEFINED; may be null otherwise.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or disp, an
 *         out-of-range index/recordIndex, a point-cloud overlap, USER_DEFINED with a null
 *         userDir, MOTION_VECTOR without previous transforms, stale results, or an
 *         active async pipeline.
 *         On stale failure *disp remains untouched.
 *
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_depenetrate_meshes(ovclash_context_t* context,
                                                        ovclash_float3_t* disp,
                                                        uint32_t index,
                                                        uint32_t recordIndex,
                                                        ovclash_depenetration_axis_t axis,
                                                        const ovclash_float3_t* userDir);

/**
 * @brief Legacy single-mesh overlap report: colliding faces of one mesh plus the outline.
 *
 * Honors only OVCLASH_OVERLAP_REPORT_FLAG_DEFAULT (0), _FACES and _OUTLINE. It ignores _FACES0,
 * _FACES1 and _POINTS0; use ovclash_create_overlap_report_ex for those. meshIndex chooses whose
 * face indices land in report->collidingFaces0, and collidingFaces1 always stays empty here.
 * meshIndex must be OVCLASH_MESH_INDEX_0 or _1 whenever faces are requested. Point-cloud overlaps
 * are unsupported and fail on both backends. *report is written only on success.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param report [out] Receives the report view. Must not be null; pass to
 *                     ovclash_release_overlap_report when done.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @param meshIndex [in] Which mesh of the pair the face indices refer to.
 * @param flags [in] Bitmask of ovclash_overlap_report_flag_t.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or report,
 *         an out-of-range index/recordIndex, a point-cloud overlap, an invalid meshIndex, stale
 *         ovstage results, an active async pipeline, or a backend extraction failure.
 *
 * @note Floating point: both backends compute outline segments under the calling thread's
 *       floating-point controls, so reports requested under different rounding or denormal
 *       modes can differ. The ovstage-only OVCLASH matrix decomposition instead replays the
 *       controls in effect when the record was detected.
 * @note Lifetime: every pointer in *report is borrowed, never owned by the caller. On the OpenUSD
 *       backend they point at engine heap buffers that ovclash_release_overlap_report frees;
 *       skipping that call leaks them. On ovstage they alias context-owned result vectors, and
 *       release only clears the struct. Release every report before the next pipeline step or
 *       run, ovclash_release_memory, ovclash_reset_context or ovclash_release_context. On
 *       ovstage, advancing a sampled pipeline can replace these borrowed buffers. ovstage rejects
 *       report acquisition when the extraction generations of the record and the selected table
 *       differ. A borrowed view acquired earlier cannot be checked afterwards.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_create_overlap_report(ovclash_context_t* context,
                                                           ovclash_overlap_report_t* report,
                                                           uint32_t index,
                                                           uint32_t recordIndex,
                                                           ovclash_mesh_index_t meshIndex,
                                                           uint32_t flags);
/**
 * @brief Extended overlap report: per-mesh faces, outline, cloud points and both world matrices.
 *
 * Honors OVCLASH_OVERLAP_REPORT_FLAG_FACES0, _FACES1 and _OUTLINE for mesh-vs-mesh records, and
 * _POINTS0, which also fills pointSize0, for point-vs-mesh records. Both backends ignore the
 * legacy _DEFAULT and _FACES bits here. matrixData0 and matrixData1 are OUTPUT buffers, not
 * inputs. When non-null, each receives exactly 16 doubles (128 bytes, row-major 4x4): the world
 * transform of its prim captured at that record. Each must therefore point at storage for 16
 * doubles. The identically named const double* parameters of ovclash_imm_compute_max_local_depth
 * and ovclash_imm_depenetrate_meshes go the opposite direction. Nothing is written to *report or
 * to the matrices on failure.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param report [out] Receives the report view. Must not be null; pass to
 *                     ovclash_release_overlap_report when done.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @param flags [in] Bitmask of ovclash_overlap_report_flag_t.
 * @param matrixData0 [out] Optional 16-double row-major world matrix of mesh/cloud 0. May be null.
 * @param matrixData1 [out] Optional 16-double row-major world matrix of mesh 1. May be null.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR on a null context or report,
 *         an out-of-range index/recordIndex, stale results, an active async pipeline,
 *         or a backend extraction failure.
 *
 * @note Floating point: both backends compute outline segments under the calling thread's
 *       floating-point controls, so reports requested under different rounding or denormal
 *       modes can differ. The ovstage-only OVCLASH matrix decomposition instead replays the
 *       controls in effect when the record was detected.
 * @note Lifetime: all report pointers are borrowed. On OpenUSD they are engine heap buffers that
 *       ovclash_release_overlap_report frees; skipping that call leaks them. On ovstage they alias
 *       context-owned vectors. Release every report before the next pipeline step or run,
 *       ovclash_release_memory, ovclash_reset_context or ovclash_release_context. On ovstage,
 *       advancing a sampled pipeline can replace these borrowed buffers. ovstage rejects report
 *       acquisition when the extraction generations of the record and the selected table differ.
 *       A borrowed view acquired earlier cannot be checked afterwards.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_create_overlap_report_ex(ovclash_context_t* context,
                                                              ovclash_overlap_report_t* report,
                                                              uint32_t index,
                                                              uint32_t recordIndex,
                                                              uint32_t flags,
                                                              double* matrixData0,
                                                              double* matrixData1);
/**
 * @brief Releases a report produced by ovclash_create_overlap_report[_ex] and clears the struct.
 *
 * On the OpenUSD backend this frees the four engine-allocated buffers that *report references.
 * Call it for every successful create, or the memory leaks. On the ovstage backend the context
 * owns the buffers, and the call only clears the view. On both backends *report is then reset to
 * the ovclash_reset_overlap_report state, which makes a second release harmless. A
 * zero-initialized or reset empty report is also safe to release with a live context. Otherwise
 * pass only a struct that a create call filled in and that has not been edited since: the OpenUSD
 * path rebuilds the engine record straight from these fields and would free arbitrary pointers.
 * Unlike the other report functions, this one is NOT rejected while an async pipeline is active.
 *
 * @param context [in] Context that created the report. Must not be null.
 * @param report [in,out] Report to release; cleared in place. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS on success, OVCLASH_STATUS_ERROR if context or report is null
 *         (OpenUSD also if the context holds no engine instance).
 *
 * @note Threading: serialize all calls on one context.
 */
OVCLASH_API ovclash_result_t ovclash_release_overlap_report(ovclash_context_t* context,
                                                            ovclash_overlap_report_t* report);

/**
 * @brief Computes maximum local penetration depth for two meshes without running a pipeline.
 *
 * meshPath0 and meshPath1 are encoded absolute prim paths from ovclash_sdf_path_to_int(). Each
 * matrix is exactly 16 doubles holding the row-major 4x4 world transform of its mesh. The call
 * loads and cooks the two meshes synchronously with the current context settings. In mode
 * OVCLASH_MAX_LOCAL_DEPTH_MODE_DISABLED the operation fails. No prior clash query is required.
 * On success *depth receives the computed value; on failure it is left unchanged.
 *
 * @param context [in] Context with an open stage. Must not be null.
 * @param depth [out] Receives maximum local depth. Must not be null.
 * @param meshPath0 [in] Encoded absolute prim path of the first mesh.
 * @param matrixData0 [in] Pointer to 16 row-major doubles for the first world transform.
 * @param meshPath1 [in] Encoded absolute prim path of the second mesh.
 * @param matrixData1 [in] Pointer to 16 row-major doubles for the second world transform.
 * @return OVCLASH_STATUS_SUCCESS when depth is computed; OVCLASH_STATUS_ERROR for a null/invalid
 *         context or output, an active async pipeline, no open stage, disabled depth computation,
 *         null matrices, unknown paths, missing/invalid mesh data, a duplicate identical mesh
 *         pose, a backend query/cooking failure, or, on ovstage, a matrix with a non-finite or
 *         out-of-float-range value.
 *
 * @note Preconditions: both matrix pointers must address 16 readable doubles; a null matrix
 *       pointer is rejected with OVCLASH_STATUS_ERROR on both backends.
 * @note Side effects: may allocate and cook temporary mesh/query data; it does not publish or
 *       replace pipeline overlap results.
 * @note Lifetime: all input pointers are borrowed only for the duration of the call.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_imm_compute_max_local_depth(ovclash_context_t* context,
                                                                 float* depth,
                                                                 uint64_t meshPath0,
                                                                 const double* matrixData0,
                                                                 uint64_t meshPath1,
                                                                 const double* matrixData1);
/**
 * @brief Computes a directed depenetration vector for two meshes without running a pipeline.
 *
 * meshPath0 and meshPath1 are encoded absolute prim paths from ovclash_sdf_path_to_int(). Each
 * matrix is exactly 16 doubles holding the row-major 4x4 world transform of its mesh. The call
 * runs synchronously with the current stage and cooking settings. No prior clash query is
 * required. On success *disp receives the direction-and-magnitude vector that the mesh query
 * returns. On failure *disp is left unchanged.
 *
 * @param context [in] Context with an open stage. Must not be null.
 * @param disp [out] Receives the depenetration vector. Must not be null.
 * @param meshPath0 [in] Encoded absolute prim path of the first mesh.
 * @param matrixData0 [in] Pointer to 16 row-major doubles for the first world transform.
 * @param meshPath1 [in] Encoded absolute prim path of the second mesh.
 * @param matrixData1 [in] Pointer to 16 row-major doubles for the second world transform.
 * @param dir [in] Direction in which to solve the depenetration. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS when disp is computed; OVCLASH_STATUS_ERROR for a null/invalid
 *         context, output, or direction, an active async pipeline, no open stage, null matrices,
 *         unknown paths, missing/invalid mesh data, a backend query/cooking failure, or, on
 *         ovstage, a matrix with a non-finite or out-of-float-range value.
 *
 * @note Preconditions: both matrix pointers must address 16 readable doubles; a null matrix
 *       pointer is rejected with OVCLASH_STATUS_ERROR on both backends.
 * @note Side effects: may allocate and cook temporary mesh/query data; it does not publish or
 *       replace pipeline overlap results.
 * @note Lifetime: all input pointers are borrowed only for the duration of the call.
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_imm_depenetrate_meshes(ovclash_context_t* context,
                                                            ovclash_float3_t* disp,
                                                            uint64_t meshPath0,
                                                            const double* matrixData0,
                                                            uint64_t meshPath1,
                                                            const double* matrixData1,
                                                            const ovclash_float3_t* dir);

/**
 * @brief Not supported in ovclash: always fails on both backends.
 *
 * Debug-mesh creation is a Kit-only feature of the KitClash override, which ovclash does not
 * compile. Both backends stop after validating the record, so neither reaches the engine. Both
 * record a "not supported" diagnostic for ovclash_get_last_error: "not supported by the ovclash
 * OpenUSD backend" on OpenUSD, "not supported by the ovstage backend" on ovstage. index and
 * recordIndex are still range-checked first, and stale result details are rejected before the
 * call reports that it is unsupported. printNames and mode are ignored. The function remains only
 * for ABI compatibility with the Kit clash interface.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 * @param index [in] Zero-based overlap-group index.
 * @param recordIndex [in] Zero-based record/frame index inside the group.
 * @param printNames [in] Ignored.
 * @param mode [in] Ignored.
 * @return Always OVCLASH_STATUS_ERROR.
 *
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_create_overlap_meshes(
    ovclash_context_t* context, uint32_t index, uint32_t recordIndex, bool printNames, uint32_t mode);
/**
 * @brief No-op in ovclash: there are never any overlap debug meshes to release.
 *
 * Counterpart of ovclash_create_overlap_meshes, which always fails here. The OpenUSD backend
 * calls an empty base-class virtual, and the ovstage backend has an empty body. Only the context
 * validation and the async-pipeline rejection run.
 *
 * @param context [in] Context created by ovclash_create_context. Must not be null.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when context is null or the call is
 *         rejected.
 *
 * @note Errors: a rejected call also records text retrievable through ovclash_get_last_error
 *       (available only under OVCLASH_STANDALONE).
 * @note Threading: serialize all calls on one context; rejected while an async pipeline is active.
 */
OVCLASH_API ovclash_result_t ovclash_release_overlap_meshes(ovclash_context_t* context);

/**
 * @brief Reads the host timeline state.
 *
 * Both standalone backends report a fixed stub timeline: startTime 0.0, endTime 0.0,
 * currentTime 0.0, timeCodesPerSecond 24.0. OpenUSD takes those values from the FixedTimeline
 * stub that replaces the Kit timeline; ovstage hard-codes the same four values. Nothing here
 * reflects the open stage or ovclash_set_times, and no ovclash entry point can change it. Treat
 * it as a constant, and drive time-sampled queries through ovclash_set_times instead. Neither
 * backend validates the context argument, which may be null, and the active-async guard does not
 * apply. Only the ovstage owner-operation rejection uses the context: it records its text on a
 * non-null context.
 *
 * @param context [in] Not validated. May be null. On ovstage a non-null context receives the
 *                owner-operation rejection text; nothing else is read from or written to it.
 * @param out_data [out] Receives the four timeline doubles. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when out_data is null or, on ovstage, when
 *         the call runs inside an owner-operation callback on the same thread (see
 *         ovclash_ovstage_run_owner_operation). Nothing is written to out_data on error.
 *
 * @note Lifetime: out_data is caller-owned plain data; nothing is borrowed.
 * @note Threading: reads no context state; safe from any thread, including during an async run.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_get_timeline_data(ovclash_context_t* context, ovclash_timeline_data_t* out_data);
/**
 * @brief Reads the timeline metadata of the open stage.
 *
 * A file-backed operation like ovclash_open_stage, but declared outside the OVCLASH_STANDALONE
 * fence. ovclash_get_timeline_data mirrors a host timeline that standalone builds do not have;
 * this function reads the stage itself. It returns the root-layer `startTimeCode`,
 * `endTimeCode`, `timeCodesPerSecond` and `framesPerSecond` of the stage passed to
 * ovclash_open_stage. A caller can then convert a static time or a dynamic range in seconds into
 * stage time codes (seconds = time code / timeCodesPerSecond), or take the range from the stage.
 * Unauthored start and end time codes read 0.0, and an unauthored `framesPerSecond` reads 24.0.
 * Both backends resolve `timeCodesPerSecond` from the authored `timeCodesPerSecond`, else the
 * authored `framesPerSecond`, else 24.0. This is the effective rate of native stage-time
 * conversion, so an authored `timeCodesPerSecond` of 24.0 still overrides a different
 * `framesPerSecond`. ovstage requires a positive finite effective rate when it opens the stage.
 * OpenUSD may report a nonpositive authored rate, so callers must reject a nonpositive or
 * nonfinite rate before converting time. On ovstage, a stage attached with
 * ovclash_ovstage_attach_stage reports the start_time_code, end_time_code, time_codes_per_second
 * and frames_per_second of its binding instead. All fields are zeroed before any validation, so a
 * call that fails after the null checks leaves a well-defined all-zero record. Nothing is written
 * when out_data is null.
 *
 * @param context [in] Context from ovclash_create_context. Must not be null.
 * @param out_data [out] Receives the four stage timeline doubles. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS when the record is written; OVCLASH_STATUS_ERROR when context or
 *         out_data is null/invalid, an async pipeline handle is active, or no stage is open.
 *
 * @note Lifetime: a null out_data records an error; otherwise a valid context's last error is
 *       cleared before the remaining checks. out_data is caller-owned plain data.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_get_stage_timeline_data(ovclash_context_t* context,
                                                             ovclash_stage_timeline_data_t* out_data);
/**
 * @brief Reads mesh and point-cloud counters for the current query.
 *
 * Reports how many meshes were selected, how many of them were shared instances, how many were
 * cooked, and how many point clouds were selected. The values are meaningful only after the
 * pipeline has run; before that they are zero. OpenUSD copies the accumulated statistics of the
 * engine, which ovclash_reset_context clears. ovstage computes them live from context state,
 * which ovclash_release_memory clears.
 *
 * @param context [in] Context from ovclash_create_context. Must not be null.
 * @param stats [out] Receives the four counters. Must not be null; left untouched on failure.
 * @return OVCLASH_STATUS_SUCCESS on success; OVCLASH_STATUS_ERROR when stats is null, context is
 *         null/invalid, or an async pipeline handle is active. Detail via ovclash_get_last_error.
 *
 * @note Lifetime: stats is caller-owned plain data; nothing is borrowed.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_get_query_stats(ovclash_context_t* context, ovclash_stats_t* stats);
/**
 * @brief Releases memory held for the current query.
 *
 * The two backends drop different amounts of state, but both leave the context usable. ovstage
 * clears the query results: source data, meshes, point clouds, overlaps, duplicates, cooked and
 * shared counters, dynamic state and the broadphase BVH. An attached stage stays attached.
 * OpenUSD releases only the overlap debug meshes of the engine, which ovclash never creates (see
 * ovclash_create_overlap_meshes), so it frees nothing and keeps every result. On both backends
 * the context can still be reconfigured and run again. ovclash_reset_context drops all query
 * state.
 *
 * @param ptr [in] Context from ovclash_create_context. A null ptr returns OVCLASH_STATUS_ERROR.
 *
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR when ptr is null or an async
 *         pipeline handle is active.
 *
 * @note Errors: the status reports the rejection. Both backends reject an active async pipeline
 *       handle and report it through ovclash_get_last_error, which exists only in
 *       OVCLASH_STANDALONE builds. A null ptr records nothing anywhere.
 * @note Lifetime: on ovstage this invalidates every borrowed view previously handed out by result
 *       getters and reports for this context.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_release_memory(ovclash_context_t* ptr);
/**
 * @brief Queries whether the backend recorded an out-of-memory condition.
 *
 * OpenUSD reads the process-global out-of-memory flag of the engine. The clash allocator sets it
 * when an allocation returns null. ovclash_reset_context, the release of any OpenUSD context and
 * the first phase of an async run clear it. Because the flag is global, every context in the
 * process shares it, and the context argument serves only the active-async guard. ovstage
 * records, per context, a std::bad_alloc from a required allocation that reaches its C API or
 * async-worker boundary. That flag survives the pipeline finish. A reset, an accepted stage-open
 * attempt or a new run clears it. An optional allocation that fails and recovers does not set it.
 * A dependency may report an allocation failure through an error status or text without setting
 * this flag, so also check the call status and ovclash_get_last_error. A null ovstage context
 * reports 0.
 *
 * @param context [in] Context from ovclash_create_context. May be null. On ovstage selects the
 *                recorded condition; both backends reject an active async pipeline handle.
 * @param out_is_oom [out] Receives 1 or 0. Must not be null; left untouched on failure.
 * @return OVCLASH_STATUS_SUCCESS when out_is_oom is written; OVCLASH_STATUS_ERROR when out_is_oom
 *         is null or an async pipeline handle is active on a non-null context.
 *
 * @note Lifetime: out_is_oom is caller-owned plain data; nothing is borrowed.
 * @note Threading: the OpenUSD flag is a plain `volatile bool` that worker threads write without
 *       synchronization; read it only after the pipeline has finished. The ovstage flag is atomic,
 *       but a query is still rejected until the async handle has been finished and destroyed.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_is_out_of_memory(ovclash_context_t* context, uint32_t* out_is_oom);
/**
 * @brief Reports whether cancellation has been requested for an asynchronous pipeline.
 *
 * The OpenUSD backend never dereferences the async argument, because the underlying bridge
 * exposes cancellation only as a process-global flag. It answers for the whole process and
 * accepts a null handle. The ovstage backend reads the per-context flag reached through the
 * handle. For a null handle or a handle without a context it reports 0 and still returns success.
 * The flag stays set until ovclash_finish_pipeline or ovclash_reset_context clears it. On OpenUSD,
 * finishing, resetting or releasing any context clears the process-global flag for every context.
 * This call may be polled while the handle is live.
 *
 * @param async [in] Handle from ovclash_run_pipeline_async. May be null on both backends.
 * @param out_cancelled [out] Receives 1 when cancellation was requested, otherwise 0.
 * @return OVCLASH_STATUS_SUCCESS when out_cancelled was written; OVCLASH_STATUS_ERROR only when
 *         out_cancelled is null, or a C++ exception was caught.
 *
 * @note Lifetime: pass null or a live handle. ovstage dereferences the handle, so it must not
 *       already have been passed to ovclash_finish_pipeline. OpenUSD never dereferences it, but
 *       finishing has already cleared the flag, so a finished handle reports nothing about its run.
 * @note Threading: may be polled from another thread while the worker runs, but not concurrently
 *       with or after ovclash_finish_pipeline on the same handle. ovstage reads an atomic
 *       per-context flag. OpenUSD reads the shared engine's volatile process-global flag, which
 *       provides no C++ thread-safety guarantee.
 */
OVCLASH_API ovclash_result_t ovclash_async_is_cancelled(ovclash_async_t* async, uint32_t* out_cancelled);

#if OVCLASH_STANDALONE
// Entry points and types declared only when OVCLASH_STANDALONE is defined: file-backed stage
// access (ovclash_open_stage, ovclash_get_prim_matrix), path encoding and path scopes
// (ovclash_sdf_path_to_int, ovclash_set_scope_paths), the per-context last error, settings
// readback, version queries and the DLPack report interop.
/**
 * @brief Opens a file-backed USD stage as the clash source.
 *
 * Only present in OVCLASH_STANDALONE builds. Both backends require an existing local filesystem
 * path. A std::filesystem::exists check runs before any USD or ovstage resolution, so an asset URI
 * that is not a plain file is rejected up front. OpenUSD opens a pxr::UsdStage, swaps the entry of
 * this context in the process-global standalone stage registry, and immediately pushes an empty
 * scope and the new stage id into the engine. ovstage populates the stage through ovstage, clears
 * the scope lists of the context, and advances the selected-table extraction generation.
 *
 * Result *details* computed earlier then fail with a "result details are stale" message on both
 * backends until matching results are published. Aggregate counts computed earlier are kept, but
 * the name0 and name1 views that the result getters already returned are not. The installed
 * pipeline shape stays available, but a started pipeline must restart at step 0, or be
 * recreated, before it runs further steps against the new stage. A path with the same spelling
 * is still a replacement and reads the file again. On OpenUSD, a layer that another holder in
 * the process keeps open, such as the stage of another context, stays shared through the Sdf
 * layer registry and is not read again. Neither backend restores OVCLASH_SETTINGS_FLAG_FULL_SCENE
 * after ovclash_set_scope_paths cleared it.
 *
 * @param context [in] Context from ovclash_create_context. Must not be null.
 * @param path [in] UTF-8 filesystem path. ptr may be null only when length is 0, which is then
 *             rejected as an empty path. Copied during the call.
 * @return OVCLASH_STATUS_SUCCESS when the stage is open; OVCLASH_STATUS_ERROR when context is
 *         null/invalid, an async pipeline handle is active, path has a null ptr with a non-zero
 *         length, is not valid UTF-8, contains an embedded NUL, is empty, does not exist, or
 *         fails to open. When the open itself fails (missing file or USD/ovstage error), the
 *         previous stage is dropped, so the context has no source until the next successful
 *         call. Argument, UTF-8, embedded-NUL and empty-path errors are rejected before the
 *         replacement point and leave the current source and result details unchanged. An actual
 *         open attempt, successful or failed, retires them on both backends.
 *
 * @note Lifetime: clears the last error of the context on entry, so read ovclash_get_last_error
 *       only after this call returned OVCLASH_STATUS_ERROR.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive. The
 *       OpenUSD stage registry is mutex-protected and shared process-wide.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_open_stage(ovclash_context_t* context, ovclash_string_t path);
/**
 * @brief Reads a prim's local-to-world transform from the open stage.
 *
 * Only present in OVCLASH_STANDALONE builds. `time` is a USD time code, not seconds. Both
 * backends fill out_matrix with identity before doing any work, so a call that fails after
 * context validation still leaves a well-defined matrix. Nothing is written when out_matrix is
 * null or the context is null or invalid. OpenUSD evaluates a fresh pxr::UsdGeomXformCache per
 * call against the open stage, and fails when the prim does not exist.
 *
 * For a finite time, ovstage uses a separate, lazy, selector-only numeric population of the
 * source. Reads at the same time reuse it. A forward read applies the new time and reads the
 * local and reset columns at the new sealed population ordinal; a rewind reopens the population.
 * ovstage converts USD time codes to population seconds with the effective composed stage rate:
 * timeCodesPerSecond, else framesPerSecond, else 24.0. The resolved rate must be positive and
 * finite. Conversion fails when a finite USD time code overflows population seconds, or when a
 * nonzero time code underflows to zero seconds. This applies to text and binary stages alike.
 * The numeric path composes world matrices itself, without a separate hierarchy pass. It never
 * advances the USD-default RENDERING population that selection uses. Numeric reads check that
 * the requested prim exists, including identity and instance-proxy prims. An explicit
 * source-read error fails the call with that error.
 *
 * ovstage never substitutes the pipeline pose of a selected mesh or point cloud, which belongs to
 * the last evaluated sample rather than to `time`. A read that yields neither a matrix nor its
 * own error fails with "Failed to query prim transform: " followed by prim_path. A prim missing at
 * the default time keeps a valid selection state. A genuine read or resource failure that resets
 * the selection population invalidates its selected-table details and installed pipeline.
 * Recreate the pipeline after addressing the error, rather than continuing its remaining steps.
 * Direct queries join any matrix-prefetch callback before they access the source. They keep its
 * copied future window, unless the query retires the selection population.
 *
 * @param context [in] Context from ovclash_create_context. Must not be null.
 * @param prim_path [in] UTF-8 absolute prim path. ptr may be null only when length is 0, which is
 *                  then rejected as empty. Copied during the call.
 * @param time [in] USD time code at which the transform is evaluated.
 * @param out_matrix [out] Receives a row-major 4x4 transform. Must point to 16 writable doubles.
 * @return OVCLASH_STATUS_SUCCESS when the transform is written; OVCLASH_STATUS_ERROR when
 *         out_matrix or context is null/invalid, an async pipeline handle is active, prim_path is
 *         an invalid span, non-UTF-8, contains an embedded NUL or is empty, no stage is open, or
 *         the prim cannot be resolved.
 *
 * @note Lifetime: clears the last error of the context on entry; out_matrix is caller-owned.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_get_prim_matrix(ovclash_context_t* context,
                                                     ovclash_string_t prim_path,
                                                     double time,
                                                     double* out_matrix /* 16 doubles, row-major */);
/**
 * @brief Encodes an absolute USD prim path into the uint64 id used by the scope and
 *        immediate-mode entry points.
 *
 * Only present in OVCLASH_STANDALONE builds. The path must be an absolute prim path. OpenUSD
 * enforces that through pxr::SdfPath, ovstage through an equivalent USD-identifier check. The two
 * encodings are unrelated, and only the backend that produced an id accepts it. Both backends
 * keep what an id names in a mutex-protected, process-global registry. The id therefore stays
 * valid and stable for the same path for the process lifetime, whether or not a stage holds that
 * path. OpenUSD returns the raw bit pattern of a pxr::SdfPath, a refcounted path-node handle. Its
 * registry keeps the node alive, but the id must never be persisted, hashed for storage or shared
 * across processes. ovstage interns the path string and returns a small non-zero id. Neither
 * registry is ever pruned, not by ovclash_release_context and not by opening another stage.
 * Encoding an unbounded number of distinct paths therefore grows memory until exit.
 *
 * @param prim_path [in] UTF-8 absolute prim path. ptr may be null only when length is 0, which is
 *                  rejected. Copied during the call.
 * @param out_encoded [out] Receives the encoded id. Must not be null; left untouched on failure.
 * @return OVCLASH_STATUS_SUCCESS when out_encoded is written; OVCLASH_STATUS_ERROR when
 *         out_encoded is null, or prim_path is an invalid span, non-UTF-8, contains an embedded
 *         NUL, is empty, or is not an absolute prim path, or, on ovstage, when the call runs inside
 *         an owner-operation callback on the same thread (see ovclash_ovstage_run_owner_operation).
 *         There is no context argument, so the return code is the only failure signal. Nothing is
 *         recorded for ovclash_get_last_error.
 *
 * @note Threading: safe to call concurrently. Both registries are mutex-protected. OpenUSD path
 *       construction relies on the concurrency guarantees of pxr::SdfPath.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_sdf_path_to_int(ovclash_string_t prim_path, uint64_t* out_encoded);
/**
 * @brief Restricts clash detection to one or two sets of USD prim paths.
 *
 * Only present in OVCLASH_STANDALONE builds. Prefer it over ovclash_set_scope, which needs
 * pre-encoded ids. paths1 alone means "paths1 against itself", both non-empty means "paths1
 * against paths2", and both empty means no scope. A scope entry also covers its descendants.
 * Meshes that match both scopes belong to paths1 only: the two-scope mesh query compares paths1
 * against paths2 minus paths1. Identical scopes therefore produce no mesh-vs-mesh pairs.
 * Validation is all-or-nothing: the scope of the context is replaced only after every entry
 * passes.
 *
 * No scope is NOT the same as whole-stage traversal. Selection reads two independent pieces of
 * state, this scope and OVCLASH_SETTINGS_FLAG_FULL_SCENE:
 *
 *   scope non-empty                   -> the scope, whatever the flag says
 *   scope empty, FULL_SCENE set       -> whole stage
 *   scope empty, FULL_SCENE cleared   -> nothing at all
 *
 * The flag starts set on a fresh context and can be restored explicitly. Side effect: when either
 * scope is non-empty, this call clears the flag on both backends. Clearing the scope paths does
 * not restore it. Neither does a successful ovclash_open_stage or, on ovstage,
 * ovclash_ovstage_attach_stage, although both clear the scope, so set the scope after them.
 * Restore full-scene traversal with ovclash_set_full_scene(context, true), or with settings that
 * enable OVCLASH_SETTINGS_FLAG_FULL_SCENE. Both of those calls also clear the scope: the engine
 * scope paths on OpenUSD, the scope lists of the context on ovstage. To keep a non-empty explicit
 * scope, apply them before setting the paths.
 *
 * OpenUSD converts the strings to SdfPath ids and pushes them into the engine with the current
 * stage id. ovstage stores the strings and prefix-matches them during selection. Both backends
 * read the scope only when a run selects meshes. A call between the steps of a run therefore
 * leaves the selected meshes and the two-scope grouping of that run unchanged.
 *
 * @param context [in] Context from ovclash_create_context. Must not be null.
 * @param paths1 [in] First scope. May be null only when paths1_count is 0. Copied during the call.
 * @param paths1_count [in] Entry count of paths1.
 * @param paths2 [in] Second scope. May be null only when paths2_count is 0. Copied during the call.
 * @param paths2_count [in] Entry count of paths2.
 * @return OVCLASH_STATUS_SUCCESS when the scope is stored; OVCLASH_STATUS_ERROR when context is
 *         null/invalid, an async pipeline handle is active, an array is null with a non-zero
 *         count, or any entry is an invalid span, non-UTF-8, contains an embedded NUL, is empty,
 *         or is not an absolute prim path.
 *
 * @note Lifetime: clears the last error of the context on entry. The caller keeps ownership of
 *       every string and array passed in.
 * @note Threading: serialize per context; rejected while an async pipeline handle is alive.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_result_t ovclash_set_scope_paths(ovclash_context_t* context,
                                                     const ovclash_string_t* paths1,
                                                     size_t paths1_count,
                                                     const ovclash_string_t* paths2,
                                                     size_t paths2_count);
/**
 * @brief Returns the last error message recorded on a context.
 *
 * Only present in OVCLASH_STANDALONE builds. It is the only diagnostic channel of the canonical
 * ABI, with no thread-local fallback. The context-free entry points never record anything here:
 * ovclash_sdf_path_to_int, ovclash_get_backend_info, ovclash_get_result_string,
 * ovclash_get_version, ovclash_get_version_string, ovclash_overlap_report_get_dltensor and the
 * ovclash_reset_* helpers. ovclash_get_timeline_data records only on ovstage, and only when it is
 * rejected inside an owner-operation callback with a non-null context.
 *
 * A success does NOT always clear the text. Calls that clear it include ovclash_open_stage,
 * ovclash_get_prim_matrix, ovclash_get_stage_timeline_data, ovclash_set_scope_paths and, on
 * ovstage, the start of a run and the optional attach and detach operations. A message can
 * outlive the call that produced it. On ovstage, a run clears the text on entry to
 * ovclash_run_pipeline_step with index 0, and for ovclash_run_pipeline_async the worker clears it
 * when the run begins, so a
 * synchronized read during the run may see the message of the previous run or of the current
 * one. Read the text after a call returned OVCLASH_STATUS_ERROR. A non-empty string never means
 * by itself that the last call failed.
 *
 * One message is recorded by a call that SUCCEEDS. On ovstage, a run that dropped meshes it could
 * not cook reports that here (see ovclash_stats_t::nbCookedMeshes), while every step still
 * returns OVCLASH_STATUS_SUCCESS. The message belongs to its run, and the next run clears it on
 * entry. Any call in between that records or clears an error still replaces it, so read it before
 * doing anything else with the context. nbCookedMeshes below nbMeshes minus nbSharedMeshes is the
 * durable signal.
 *
 * @param context [in] Context to query. May be null, which yields an empty { NULL, 0 } view.
 * @return Borrowed UTF-8 view of the recorded message. It is null-terminated and the reported
 *         length excludes the terminator; the view is empty when nothing has been recorded.
 *
 * @note Lifetime: on ovstage the view points at a thread-local copy, which the next
 *       ovclash_get_last_error on the same thread invalidates. While the out-of-memory flag of
 *       the context is set, ovstage returns a static diagnostic that needs no allocation instead.
 *       Once the thread-local storage of the thread is destroyed, a static notice stands in for a
 *       recorded message; glibc destroys that storage of the main thread at exit, before static
 *       destructors run. On OpenUSD the context owns the text. The next call on that context that
 *       records or clears an error invalidates it, and so does ovclash_release_context. Copy the
 *       bytes to keep them on either backend.
 * @note Threading: access is synchronized on ovstage, where the async pipeline worker writes the
 *       same string while a pipeline runs. A read during the run is safe, but which message it
 *       returns is nondeterministic.
 * @ingroup ovclash_c_api
 */
OVCLASH_API ovclash_string_t ovclash_get_last_error(const ovclash_context_t* context);

/**
 * @brief Reads back the settings currently applied to a context.
 *
 * The inverse of ovclash_set_settings(). A fresh context reports exactly the
 * ovclash_reset_settings() table on both backends.
 *
 * Every individual ovclash_set_* setter is reflected here too, so the result always describes the
 * live configuration, not only the last whole-struct assignment. nbTasks reads back clamped to
 * 256, as both backends store it.
 *
 * @param context [in] Context to query. Must not be null and must be valid.
 * @param out_settings [out] Receives the current settings. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if either argument is
 *         null or the context is invalid (nothing written). No error text is recorded.
 *
 * @note Threading: must not run concurrently with a setter on the same context. It is not
 *       rejected while an async pipeline handle is active. The setters are, so a read during an
 *       async run is stable.
 */
OVCLASH_API ovclash_result_t ovclash_get_settings(const ovclash_context_t* context, ovclash_settings_t* out_settings);

/**
 * @defgroup ovclash_version Version information
 * @{
 */

/**
 * @brief Reports the runtime version of the loaded ovclash library.
 *
 * Use it to check that the loaded shared library matches the headers the caller compiled
 * against. The compile-time constants are OVCLASH_VERSION_MAJOR, _MINOR and _PATCH in
 * `<ovclash/version.h>`.
 *
 * @param out_major [out] Major version. Must not be null.
 * @param out_minor [out] Minor version. Must not be null.
 * @param out_patch [out] Patch version. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if any argument is null (nothing
 *         written).
 *
 * @note Threading: safe to call concurrently. Reads no context and no global state.
 */
OVCLASH_API ovclash_result_t ovclash_get_version(uint32_t* out_major, uint32_t* out_minor, uint32_t* out_patch);

/**
 * @brief Returns the runtime version as a "MAJOR.MINOR.PATCH" string, for example "1.2.3".
 *
 * @return An explicit-length, null-terminated string with static storage duration. The library
 *         owns it. It stays valid for the lifetime of the process and must not be freed or
 *         modified.
 *
 * @note Threading: safe to call concurrently.
 */
OVCLASH_API ovclash_string_t ovclash_get_version_string(void);

/** @} */

/**
 * @defgroup ovclash_dlpack DLPack array interop
 * @{
 */

/** Selects which array of an ovclash_overlap_report_t to describe as a tensor. */
// Keep invalid numeric inputs representable until the API validates their range.
typedef enum ovclash_report_buffer_t
#ifdef __cplusplus
    : uint32_t
#endif
{
    OVCLASH_REPORT_BUFFER_FACES0 = 0, //!< collidingFaces0: uint32, shape [nbCollidingFaces0]
    OVCLASH_REPORT_BUFFER_FACES1 = 1, //!< collidingFaces1: uint32, shape [nbCollidingFaces1]
    OVCLASH_REPORT_BUFFER_OUTLINE = 2, //!< collidingSegments: float32, shape [nbCollidingSegments, 6]
    OVCLASH_REPORT_BUFFER_POINTS0 = 3, //!< collidingPoints0: uint32, shape [nbCollidingPoints0]
} ovclash_report_buffer_t;

/**
 * @brief A DLTensor plus inline storage for its shape and strides.
 *
 * DLTensor::shape and ::strides are pointers, so their arrays need storage. This transparent
 * struct holds that storage inline, so the view can live on the stack and needs no allocation or
 * cleanup:
 *
 *     ovclash_dltensor_view_t view;
 *     if (ovclash_overlap_report_get_dltensor(&report, OVCLASH_REPORT_BUFFER_FACES0, &view).status ==
 *         OVCLASH_STATUS_SUCCESS)
 *     {
 *         consume(&view.tensor);
 *     }
 *
 * A copy of a filled struct still has tensor.shape and tensor.strides pointing into the
 * original. Fill a fresh view instead of copying a filled one.
 */
typedef struct ovclash_dltensor_view_t
{
    DLTensor tensor; //!< shape/strides point into the two arrays below
    int64_t shape[2];
    int64_t strides[2];
} ovclash_dltensor_view_t;

/**
 * @brief Describes one overlap-report array as a DLPack tensor, without copying.
 *
 * The tensor borrows the memory of the report: tensor.data aliases the matching pointer inside
 * @p report. Nothing is allocated and there is nothing to free.
 *
 * @param report [in] A report previously filled by ovclash_create_overlap_report()
 *                    or ovclash_create_overlap_report_ex(). Must not be null.
 * @param buffer [in] Which array to describe.
 * @param out_view [out] Receives the tensor and its shape/stride storage. Must not be null.
 * @return OVCLASH_STATUS_SUCCESS, or OVCLASH_STATUS_ERROR if @p report or
 *         @p out_view is null or @p buffer is not a valid enumerator. There is no context, so
 *         nothing is recorded for ovclash_get_last_error.
 *
 * @note Results: an array that the report flags did not request has count 0. That is reported
 *       as success with a zero-length tensor and `tensor.data == NULL`, not as an error.
 * @note Lifetime: the tensor is valid only while @p report is; it dangles after
 *       ovclash_release_overlap_report(). It carries no deleter, so it must never reach a
 *       consumer that expects to own a DLManagedTensor.
 * @note Device: always kDLCPU, device_id 0.
 * @note Threading: safe to call concurrently with distinct @p out_view storage; the call only
 *       reads @p report.
 */
OVCLASH_API ovclash_result_t ovclash_overlap_report_get_dltensor(const ovclash_overlap_report_t* report,
                                                                 ovclash_report_buffer_t buffer,
                                                                 ovclash_dltensor_view_t* out_view);

/** @} */
#endif // OVCLASH_STANDALONE

/** @} */ // ovclash_c_api

#ifdef __cplusplus
}
#endif
