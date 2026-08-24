/* Public embedding API for the Gifsicle Windows DLL. */
#ifndef GIFSICLE_API_H
#define GIFSICLE_API_H
#include <stddef.h>
#if defined(_WIN32) || defined(__CYGWIN__)
# if defined(GIFSICLE_BUILD_DLL)
#  define GIFSICLE_API __declspec(dllexport)
# elif defined(GIFSICLE_USE_DLL)
#  define GIFSICLE_API __declspec(dllimport)
# else
#  define GIFSICLE_API
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define GIFSICLE_API __attribute__((visibility("default")))
#else
# define GIFSICLE_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* gamma: NULL/"srgb", "1", another numeric value, or "oklab". */
GIFSICLE_API int gifsicle_process_file(const char *input_path, int optimize,
                                       int lossy, int colors,
                                       const char *resize,
                                       const char *gamma,
                                       const char *output_path);
GIFSICLE_API int gifsicle_process_memory(const void *input_ptr,
                                         size_t input_len, int optimize,
                                         int lossy, int colors,
                                         const char *resize,
                                         const char *gamma, void **out_ptr,
                                         size_t *out_len);
/* Returns a DLL-owned string in the form "frame_count-average_ms".
 * The caller must not free or modify it. The pointer remains valid until the
 * next call to this function on the same thread. Read failures return "-". */
GIFSICLE_API const char *gifsicle_get_frame_info(const char *input_path);
GIFSICLE_API void gifsicle_free(void *ptr);

#ifdef __cplusplus
}
#endif
#endif