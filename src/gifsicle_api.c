/* Public wrapper API for embedding Gifsicle. */
#ifndef GIFSICLE_BUILD_DLL
#define GIFSICLE_BUILD_DLL 1
#endif
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "gifsicle.h"
#include "kcolor.h"
#include <gifsicle_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
# define GIFSICLE_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
# define GIFSICLE_THREAD_LOCAL __thread
#else
# define GIFSICLE_THREAD_LOCAL
#endif

static int parse_resize(const char *s, int *w, int *h) {
  char *end; long x, y;
  if (!s || !*s) return 1;
  x = strtol(s, &end, 10);
  if (end == s || (*end != 'x' && *end != 'X')) return 0;
  y = strtol(end + 1, &end, 10);
  if (*end || x < 0 || y < 0 || x > 65535 || y > 65535 || (x == 0 && y == 0)) return 0;
  *w = (int)x; *h = (int)y; return 1;
}

static int set_gamma(const char *s) {
  char *end; double value;
  if (!s || !*s || strcmp(s, "srgb") == 0) {
    kc_set_gamma(KC_GAMMA_SRGB, 2.2);
    return 1;
  }
  if (strcmp(s, "oklab") == 0)
    return 0;
  value = strtod(s, &end);
  if (end == s || *end || value <= 0.0)
    return 0;
  kc_set_gamma(KC_GAMMA_NUMERIC, value);
  return 1;
}

static void apply_colors(Gif_Stream *gfs, Gt_OutputData *output_data) {
  kchist kch;
  Gif_Colormap *new_cm;
  uint32_t ntransp;
  int i, any_locals = 0;

  for (i = 0; i < gfs->nimages; ++i)
    if (gfs->images[i]->local)
      any_locals = 1;
  kchist_make(&kch, gfs, &ntransp);
  if (kch.n <= output_data->colormap_size && !any_locals) {
    kchist_cleanup(&kch);
    return;
  }
  output_data->colormap_needs_transparency = ntransp > 0;
  new_cm = colormap_flat_diversity(&kch, output_data);
  colormap_stream(gfs, new_cm, output_data);
  Gif_DeleteColormap(new_cm);
  kchist_cleanup(&kch);
}

static int process_stream(Gif_Stream *gfs, int optimize, int lossy,
                           int colors, const char *resize, const char *gamma,
                           const char *output_path, void **out_ptr,
                           size_t *out_len) {
  Gif_CompressInfo ci;
  Gt_OutputData output_data;
  Gt_Frameset *frameset;
  Gif_Stream *out;
  int w = 0, h = 0, ok, i, huge_stream, compress_immediately;
  FILE *f;
  if (!gfs || gfs->nimages <= 0) return -1;
  if (!parse_resize(resize, &w, &h)) return -2;
  if (colors != 0 && (colors < 2 || colors > 256)) return -6;

  gifsicle_initialize_api();
  Gif_InitCompressInfo(&ci);
  ci.loss = lossy > 0 ? lossy : 0;
  gif_write_info = ci;

  output_data = active_output_data;
  output_data.optimizing = optimize > 0 ? optimize : 0;
  output_data.colormap_size = colors;
  if (resize && *resize) {
    output_data.scaling = GT_SCALING_RESIZE;
    output_data.resize_width = w;
    output_data.resize_height = h;
  }

  frameset = new_frameset(gfs->nimages);
  if (!frameset) return -3;
  /* Keep one caller-owned stream reference while merge_frame_interval
     consumes the references installed by add_frame(). */
  gfs->refcount++;
  for (i = 0; i < gfs->nimages; ++i)
    add_frame(frameset, gfs, gfs->images[i]);
  compress_immediately = (output_data.scaling != GT_SCALING_NONE
                          || (output_data.optimizing & GT_OPT_MASK)
                          || output_data.colormap_size > 0) ? 0 : 1;
  out = merge_frame_interval(frameset, 0, -1, &output_data,
                             compress_immediately, &huge_stream);
  blank_frameset(frameset, 0, 0, 1);
  if (!out) return -3;

  if (!set_gamma(gamma)) {
    Gif_DeleteStream(out);
    return -5;
  }
  thread_count = 1;
  if (resize && *resize)
    resize_stream(out, (double)w, (double)h, 0, SCALE_METHOD_MIX, 0);
  if (colors > 0)
    apply_colors(out, &output_data);
  if (optimize > 0)
    optimize_fragments(out, optimize, huge_stream);
  if (out_ptr && out_len) {
    uint8_t *data = NULL; uint32_t len = 0;
    ok = Gif_WriteMemory(out, &ci, &data, &len);
    Gif_DeleteStream(out);
    if (!ok) return -3;
    *out_ptr = data; *out_len = (size_t)len; return 0;
  }
  if (!output_path || !*output_path) {
    Gif_DeleteStream(out);
    return -4;
  }
  f = fopen(output_path, "wb");
  if (!f) {
    Gif_DeleteStream(out);
    return -4;
  }
  ok = Gif_FullWriteFile(out, &ci, f);
  fclose(f);
  Gif_DeleteStream(out);
  return ok ? 0 : -3;
}

GIFSICLE_API int gifsicle_process_memory(const void *input_ptr, size_t input_len,
                                         int optimize, int lossy, int colors,
                                         const char *resize, const char *gamma,
                                         void **out_ptr, size_t *out_len) {
  Gif_Record record; Gif_Stream *gfs; int result;
  if (!input_ptr || input_len == 0 || input_len > 0xFFFFFFFFU || !out_ptr || !out_len) return -1;
  *out_ptr = NULL; *out_len = 0;
  record.data = (const unsigned char *)input_ptr; record.length = (uint32_t)input_len;
  gfs = Gif_FullReadRecord(&record, GIF_READ_UNCOMPRESSED, "<memory>", 0); if (!gfs) return -1;
  result = process_stream(gfs, optimize, lossy, colors, resize, gamma, NULL,
                          out_ptr, out_len);
  Gif_DeleteStream(gfs); return result;
}

GIFSICLE_API int gifsicle_process_file(const char *input_path, int optimize,
                                       int lossy, int colors,
                                       const char *resize,
                                       const char *gamma,
                                       const char *output_path) {
  FILE *f; Gif_Stream *gfs; int result;
  if (!input_path || !*input_path || !output_path || !*output_path) return -1;
  f = fopen(input_path, "rb"); if (!f) return -1;
  gfs = Gif_FullReadFile(f, GIF_READ_UNCOMPRESSED, input_path, 0); fclose(f); if (!gfs) return -1;
  result = process_stream(gfs, optimize, lossy, colors, resize, gamma,
                          output_path, NULL, NULL);
  Gif_DeleteStream(gfs); return result;
}

GIFSICLE_API const char *gifsicle_get_frame_info(const char *input_path) {
  FILE *f;
  Gif_Stream *gfs;
  static GIFSICLE_THREAD_LOCAL char info[64];
  unsigned long long total_delay = 0;
  unsigned long long average_ms;
  int i;

  if (!input_path || !*input_path) {
    snprintf(info, 64, "-");
    return info;
  }
  f = fopen(input_path, "rb");
  if (!f) {
    snprintf(info, 64, "-");
    return info;
  }
  gfs = Gif_FullReadFile(f, GIF_READ_COMPRESSED, input_path, 0);
  fclose(f);
  if (!gfs || gfs->nimages <= 0) {
    Gif_DeleteStream(gfs);
    snprintf(info, 64, "-");
    return info;
  }
  for (i = 0; i < gfs->nimages; ++i)
    total_delay += gfs->images[i]->delay;
  average_ms = (total_delay * 10 + (unsigned long long) gfs->nimages / 2)
               / (unsigned long long) gfs->nimages;
  snprintf(info, 64, "%d-%llu", gfs->nimages, average_ms);
  Gif_DeleteStream(gfs);
  return info;
}

GIFSICLE_API void gifsicle_free(void *ptr) { Gif_Free(ptr); }
