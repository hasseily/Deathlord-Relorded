# Run from any directory with: cmake -P cmake/PrepareCleanHdv.cmake
# CI fetches this historical commit before invoking the script. The image is
# extracted into ignored build output, never into assets/Images or user saves.
cmake_minimum_required(VERSION 3.20)
find_package(Git REQUIRED)
get_filename_component(source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(output_dir "${source_dir}/build-package-input")
set(output_hdv "${output_dir}/Deathlord PRODOS.hdv")
set(clean_commit "645e8eb0c28b984fa42a05bac314d4d509e8a711")
set(clean_sha256 "32311bcd479d902c693a0e14fee572f64cc4661d2c0cf32b28400170d3cf33d9")

file(MAKE_DIRECTORY "${output_dir}")
execute_process(
    COMMAND "${GIT_EXECUTABLE}" show "${clean_commit}:assets/Images/Deathlord PRODOS.hdv"
    WORKING_DIRECTORY "${source_dir}"
    OUTPUT_FILE "${output_hdv}.tmp"
    ERROR_VARIABLE git_error
    RESULT_VARIABLE git_result
)
if(NOT git_result EQUAL 0)
    file(REMOVE "${output_hdv}.tmp")
    message(FATAL_ERROR "Cannot extract original HDV: ${git_error}\nFetch ${clean_commit} from origin first.")
endif()
file(SHA256 "${output_hdv}.tmp" actual_sha256)
if(NOT actual_sha256 STREQUAL clean_sha256)
    file(REMOVE "${output_hdv}.tmp")
    message(FATAL_ERROR "Original HDV checksum mismatch: ${actual_sha256}")
endif()
file(RENAME "${output_hdv}.tmp" "${output_hdv}")
message(STATUS "Verified original HDV: ${output_hdv} (${actual_sha256})")
