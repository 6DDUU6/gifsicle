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

/* gamma: NULL/"srgb", "1", or another positive numeric value.
 * Gifsicle 1.95 accepts this for API compatibility; its lossy encoder uses
 * the legacy RGB color-difference algorithm independently of this value. */
GIFSICLE_API int gifsicle_process_file(const char *input_path, int optimize,
                                       int lossy, const char *resize,
                                       const char *gamma,
                                       const char *output_path);
GIFSICLE_API int gifsicle_process_memory(const void *input_ptr,
                                         size_t input_len, int optimize,
                                         int lossy, const char *resize,
                                         const char *gamma, void **out_ptr,
                                         size_t *out_len);
GIFSICLE_API void gifsicle_free(void *ptr);

#ifdef __cplusplus
}
#endif
#endif
