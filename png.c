/**
 * @file png.c
 *
 * Copyright (c) 2015 大前良介 (OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief PNGファイルの読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介 (OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>
#include "image.h"

/**
 * @brief PNG形式のファイルを読み込む。
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_png_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_png_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief PNG形式のファイルを読み込む。
 *
 * @param[in] fp ファイルストリーム
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_png_stream(FILE *fp) {
  image_t *img = NULL;
  int i, x, y;
  int width, height;
  int num;
  png_colorp palette;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytep row;
  png_bytepp rows;
  png_byte sig_bytes[8];
  if (fread(sig_bytes, sizeof(sig_bytes), 1, fp) != 1) {
    return NULL;
  }
  if (png_sig_cmp(sig_bytes, 0, sizeof(sig_bytes))) {
    return NULL;
  }
  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    goto error;
  }
  info = png_create_info_struct(png);
  if (info == NULL) {
    goto error;
  }
  if (setjmp(png_jmpbuf(png))) {
    goto error;
  }
  png_init_io(png, fp);
  png_set_sig_bytes(png, sizeof(sig_bytes));
  png_read_png(png, info, PNG_TRANSFORM_PACKING | PNG_TRANSFORM_STRIP_16, NULL);
  width = png_get_image_width(png, info);
  height = png_get_image_height(png, info);
  rows = png_get_rows(png, info);
  // 画像形式に応じて詰め込み
  switch (png_get_color_type(png, info)) {
    case PNG_COLOR_TYPE_PALETTE:  // インデックスカラー
      if ((img = allocate_image(width, height, COLOR_TYPE_INDEX)) == NULL) {
        goto error;
      }
      png_get_PLTE(png, info, &palette, &num);
      img->palette_num = num;
      for (i = 0; i < num; i++) {
        png_color pc = palette[i];
        img->palette[i] = color_from_rgb(pc.red, pc.green, pc.blue);
      }
      {
        png_bytep trans = NULL;
        int num_trans = 0;
        if (png_get_tRNS(png, info, &trans, &num_trans, NULL) == PNG_INFO_tRNS
            && trans != NULL && num_trans > 0) {
          for (i = 0; i < num_trans; i++) {
            img->palette[i].a = trans[i];
          }
          for (; i < num; i++) {
            img->palette[i].a = 0xff;
          }
        }
      }
      for (y = 0; y < height; y++) {
        row = rows[y];
        for (x = 0; x < width; x++) {
          img->map[y][x].i = *row++;
        }
      }
      break;
    case PNG_COLOR_TYPE_GRAY:  // グレースケール
      if ((img = allocate_image(width, height, COLOR_TYPE_GRAY)) == NULL) {
        goto error;
      }
      for (y = 0; y < height; y++) {
        row = rows[y];
        for (x = 0; x < width; x++) {
          img->map[y][x].g = *row++;
        }
      }
      break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:  // グレースケール+α
      if ((img = allocate_image(width, height, COLOR_TYPE_RGBA)) == NULL) {
        goto error;
      }
      for (y = 0; y < height; y++) {
        row = rows[y];
        for (x = 0; x < width; x++) {
          uint8_t g = *row++;
          img->map[y][x].c.r = g;
          img->map[y][x].c.g = g;
          img->map[y][x].c.b = g;
          img->map[y][x].c.a = *row++;
        }
      }
      break;
    case PNG_COLOR_TYPE_RGB:  // RGB
      if ((img = allocate_image(width, height, COLOR_TYPE_RGB)) == NULL) {
        goto error;
      }
      for (y = 0; y < height; y++) {
        row = rows[y];
        for (x = 0; x < width; x++) {
          img->map[y][x].c.r = *row++;
          img->map[y][x].c.g = *row++;
          img->map[y][x].c.b = *row++;
          img->map[y][x].c.a = 0xff;
        }
      }
      break;
    case PNG_COLOR_TYPE_RGB_ALPHA:  // RGBA
      if ((img = allocate_image(width, height, COLOR_TYPE_RGBA)) == NULL) {
        goto error;
      }
      for (y = 0; y < height; y++) {
        row = rows[y];
        for (x = 0; x < width; x++) {
          img->map[y][x].c.r = *row++;
          img->map[y][x].c.g = *row++;
          img->map[y][x].c.b = *row++;
          img->map[y][x].c.a = *row++;
        }
      }
      break;
  }
  error:
  png_destroy_read_struct(&png, &info, NULL);
  return img;
}

/**
 * @brief PNG形式としてファイルに書き出す。
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @return 成否
 */
result_t write_png_file(const char *filename, image_t *img) {
  result_t result = FAILURE;
  if (img == NULL) {
    return result;
  }
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror(filename);
    return result;
  }
  result = write_png_stream(fp, img);
  fclose(fp);
  return result;
}

/**
 * @brief PNG形式としてファイルに書き出す。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
result_t write_png_stream(FILE *fp, image_t *img) {
  int i, x, y;
  result_t result = FAILURE;
  int row_size;
  int color_type;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytep row;
  png_bytepp rows = NULL;
  png_colorp palette = NULL;
  if (img == NULL) {
    return result;
  }
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:  // インデックスカラー
      color_type = PNG_COLOR_TYPE_PALETTE;
      row_size = sizeof(png_byte) * img->width;
      break;
    case COLOR_TYPE_GRAY:  // グレースケール
      color_type = PNG_COLOR_TYPE_GRAY;
      row_size = sizeof(png_byte) * img->width;
      break;
    case COLOR_TYPE_RGB:  // RGB
      color_type = PNG_COLOR_TYPE_RGB;
      row_size = sizeof(png_byte) * img->width * 3;
      break;
    case COLOR_TYPE_RGBA:  // RGBA
      color_type = PNG_COLOR_TYPE_RGBA;
      row_size = sizeof(png_byte) * img->width * 4;
      break;
    default:
      return FAILURE;
  }
  png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL) {
    goto error;
  }
  info = png_create_info_struct(png);
  if (info == NULL) {
    goto error;
  }
  if (setjmp(png_jmpbuf(png))) {
    goto error;
  }
  png_init_io(png, fp);
  png_set_IHDR(png, info, img->width, img->height, 8,
      color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
      PNG_FILTER_TYPE_DEFAULT);
  rows = png_malloc(png, sizeof(png_bytep) * img->height);
  if (rows == NULL) {
    goto error;
  }
  png_set_rows(png, info, rows);
  memset(rows, 0, sizeof(png_bytep) * img->height);
  for (y = 0; y < img->height; y++) {
    if ((rows[y] = png_malloc(png, row_size)) == NULL) {
      goto error;
    }
  }
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:  // インデックスカラー
      palette = png_malloc(png, sizeof(png_color) * img->palette_num);
      for (i = 0; i < img->palette_num; i++) {
        palette[i].red = img->palette[i].r;
        palette[i].green = img->palette[i].g;
        palette[i].blue = img->palette[i].b;
      }
      png_set_PLTE(png, info, palette, img->palette_num);
      for (i = img->palette_num - 1; i >= 0 && img->palette[i].a != 0xff; i--);
      if (i >= 0) {
        int num_trans = i + 1;
        png_byte trans[255];
        for (i = 0; i < num_trans; i++) {
          trans[i] = img->palette[i].a;
        }
        png_set_tRNS(png, info, trans, num_trans, NULL);
      }
      png_free(png, palette);
      for (y = 0; y < img->height; y++) {
        row = rows[y];
        for (x = 0; x < img->width; x++) {
          *row++ = img->map[y][x].i;
        }
      }
      break;
    case COLOR_TYPE_GRAY:  // グレースケール
      for (y = 0; y < img->height; y++) {
        row = rows[y];
        for (x = 0; x < img->width; x++) {
          *row++ = img->map[y][x].g;
        }
      }
      break;
    case COLOR_TYPE_RGB:  // RGB
      for (y = 0; y < img->height; y++) {
        row = rows[y];
        for (x = 0; x < img->width; x++) {
          *row++ = img->map[y][x].c.r;
          *row++ = img->map[y][x].c.g;
          *row++ = img->map[y][x].c.b;
        }
      }
      break;
    case COLOR_TYPE_RGBA:  // RGBA
      for (y = 0; y < img->height; y++) {
        row = rows[y];
        for (x = 0; x < img->width; x++) {
          *row++ = img->map[y][x].c.r;
          *row++ = img->map[y][x].c.g;
          *row++ = img->map[y][x].c.b;
          *row++ = img->map[y][x].c.a;
        }
      }
      break;
  }
  png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);
  result = SUCCESS;
  error:
  if (rows != NULL) {
    for (y = 0; y < img->height; y++) {
      png_free(png, rows[y]);
    }
    png_free(png, rows);
  }
  png_destroy_write_struct(&png, &info);
  return result;
}
