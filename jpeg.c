/**
 * @file jpeg.c
 *
 * Copyright(c) 2015 大前良介(OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief JPEGファイルの読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介(OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <jpeglib.h>
#include "image.h"

/**
 * @brief JPEG形式のファイルを読み込む。
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_jpeg_file(const char *filename) {
  FILE *fp;
  if ((fp = fopen(filename, "rb")) == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_jpeg_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief JPEG形式のファイルを読み込む。
 *
 * @param[in] fp ファイルストリーム
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_jpeg_stream(FILE *fp) {
  uint32_t i, x, y;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  image_t *img = NULL;
  JSAMPARRAY rows = NULL;
  int row_size;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    goto error;
  }
  jpeg_start_decompress(&cinfo);
  if ((rows = calloc(sizeof(JSAMPROW), cinfo.output_height)) == NULL) {
    goto error;
  }
  row_size = sizeof(JSAMPLE) * cinfo.output_width * cinfo.output_components;
  for (i = 0; i < cinfo.output_height; ++i) {
    if ((rows[i] = malloc(row_size)) == NULL) {
      goto error;
    }
  }
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, rows + cinfo.output_scanline,
        cinfo.output_height - cinfo.output_scanline);
  }
  jpeg_finish_decompress(&cinfo);
  if ((img = allocate_image(cinfo.output_width, cinfo.output_height,
      COLOR_TYPE_RGB)) == NULL) {
    goto error;
  }
  for (y = 0; y < cinfo.output_height; y++) {
    for (x = 0; x < cinfo.output_width; x++) {
      img->map[y][x].c.r = rows[y][x * 3 + 0];
      img->map[y][x].c.g = rows[y][x * 3 + 1];
      img->map[y][x].c.b = rows[y][x * 3 + 2];
      img->map[y][x].c.a = 0xff;
    }
    free(rows[y]);
  }
  free(rows);
  jpeg_destroy_decompress(&cinfo);
  return img;
  error:
  if (rows != NULL) {
    for (y = 0; y < cinfo.output_height; y++) {
      free(rows[y]);
    }
    free(rows);
  }
  free_image(img);
  return NULL;
}

/**
 * @brief JPEG形式としてファイルに書き出す。
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @return 成否
 */
result_t write_jpeg_file(const char *filename, image_t *img) {
  result_t result = FAILURE;
  FILE *fp;
  if (img == NULL) {
    return result;
  }
  if ((fp = fopen(filename, "wb")) == NULL) {
    perror(filename);
    return result;
  }
  result = write_jpeg_stream(fp, img);
  fclose(fp);
  return result;
}

/**
 * @brief JPEG形式としてファイルに書き出す。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
result_t write_jpeg_stream(FILE *fp, image_t *img) {
  int x, y;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  image_t *to_free = NULL;
  JSAMPROW row = NULL;
  if (img == NULL) {
    return FAILURE;
  }
  if ((row = malloc(sizeof(JSAMPLE) * 3 * img->width)) == NULL) {
    return FAILURE;
  }
  if (img->color_type != COLOR_TYPE_RGB) {
    // 画像形式がRGBでない場合はRGBに変換して出力
    to_free = clone_image(img);
    img = image_to_rgb(to_free);
  }
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, fp);
  cinfo.image_width = img->width;
  cinfo.image_height = img->height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 75, TRUE);
  jpeg_start_compress(&cinfo, TRUE);
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      row[x * 3 + 0] = img->map[y][x].c.r;
      row[x * 3 + 1] = img->map[y][x].c.g;
      row[x * 3 + 2] = img->map[y][x].c.b;
    }
    jpeg_write_scanlines(&cinfo, &row, 1);
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  free(row);
  free_image(to_free);
  return SUCCESS;
}
