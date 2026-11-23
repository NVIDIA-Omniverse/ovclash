// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ovclash/ovclash_export.h"
#include "ovclash_suite/ovclash_suite_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \defgroup ovclash_suite_model_api ovclash_suite model C API
 *
 * C-first model API for the ClashQuery, ClashInfo, ClashFrameInfo and PointCloudSettings
 * objects.
 *
 * Common preconditions: handle parameters must be non-null unless the function documents a
 * destroy or cleanup case that accepts null. Output pointers must be non-null. Input
 * ovclash_suite_string_t fields must be valid borrowed UTF-8 spans: ptr may be null only when
 * length is zero. A counted array field, such as a frame face or point buffer or a frame view
 * array, may be null only when its count is zero.
 *
 * Common side effects: create functions allocate a handle and destroy functions release one.
 * Mutators modify the supplied handle and may update the last-modified metadata. Serializers
 * write UTF-8 JSON. Getters return borrowed views backed by the handle.
 *
 * Common ownership: the caller owns every handle returned through an out parameter and destroys
 * it with the matching destroy function. Views and string spans returned by getters are borrowed.
 * They stay valid until the handle is mutated or destroyed. A frame view that a clash info
 * synthesizes (see ovclash_suite_clash_info_get_frame_info) lasts only until the next frame read
 * on that handle. Input strings and counted buffers are copied before the call returns.
 *
 * Common threading: model handles are thread-compatible, so independent handles may be used
 * concurrently. A single handle is not internally synchronized; callers serialize access to a
 * shared handle. Last-error text is thread-local and valid until the next Suite API call on the
 * same thread.
 *
 * Common error catalogue for each function returning ovclash_suite_result_t:
 * - OVCLASH_SUITE_STATUS_SUCCESS: the requested operation completed and every output parameter
 *   documented for the function is initialized.
 * - OVCLASH_SUITE_STATUS_ERROR: an invalid handle, output pointer, UTF-8 text or string span, a
 *   counted buffer that is null with a non-zero count, invalid JSON where JSON is required,
 *   non-finite numeric data in a serializer, an output buffer that is too small, allocation
 *   failure, or a caught C++ exception. ovclash_suite_get_thread_last_error returns the detail.
 *
 * Serializer buffer rule: out_buffer == NULL with out_buffer_size == 0 is the size query. It
 * succeeds when the model is serializable and out_required_size is valid. A non-null buffer that
 * cannot hold the required bytes plus a trailing null returns OVCLASH_SUITE_STATUS_ERROR. That
 * error still writes out_required_size and leaves the buffer unchanged. NaN or an infinity in any
 * serialized scalar, matrix, float array or nested frame returns OVCLASH_SUITE_STATUS_ERROR with a
 * non-finite-value diagnostic, for size queries and buffer writes alike. That error leaves
 * out_buffer and out_required_size unchanged, so no partial JSON is returned. Serialization never
 * mutates the model or its borrowed views.
 *
 * Numeric JSON formatting keeps finite values at round-trip precision and writes negative zero
 * as -0.0, also in matrices and float arrays. Conversion uses the classic numeric locale and the
 * default floating-point environment, then restores the complete environment of the caller. This
 * does not change how SQLite stores a scalar REAL negative zero.
 *
 * The common preconditions, side effects, ownership, threading rules and error catalogue above
 * are part of the contract of every function, unless the function documents a narrower return
 * section. Short mutator comments name only the field-specific argument, because handle,
 * ownership, threading and error behavior is the same across the model API.
 *
 * Example:
 * \code
 * ovclash_suite_query_t* query = 0;
 * if (ovclash_suite_query_create(&query).status == OVCLASH_SUITE_STATUS_SUCCESS) {
 *     ovclash_suite_query_set_query_name(query, (ovclash_suite_string_t){ "Q", 1 });
 *     ovclash_suite_query_destroy(query);
 * }
 * \endcode
 *
 * Settings JSON example:
 * \code
 * ovclash_suite_query_t* query = 0;
 * ovclash_suite_query_create(&query);
 * ovclash_suite_result_t r = ovclash_suite_query_load_settings_json(
 *     query, (ovclash_suite_string_t){ "{\"SETTING_DYNAMIC\":false}", 25 });
 * if (r.status != OVCLASH_SUITE_STATUS_SUCCESS) {
 *     ovclash_suite_string_t err = ovclash_suite_get_thread_last_error();
 *     (void)err;
 * }
 * ovclash_suite_query_destroy(query);
 * \endcode
 *
 * Frame-buffer example:
 * \code
 * uint32_t faces[] = { 0, 1, 2 };
 * ovclash_suite_clash_frame_info_record_view_t frame = { 0 };
 * frame.nb_usd_faces0 = 3;
 * frame.usd_faces0 = faces;
 * ovclash_suite_clash_frame_info_t* owned = 0;
 * ovclash_suite_clash_frame_info_create_from_view(&frame, &owned);
 * ovclash_suite_clash_frame_info_destroy(owned);
 * \endcode
 */
/** @{ */

/**
 * Creates a clash query model with the legacy defaults.
 *
 * identifier is -1 and settings_json is {}. creation_timestamp and last_modified_timestamp hold
 * the current time (see ovclash_suite_current_timestamp). last_modified_by holds the current user
 * name (see ovclash_suite_current_user_name). Every other string is empty. The Python ClashQuery
 * constructor takes its timestamp and user-name defaults from this function.
 *
 * \param out_query Receives the created query handle. Must not be null. Destroy with ovclash_suite_query_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_create(ovclash_suite_query_t** out_query);

/**
 * Creates a clash query model by copying a public query view.
 *
 * \param view Query view to copy. String views are copied by the created handle. Must not be null.
 * \param out_query Receives the created query handle. Must not be null. Destroy with ovclash_suite_query_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_create_from_view(const ovclash_suite_query_view_t* view,
                                                                        ovclash_suite_query_t** out_query);

/**
 * Destroys a clash query model.
 *
 * \param query Query handle returned by a query create function. May be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_destroy(ovclash_suite_query_t* query);

/**
 * Reads a view of a clash query model.
 *
 * \param query Query handle to read. Must not be null.
 * \param out_view Receives a view backed by query. Must not be null. Valid until query is mutated or destroyed.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_get_view(const ovclash_suite_query_t* query,
                                                                ovclash_suite_query_view_t* out_view);

/**
 * Serializes a clash query to the legacy Python dictionary JSON shape.
 *
 * \param query Query handle to serialize. Must not be null.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_serialize_json(const ovclash_suite_query_t* query,
                                                                      char* out_buffer,
                                                                      size_t out_buffer_size,
                                                                      size_t* out_required_size);

/**
 * Resets a clash query identifier to the unsaved value -1.
 *
 * Preconditions: query must be a valid handle returned by a query create
 * function. Passing null is invalid.
 *
 * \param query Query handle to mutate. Must not be null.
 *
 * Side effects: mutates only the identifier field; the query remains otherwise
 * unchanged and no persistent data store is touched.
 *
 * Ownership: query remains owned by the caller and must still be destroyed with
 * ovclash_suite_query_destroy. No borrowed views are returned.
 *
 * Threading: callers must serialize access to query while mutating it.
 * Independent query handles may be reset concurrently.
 *
 * Example:
 * \code
 * ovclash_suite_query_t* query = 0;
 * ovclash_suite_query_create(&query);
 * ovclash_suite_query_reset_identifier(query);
 * ovclash_suite_query_destroy(query);
 * \endcode
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when the identifier is reset.
 *         OVCLASH_SUITE_STATUS_ERROR when query is null or a C++ exception
 *         is caught. Failure detail is available through
 *         ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_reset_identifier(ovclash_suite_query_t* query);

/**
 * Updates the last-modified timestamp and user fields from the Suite clock and user name.
 *
 * Preconditions: query must be a valid handle returned by a query create
 * function. Passing null is invalid.
 *
 * \param query Query handle to mutate. Must not be null.
 *
 * Side effects: sets last_modified_timestamp to ovclash_suite_current_timestamp text and
 * last_modified_by to ovclash_suite_current_user_name text, which is empty when no user name
 * is available. No persistent data store is touched until the caller writes the query
 * through a data API.
 *
 * Ownership: query remains owned by the caller and must still be destroyed with
 * ovclash_suite_query_destroy. Generated string storage is owned by query.
 *
 * Threading: callers must serialize access to query while mutating it.
 * Independent query handles may update timestamps concurrently.
 *
 * Example:
 * \code
 * ovclash_suite_query_t* query = 0;
 * ovclash_suite_query_create(&query);
 * ovclash_suite_query_update_last_modified(query);
 * ovclash_suite_query_destroy(query);
 * \endcode
 *
 * \return OVCLASH_SUITE_STATUS_SUCCESS when metadata is updated.
 *         OVCLASH_SUITE_STATUS_ERROR when query is null, allocation fails, or a
 *         C++ exception is caught. Failure detail is available through
 *         ovclash_suite_get_thread_last_error.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_update_last_modified(ovclash_suite_query_t* query);

/**
 * Sets the query identifier.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param identifier Identifier value to store.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_identifier(ovclash_suite_query_t* query, int64_t identifier);

/**
 * Sets the query name.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed query name to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_query_name(ovclash_suite_query_t* query,
                                                                      ovclash_suite_string_t value);

/**
 * Sets the first object scope path.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed first object scope path to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_object_a_path(ovclash_suite_query_t* query,
                                                                         ovclash_suite_string_t value);

/**
 * Sets the second object scope path.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed second object scope path to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_object_b_path(ovclash_suite_query_t* query,
                                                                         ovclash_suite_string_t value);

/**
 * Sets the settings JSON text and updates the last-modified metadata.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed settings JSON text to copy. Updates last-modified metadata.
 *
 * \return Success when value is empty or a syntactically valid JSON value of any kind (object,
 *         array, string, number, true, false or null), and was copied. Failure when query is
 *         null, value has an invalid span or invalid UTF-8, value is neither valid JSON nor
 *         empty, allocation fails, or an exception is caught.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_settings_json(ovclash_suite_query_t* query,
                                                                         ovclash_suite_string_t value);

/**
 * Loads the settings JSON text without updating the last-modified metadata.
 *
 * The Python ClashQuery.load_settings_from_str wraps this function as a bool-return helper. That
 * helper returns False for empty or invalid JSON, although this function accepts empty text.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed settings JSON text to copy without updating last-modified metadata.
 *
 * \return Success when value is empty or a syntactically valid JSON value of any kind (object,
 *         array, string, number, true, false or null), and was copied without touching the
 *         last-modified metadata. Failure when query is null, value has an invalid span or
 *         invalid UTF-8, value is neither valid JSON nor empty, allocation fails, or an exception
 *         is caught.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_load_settings_json(ovclash_suite_query_t* query,
                                                                          ovclash_suite_string_t value);

/**
 * Sets the creation timestamp.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed creation timestamp in Suite storage format.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_creation_timestamp(ovclash_suite_query_t* query,
                                                                              ovclash_suite_string_t value);

/**
 * Sets the last-modified timestamp.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed last-modified timestamp in Suite storage format.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_last_modified_timestamp(ovclash_suite_query_t* query,
                                                                                   ovclash_suite_string_t value);

/**
 * Sets the last-modified user name.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed last-modified user name to copy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_last_modified_by(ovclash_suite_query_t* query,
                                                                            ovclash_suite_string_t value);

/**
 * Sets the query comment.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed comment text to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_comment(ovclash_suite_query_t* query,
                                                                   ovclash_suite_string_t value);

/**
 * Sets the region-of-interest include path.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed ROI include path to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_roi_include_path(ovclash_suite_query_t* query,
                                                                            ovclash_suite_string_t value);

/**
 * Sets the region-of-interest exclude path.
 *
 * \param query Query handle to mutate. Must not be null.
 * \param value Length-prefixed ROI exclude path to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_set_roi_exclude_path(ovclash_suite_query_t* query,
                                                                            ovclash_suite_string_t value);

/**
 * Compares every query field. The settings JSON compares byte for byte rather than as parsed JSON,
 * so settings that differ only in key order or number spelling (1 against 1.0) are unequal.
 *
 * \param lhs First query handle. Must not be null.
 * \param rhs Second query handle. Must not be null.
 * \param out_equal Receives 1 when equal, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_query_equals(const ovclash_suite_query_t* lhs,
                                                              const ovclash_suite_query_t* rhs,
                                                              uint32_t* out_equal);

/**
 * Creates point-cloud settings with the legacy defaults.
 *
 * identifier is -1, both paths are empty, save_to_session_layer and skip_rendering are 1, and
 * max_lod_level is OVCLASH_SUITE_POINT_CLOUD_SETTINGS_MAX_LOD_LEVEL (1000000). These are the
 * defaults of the Python PointCloudSettings constructor.
 *
 * \param out_settings Receives the created settings handle. Must not be null. Destroy with
 * ovclash_suite_point_cloud_settings_destroy.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_create(ovclash_suite_point_cloud_settings_t** out_settings);

/**
 * Creates point-cloud settings by copying a public settings view.
 *
 * \param view Settings view to copy. String views are copied by the created handle. Must not be null.
 * \param out_settings Receives the created settings handle. Must not be null. Destroy with
 * ovclash_suite_point_cloud_settings_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_point_cloud_settings_create_from_view(
    const ovclash_suite_point_cloud_settings_view_t* view, ovclash_suite_point_cloud_settings_t** out_settings);

/** \param settings Settings handle returned by a point-cloud-settings create function. May be null. */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_destroy(ovclash_suite_point_cloud_settings_t* settings);

/**
 * Reads a view of point-cloud settings.
 *
 * \param settings Settings handle to read. Must not be null.
 * \param out_view Receives a view backed by settings. Must not be null. Valid until settings is mutated or destroyed.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_point_cloud_settings_get_view(
    const ovclash_suite_point_cloud_settings_t* settings, ovclash_suite_point_cloud_settings_view_t* out_view);

/**
 * Serializes point-cloud settings to JSON.
 *
 * \param settings Settings handle to serialize. Must not be null.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_serialize_json(const ovclash_suite_point_cloud_settings_t* settings,
                                                  char* out_buffer,
                                                  size_t out_buffer_size,
                                                  size_t* out_required_size);

/** \param settings Settings handle to mutate. Must not be null. */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_reset_identifier(ovclash_suite_point_cloud_settings_t* settings);

/**
 * Sets the point-cloud settings identifier.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param identifier Identifier value to store.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_set_identifier(ovclash_suite_point_cloud_settings_t* settings, int64_t identifier);

/**
 * Sets the source point-cloud prim path.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param value Length-prefixed source point-cloud prim path to copy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_point_cloud_settings_set_point_cloud_prim_path(
    ovclash_suite_point_cloud_settings_t* settings, ovclash_suite_string_t value);

/**
 * Sets the generated destination path.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param value Length-prefixed generated destination path to copy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_point_cloud_settings_set_destination_path(
    ovclash_suite_point_cloud_settings_t* settings, ovclash_suite_string_t value);

/**
 * Sets whether generated data goes to the session layer.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param value Non-zero to save generated data to the session layer.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_point_cloud_settings_set_save_to_session_layer(
    ovclash_suite_point_cloud_settings_t* settings, uint32_t value);

/**
 * Sets whether point-cloud rendering is skipped.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param value Non-zero to skip point-cloud rendering.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_set_skip_rendering(ovclash_suite_point_cloud_settings_t* settings, uint32_t value);

/**
 * Sets the maximum level of detail.
 *
 * \param settings Settings handle to mutate. Must not be null.
 * \param value Maximum level-of-detail value, stored verbatim. No value has a special meaning, -1
 *        included. The default, OVCLASH_SUITE_POINT_CLOUD_SETTINGS_MAX_LOD_LEVEL (1000000, see
 *        ovclash_suite_get_named_constant), is effectively unlimited.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_set_max_lod_level(ovclash_suite_point_cloud_settings_t* settings, int64_t value);

/**
 * Compares every field, including identifier.
 *
 * \param lhs First settings handle. Must not be null.
 * \param rhs Second settings handle. Must not be null.
 * \param out_equal Receives 1 when equal, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_point_cloud_settings_equals(const ovclash_suite_point_cloud_settings_t* lhs,
                                          const ovclash_suite_point_cloud_settings_t* rhs,
                                          uint32_t* out_equal);

/**
 * Creates a clash frame-info model with legacy defaults.
 *
 * identifier and clash_info_id are -1, both object matrices are identity, max_local_depth and
 * every penetration depth are -1.0, and every other field is zero or empty.
 *
 * \param out_frame Receives the created frame handle. Must not be null. Destroy with
 * ovclash_suite_clash_frame_info_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_create(ovclash_suite_clash_frame_info_t** out_frame);

/**
 * Creates a clash frame-info model by copying a public frame view.
 *
 * \param view Frame view to copy. Array views are copied by the created handle. Must not be null.
 * \param out_frame Receives the created frame handle. Must not be null. Destroy with
 * ovclash_suite_clash_frame_info_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_create_from_view(
    const ovclash_suite_clash_frame_info_record_view_t* view, ovclash_suite_clash_frame_info_t** out_frame);

/** \param frame Frame handle returned by a clash-frame-info create function. May be null. */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_destroy(ovclash_suite_clash_frame_info_t* frame);

/**
 * Reads a view of a clash frame-info model.
 *
 * \param frame Frame handle to read. Must not be null.
 * \param out_view Receives a view backed by frame. Must not be null. Valid until frame is mutated or destroyed.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_get_view(
    const ovclash_suite_clash_frame_info_t* frame, ovclash_suite_clash_frame_info_record_view_t* out_view);

/**
 * Serializes a clash frame info to JSON.
 *
 * \param frame Frame handle to serialize. Must not be null.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_serialize_json(
    const ovclash_suite_clash_frame_info_t* frame, char* out_buffer, size_t out_buffer_size, size_t* out_required_size);

/**
 * Tests whether a matrix differs from the stored object 0 matrix.
 *
 * \param frame Frame handle to inspect. Must not be null.
 * \param matrix Row-major 4x4 matrix to compare. Must point to 16 doubles.
 * \param out_changed Receives 0 when every element differs from the stored matrix by less than
 * the legacy tolerance 1e-6, otherwise 1. A NaN difference counts as changed. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_check_object_0_matrix_changed(
    const ovclash_suite_clash_frame_info_t* frame, const double* matrix, uint32_t* out_changed);

/**
 * Tests whether a matrix differs from the stored object 1 matrix.
 *
 * \param frame Frame handle to inspect. Must not be null.
 * \param matrix Row-major 4x4 matrix to compare. Must point to 16 doubles.
 * \param out_changed Receives 0 when every element differs from the stored matrix by less than
 * the legacy tolerance 1e-6, otherwise 1. A NaN difference counts as changed. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_check_object_1_matrix_changed(
    const ovclash_suite_clash_frame_info_t* frame, const double* matrix, uint32_t* out_changed);

/**
 * Reads the overlapping triangle count of a frame.
 *
 * \param frame Frame handle to inspect. Must not be null.
 * \param out_count Receives overlap_elements when the frame has no point indices, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_clash_frame_info_get_overlap_tris(const ovclash_suite_clash_frame_info_t* frame, uint32_t* out_count);

/**
 * Reads the overlapping point count of a frame.
 *
 * \param frame Frame handle to inspect. Must not be null.
 * \param out_count Receives overlap_elements when the frame has point indices, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_clash_frame_info_get_overlap_points(const ovclash_suite_clash_frame_info_t* frame, uint32_t* out_count);

/**
 * Compares every field except identifier and clash_info_id, with exact floating-point equality,
 * so a NaN value never compares equal.
 *
 * \param lhs First frame handle to compare. Must not be null.
 * \param rhs Second frame handle to compare. Must not be null.
 * \param out_equal Receives 1 when equal, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_frame_info_equals(const ovclash_suite_clash_frame_info_t* lhs,
                                                                         const ovclash_suite_clash_frame_info_t* rhs,
                                                                         uint32_t* out_equal);

/**
 * Creates a clash-info model with the legacy defaults.
 *
 * identifier is -1, query_id is 0, overlap_type is OVCLASH_SUITE_OVERLAP_NORMAL, present is 1
 * and state is OVCLASH_SUITE_CLASH_STATE_NEW. max_local_depth is 0.0, and depth_epsilon and every
 * penetration depth are -1.0. Both timestamps hold the current time and last_modified_by holds the
 * current user name. Every other field is zero or empty. No frames are set.
 *
 * \param out_info Receives the created clash-info handle. Must not be null. Destroy with
 * ovclash_suite_clash_info_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_create(ovclash_suite_clash_info_t** out_info);

/**
 * Creates a clash-info model by copying a public clash-info view.
 *
 * \param view Clash-info view to copy. String views are copied by the created handle. Must not be null.
 * \param out_info Receives the created clash-info handle. Must not be null. Destroy with
 * ovclash_suite_clash_info_destroy.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_create_from_view(
    const ovclash_suite_clash_info_record_view_t* view, ovclash_suite_clash_info_t** out_info);

/** \param info Clash-info handle returned by a clash-info create function. May be null. */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_destroy(ovclash_suite_clash_info_t* info);

/**
 * Reads a view of a clash-info model.
 *
 * \param info Clash-info handle to read. Must not be null.
 * \param out_view Receives a view backed by info. Its frame_count is the number of frames set
 *        with ovclash_suite_clash_info_set_frame_views (0 before the first call), whatever the
 *        creating view held. Must not be null. Valid until info is mutated or destroyed.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_get_view(const ovclash_suite_clash_info_t* info,
                                                                     ovclash_suite_clash_info_record_view_t* out_view);

/**
 * Replaces the frame list of the handle with copies of frames.
 *
 * On failure the frame list of the handle is unchanged.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param frames Array of frame views to copy. May be null when frame_count is zero.
 * \param frame_count Number of frame views in frames.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_frame_views(
    ovclash_suite_clash_info_t* info, const ovclash_suite_clash_frame_info_record_view_t* frames, size_t frame_count);

/**
 * Serializes a clash info to JSON.
 *
 * \param info Clash-info handle to serialize. Must not be null.
 * \param out_buffer Destination UTF-8 buffer. May be null when out_buffer_size is zero.
 * \param out_buffer_size Size of out_buffer in bytes, including room for the trailing null when supplied.
 * \param out_required_size Receives the required byte count excluding the trailing null. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_serialize_json(const ovclash_suite_clash_info_t* info,
                                                                           char* out_buffer,
                                                                           size_t out_buffer_size,
                                                                           size_t* out_required_size);

/** \param info Clash-info handle to mutate. Must not be null. */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_reset_identifier(ovclash_suite_clash_info_t* info);

/** \param info Clash-info handle to mutate. Must not be null. */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_update_last_modified(ovclash_suite_clash_info_t* info);

/**
 * Sets the review state.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param value State value to store. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_state(ovclash_suite_clash_info_t* info, uint32_t value);

/**
 * Sets the priority.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param value Priority value to store. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_priority(ovclash_suite_clash_info_t* info,
                                                                         uint32_t value);

/**
 * Sets the person in charge.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param value Length-prefixed person-in-charge text to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_person_in_charge(ovclash_suite_clash_info_t* info,
                                                                                 ovclash_suite_string_t value);

/**
 * Sets the clash comment.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param value Length-prefixed comment text to copy. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_comment(ovclash_suite_clash_info_t* info,
                                                                        ovclash_suite_string_t value);

/**
 * Sets the maximum local depth.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param value Maximum local depth value to store. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_max_local_depth(ovclash_suite_clash_info_t* info,
                                                                                double value);

/**
 * Sets the penetration depth along one signed axis.
 *
 * \param info Clash-info handle to mutate. Must not be null.
 * \param axis Signed axis selector: +1 px, -1 nx, +2 py, -2 ny, +3 pz, -3 nz. Any other value fails.
 * \param value Penetration depth value to store. Updates last-modified metadata.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_set_penetration_depth(ovclash_suite_clash_info_t* info,
                                                                                  int32_t axis,
                                                                                  double value);

/**
 * Tests for a contact: an OVCLASH_SUITE_OVERLAP_NORMAL overlap with min_distance 0 whose
 * max_local_depth is at most depth_epsilon when depth_epsilon is positive, otherwise exactly 0.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_value Receives 1 when true, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_is_contact(const ovclash_suite_clash_info_t* info,
                                                                       uint32_t* out_value);

/**
 * Tests for a hard clash: an OVCLASH_SUITE_OVERLAP_NORMAL overlap with min_distance 0 that is not
 * a contact (see ovclash_suite_clash_info_is_contact).
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_value Receives 1 when true, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_is_hard_clash(const ovclash_suite_clash_info_t* info,
                                                                          uint32_t* out_value);

/**
 * Tests for a soft clash: an OVCLASH_SUITE_OVERLAP_NORMAL overlap with a positive min_distance.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_value Receives 1 when true, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_is_soft_clash(const ovclash_suite_clash_info_t* info,
                                                                          uint32_t* out_value);

/**
 * Tests whether overlap_type is OVCLASH_SUITE_OVERLAP_DUPLICATE.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_value Receives 1 when true, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_is_duplicate(const ovclash_suite_clash_info_t* info,
                                                                         uint32_t* out_value);

/**
 * Tests whether overlap_type is OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_value Receives 1 when true, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_is_point_cloud_clash(const ovclash_suite_clash_info_t* info,
                                                                                 uint32_t* out_value);

/**
 * Reads the overlapping triangle count of a clash info.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_count Receives overlap_elements unless overlap_type is
 *        OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_get_overlap_tris(const ovclash_suite_clash_info_t* info,
                                                                             uint32_t* out_count);

/**
 * Reads the overlapping point count of a clash info.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_count Receives overlap_elements when overlap_type is
 *        OVCLASH_SUITE_OVERLAP_POINT_CLOUD_MESH, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_get_overlap_points(const ovclash_suite_clash_info_t* info,
                                                                               uint32_t* out_count);

/**
 * Reads one frame of a clash info.
 *
 * A frame is found only after ovclash_suite_clash_info_set_frame_views, in one of two cases. When
 * num_records equals the number of frames set, every index below it finds its frame. When
 * num_records is 1 and the last ovclash_suite_clash_info_set_frame_views call passed no frames,
 * any index finds a frame synthesized from the header: start_time, min_distance, max_local_depth
 * and overlap_elements, with identity matrices. A synthesized view lasts only until the next frame
 * read on info.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param index Zero-based frame index to query.
 * \param out_frame Receives the selected frame view when found. Must not be null.
 * \param out_found Receives 1 when a frame was returned, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_clash_info_get_frame_info(const ovclash_suite_clash_info_t* info,
                                        uint32_t index,
                                        ovclash_suite_clash_frame_info_record_view_t* out_frame,
                                        uint32_t* out_found);

/**
 * Reads the frame at index num_records - 1, following the rules of
 * ovclash_suite_clash_info_get_frame_info; nothing is found when num_records is 0.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param out_frame Receives the selected frame view when found. Must not be null.
 * \param out_found Receives 1 when a frame was returned, otherwise 0. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t
ovclash_suite_clash_info_get_last_frame_info(const ovclash_suite_clash_info_t* info,
                                             ovclash_suite_clash_frame_info_record_view_t* out_frame,
                                             uint32_t* out_found);

/**
 * Finds the index of the set frame closest to a timecode.
 *
 * \param info Clash-info handle to inspect. Must not be null.
 * \param timecode Timecode to search for.
 * \param out_index Receives the index of the set frame whose timecode is closest to timecode,
 *        the later one on a tie, or 0 when no frames are set. Frames must be set in ascending
 *        timecode order. Must not be null.
 */
OVCLASH_API ovclash_suite_result_t ovclash_suite_clash_info_get_frame_info_index_by_timecode(
    const ovclash_suite_clash_info_t* info, double timecode, uint32_t* out_index);

/** @} */
#ifdef __cplusplus
}
#endif
