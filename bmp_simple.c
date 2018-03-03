/**
 * @file bmp_simple.c
 *
 * Copyright (c) 2015 大前良介 (OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief BMPファイルの簡易読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介 (OHMAE Ryosuke)</a>
 * @date 2015/03/01
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "image.h"

#define FILE_TYPE 0x4D42    /**< "BM"をリトルエンディアンで解釈した値 */
#define FILE_HEADER_SIZE 14 /**< BMPファイルヘッダサイズ */
#define INFO_HEADER_SIZE 40 /**< Windowsヘッダサイズ */
#define DEFAULT_HEADER_SIZE (FILE_HEADER_SIZE + INFO_HEADER_SIZE) /**< 標準のヘッダサイズ */

/**
 * @brief BMPファイルヘッダ
 *
 * メモリマップとして利用するには
 * pragmaが必要
 */
#pragma pack(2)
typedef struct BITMAPFILEHEADER {
  uint16_t bfType;      /**< ファイルタイプ、必ず"BM" */
  uint32_t bfSize;      /**< ファイルサイズ */
  uint16_t bfReserved1; /**< リザーブ */
  uint16_t bfReserved2; /**< リサーブ */
  uint32_t bfOffBits;   /**< 先頭から画像情報までのオフセット、ヘッダ構造体＋パレットサイズ */
} BITMAPFILEHEADER;
#pragma pack()

/**
 * @brief 画像情報ヘッダ
 */
typedef struct BITMAPINFOHEADER {
  uint32_t biSize;         /**< この構造体のサイズ */
  int32_t biWidth;         /**< 画像の幅 */
  int32_t biHeight;        /**< 画像の高さ */
  uint16_t biPlanes;       /**< 画像の枚数、通常1 */
  uint16_t biBitCount;     /**< 一色のビット数 */
  uint32_t biCompression;  /**< 圧縮形式 */
  uint32_t biSizeImage;    /**< 画像領域のサイズ */
  int32_t biXPelsPerMeter; /**< 画像の横方向解像度情報 */
  int32_t biYPelsPerMeter; /**< 画像の縦方向解像度情報*/
  uint32_t biClrUsed;      /**< カラーパレットのうち実際に使っている色の個数 */
  uint32_t biClrImportant; /**< カラーパレットのうち重要な色の数 */
} BITMAPINFOHEADER;

/**
 * @brief BMP形式のファイルを読み込む。
 *
 * 24bitRGB形式にのみ対応、それ以外の形式は読み込み失敗扱い
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_bmp_simple_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_bmp_simple_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief BMP形式のファイルを読み込む。
 *
 * 24bitRGB形式にのみ対応、それ以外の形式は読み込み失敗扱い
 *
 * @param[in] fp 読み込むファイルストリームのポインタ
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_bmp_simple_stream(FILE *fp) {
  uint8_t header_buffer[DEFAULT_HEADER_SIZE];
  BITMAPFILEHEADER *file = (BITMAPFILEHEADER*)header_buffer;
  BITMAPINFOHEADER *info = (BITMAPINFOHEADER*)(header_buffer + FILE_HEADER_SIZE);
  uint8_t *row, *buffer;
  image_t *img = NULL;
  int x, y;
  int width;
  int height;
  int stride;
  if (fread(header_buffer, DEFAULT_HEADER_SIZE, 1, fp) != 1) {
    return NULL;
  }
  if (file->bfOffBits != DEFAULT_HEADER_SIZE ||
      info->biBitCount != 24 ||
      info->biHeight <= 0) {
    return NULL;
  }
  width = info->biWidth;
  height = info->biHeight;
  stride = (width * 3 + 3) / 4 * 4;
  if ((buffer = malloc(stride)) == NULL) {
    return NULL;
  }
  if ((img = allocate_image(width, height, COLOR_TYPE_RGB)) == NULL) {
    goto error;
  }
  for (y = height - 1; y >= 0; y--) {
    if (fread(buffer, stride, 1, fp) != 1) {
      goto error;
    }
    row = buffer;
    for (x = 0; x < width; x++) {
      img->map[y][x].c.b = *row++;
      img->map[y][x].c.g = *row++;
      img->map[y][x].c.r = *row++;
      img->map[y][x].c.a = 0xff;
    }
  }
  free(buffer);
  return img;
  error:
  free(buffer);
  free_image(img);
  return NULL;
}

/**
 * @brief BMP形式としてファイルに書き出す。
 *
 * COLOR_TYPE_RGBの場合にのみ
 * Windows形式での出力を行う
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @return 成否
 */
result_t write_bmp_simple_file(const char *filename, image_t *img) {
  result_t result = FAILURE;
  if (img == NULL) {
    return result;
  }
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror(filename);
    return result;
  }
  result = write_bmp_simple_stream(fp, img);
  fclose(fp);
  return result;
}

/**
 * @brief BMP形式としてファイルに書き出す。
 *
 * COLOR_TYPE_RGBの場合にのみ
 * Windows形式での出力を行う
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
result_t write_bmp_simple_stream(FILE *fp, image_t *img) {
  uint8_t header_buffer[DEFAULT_HEADER_SIZE];
  BITMAPFILEHEADER *file = (BITMAPFILEHEADER*)header_buffer;
  BITMAPINFOHEADER *info = (BITMAPINFOHEADER*)(header_buffer + FILE_HEADER_SIZE);
  int x, y;
  int stride;
  uint8_t *row, *buffer;
  if (img->color_type != COLOR_TYPE_RGB) {
    return FAILURE;
  }
  stride = (img->width * 3 + 3) / 4 * 4;
  if ((buffer = malloc(stride)) == NULL) {
    return FAILURE;
  }
  file->bfType = FILE_TYPE;
  file->bfSize = DEFAULT_HEADER_SIZE + stride * img->height;
  file->bfReserved1 = 0;
  file->bfReserved2 = 0;
  file->bfOffBits = DEFAULT_HEADER_SIZE;
  info->biSize = INFO_HEADER_SIZE;
  info->biWidth = img->width;
  info->biHeight = img->height;
  info->biPlanes = 1;
  info->biBitCount = 24;
  info->biCompression = 0;
  info->biSizeImage = stride * img->height;
  info->biXPelsPerMeter = 0;
  info->biYPelsPerMeter = 0;
  info->biClrUsed = 0;
  info->biClrImportant = 0;
  if (fwrite(header_buffer, DEFAULT_HEADER_SIZE, 1, fp) != 1) {
    goto error;
  }
  memset(buffer, 0, stride);
  for (y = img->height - 1; y >= 0; y--) {
    row = buffer;
    for (x = 0; x < img->width; x++) {
      *row++ = img->map[y][x].c.b;
      *row++ = img->map[y][x].c.g;
      *row++ = img->map[y][x].c.r;
    }
    if (fwrite(buffer, stride, 1, fp) != 1) {
      goto error;
    }
  }
  free(buffer);
  return SUCCESS;
  error:
  free(buffer);
  return FAILURE;
}
