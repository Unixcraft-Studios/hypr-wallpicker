#include "jpeg_loader.h"

#include <stdbool.h>
#include <stdio.h>

#include <jpeglib.h>
#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  struct jpeg_error_mgr base;
  jmp_buf jump_buffer;
} JpegErrorManager;

typedef struct {
  struct jpeg_decompress_struct decoder;
  JpegErrorManager error_manager;
  FILE *file;
  unsigned char *pixels;
  bool decoder_created;
} JpegLoader;

static void JpegErrorExit(j_common_ptr cinfo) {
  JpegErrorManager *error_manager = (JpegErrorManager *)cinfo->err;
  char message[JMSG_LENGTH_MAX];

  (*cinfo->err->format_message)(cinfo, message);
  fprintf(stderr, "Warning: JPEG decoder: %s\n", message);
  longjmp(error_manager->jump_buffer, 1);
}

static void DestroyJpegLoader(JpegLoader *loader) {
  if (loader == NULL) {
    return;
  }

  if (loader->decoder_created) {
    jpeg_destroy_decompress(&loader->decoder);
  }

  if (loader->file != NULL) {
    fclose(loader->file);
  }

  if (loader->pixels != NULL) {
    MemFree(loader->pixels);
  }

  free(loader);
}

Image LoadJpegImage(const char *file_path) {
  Image image = {0};
  JpegLoader *loader = NULL;
  size_t width;
  size_t height;
  size_t channels;
  size_t row_stride;
  size_t byte_count;

  if (file_path == NULL) {
    return image;
  }

  loader = calloc(1, sizeof(*loader));
  if (loader == NULL) {
    fprintf(stderr, "Warning: out of memory creating JPEG decoder\n");
    return image;
  }

  loader->file = fopen(file_path, "rb");
  if (loader->file == NULL) {
    perror("Warning: failed to open JPEG");
    DestroyJpegLoader(loader);
    return image;
  }

  loader->decoder.err = jpeg_std_error(&loader->error_manager.base);
  loader->error_manager.base.error_exit = JpegErrorExit;

  if (setjmp(loader->error_manager.jump_buffer)) {
    fprintf(stderr, "Warning: failed to load JPEG: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  jpeg_create_decompress(&loader->decoder);
  loader->decoder_created = true;
  jpeg_stdio_src(&loader->decoder, loader->file);

  if (jpeg_read_header(&loader->decoder, TRUE) != JPEG_HEADER_OK) {
    fprintf(stderr, "Warning: invalid JPEG header: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  loader->decoder.out_color_space = JCS_RGB;
  if (!jpeg_start_decompress(&loader->decoder)) {
    fprintf(stderr, "Warning: failed to start JPEG decoder: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  width = loader->decoder.output_width;
  height = loader->decoder.output_height;
  channels = loader->decoder.output_components;

  if (width == 0 || height == 0 || channels != 3 || width > INT_MAX ||
      height > INT_MAX || width > SIZE_MAX / channels) {
    fprintf(stderr, "Warning: unsupported JPEG dimensions: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  row_stride = width * channels;
  if (height > SIZE_MAX / row_stride || height * row_stride > UINT_MAX) {
    fprintf(stderr, "Warning: JPEG is too large to load: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  byte_count = height * row_stride;
  loader->pixels = MemAlloc((unsigned int)byte_count);
  if (loader->pixels == NULL) {
    fprintf(stderr, "Warning: out of memory loading JPEG: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  while (loader->decoder.output_scanline < loader->decoder.output_height) {
    JSAMPROW row =
        loader->pixels + (size_t)loader->decoder.output_scanline * row_stride;
    if (jpeg_read_scanlines(&loader->decoder, &row, 1) != 1) {
      fprintf(stderr, "Warning: failed to read JPEG pixels: %s\n", file_path);
      DestroyJpegLoader(loader);
      return image;
    }
  }

  if (!jpeg_finish_decompress(&loader->decoder)) {
    fprintf(stderr, "Warning: failed to finish JPEG decoder: %s\n", file_path);
    DestroyJpegLoader(loader);
    return image;
  }

  image.data = loader->pixels;
  image.width = (int)width;
  image.height = (int)height;
  image.mipmaps = 1;
  image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
  loader->pixels = NULL;

  DestroyJpegLoader(loader);
  return image;
}
