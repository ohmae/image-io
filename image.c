/**
 * @file image.c
 *
 * Copyright (c) 2015 大前良介 (OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief 画像データの共通処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介 (OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "image.h"

/**
 * @brief 画像情報のダンプを行う。
 *
 * デバッグ用
 *
 * @param[in] img  ダンプする画像
 */
void dump_image_info(image_t *img) {
  fprintf(stderr, "width:  %u\n", img->width);
  fprintf(stderr, "height: %u\n", img->height);
  fprintf(stderr, "type:   %d\n", img->color_type);
  fprintf(stderr, "pnum:   %d\n", img->palette_num);
  fprintf(stderr, "palette:%p\n", img->palette);
#if 0
  if (img->palette_num != 0) {
    int i;
    for (i = 0; i < img->palette_num; i++) {
      color_t *p = &img->palette[i];
      fprintf(stderr, "%3d: r:%02X g:%02X b:%02X\n", i, p->r, p->g, p->b);
    }
  }
#endif
}

/**
 * @brief image_t型構造体のメモリを確保し初期化する。
 *
 * @param[in] width   画像の幅
 * @param[in] height  画像の高さ
 * @param[in] type    色表現の種別
 * @return 初期化済みimage_t型構造体
 */
image_t *allocate_image(uint32_t width, uint32_t height, uint8_t type) {
  uint32_t i;
  image_t *img;
  if ((img = calloc(1, sizeof(image_t))) == NULL) {
    return NULL;
  }
  img->width = width;
  img->height = height;
  img->color_type = type;
  if (type == COLOR_TYPE_INDEX) {
    if ((img->palette = calloc(256, sizeof(color_t))) == NULL) {
      goto error;
    }
  } else {
    img->palette = NULL;
  }
  img->palette_num = 0;
  if ((img->map = calloc(height, sizeof(pixcel_t*))) == NULL) {
    goto error;
  }
  for (i = 0; i < height; i++) {
    if ((img->map[i] = calloc(width, sizeof(pixcel_t))) == NULL) {
      goto error;
    }
  }
  return img;
  error:
  free_image(img);
  return NULL;
}

/**
 * @brief image_t型のクローンを作成する。
 *
 * カラーパレットやイメージデータは内部的に別にメモリを確保しているため、
 * allocateした後にmemcpyしてもクローンは作成できない。
 * この関数を使ってdeepcopyを行うこと。
 *
 * @param[in] img クローン元image_t型構造体
 * @return クローンされたimage_t型構造体
 */
image_t *clone_image(image_t *img) {
  uint32_t i;
  image_t *new_img = allocate_image(img->width, img->height, img->color_type);
  if (new_img == NULL) {
    return NULL;
  }
  new_img->palette_num = img->palette_num;
  if (img->color_type == COLOR_TYPE_INDEX) {
    memcpy(new_img->palette, img->palette, sizeof(color_t) * img->palette_num);
  }
  for (i = 0; i < img->height; i++) {
    memcpy(new_img->map[i], img->map[i], sizeof(pixcel_t) * img->width);
  }
  return new_img;
}

/**
 * @brief image_t型構造体のメモリを開放する。
 *
 * 内部的に確保したメモリも開放する。
 * 内部メンバーのポインタを直接変更した場合
 * 正常に動作しないため注意。
 *
 * @param[in,out] img 開放するimage_t型構造体
 */
void free_image(image_t *img) {
  uint32_t i;
  if (img == NULL) {
    return;
  }
  if (img->palette != NULL) {
    free(img->palette);
  }
  for (i = 0; i < img->height; i++) {
    free(img->map[i]);
  }
  free(img->map);
  free(img);
}

/**
 * @brief RGB値を指定してcolor_t型の値を作成する。
 *
 * アルファ値は0xffが設定される。
 *
 * @param[in] r red
 * @param[in] g green
 * @param[in] b blue
 * @return color_t
 */
color_t color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
  color_t c;
  c.r = r;
  c.g = g;
  c.b = b;
  c.a = 0xff;
  return c;
}

/**
 * @brief RGBA値を指定してcolor_t型の値を作成する。
 *
 * @param[in] r red
 * @param[in] g green
 * @param[in] b blue
 * @param[in] a alpha
 * @return color_t
 */
color_t color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  color_t c;
  c.r = r;
  c.g = g;
  c.b = b;
  c.a = a;
  return c;
}

/**
 * @brief 引数の画像をインデックスカラー方式に変換する。
 *
 * 減色済みの画像でなければ失敗しNULLが返る。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_to_index(image_t *img) {
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:
      break;
    case COLOR_TYPE_GRAY:
      img = image_gray_to_index(img);
      break;
    case COLOR_TYPE_RGB:
      img = image_rgb_to_index(img);
      break;
    case COLOR_TYPE_RGBA:
      img = image_rgba_to_rgb(img, color_from_rgb(255, 255, 255));
      img = image_rgb_to_index(img);
      break;
  }
  return img;
}

/**
 * @brief 引数の画像をRGB方式に変換する。
 *
 * RGBA形式の場合は、白背景に変換する。
 * 背景色を指定したい場合は
 * image_rgba_to_rgb()
 * を利用すること。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_to_gray(image_t *img) {
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:
      img = image_index_to_rgb(img);
      img = image_rgb_to_gray(img);
      break;
    case COLOR_TYPE_GRAY:
      break;
    case COLOR_TYPE_RGB:
      img = image_rgb_to_gray(img);
      break;
    case COLOR_TYPE_RGBA:
      img = image_rgba_to_rgb(img, color_from_rgb(255, 255, 255));
      img = image_rgb_to_gray(img);
      break;
  }
  return img;
}

/**
 * @brief 引数の画像をRGB方式に変換する。
 *
 * RGBA形式の場合は、白背景に変換する。
 * 背景色を指定したい場合は
 * image_rgba_to_rgb()
 * を利用すること。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_to_rgb(image_t *img) {
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:
      img = image_index_to_rgb(img);
      break;
    case COLOR_TYPE_GRAY:
      img = image_gray_to_rgb(img);
      break;
    case COLOR_TYPE_RGB:
      break;
    case COLOR_TYPE_RGBA:
      img = image_rgba_to_rgb(img, color_from_rgb(255, 255, 255));
      break;
  }
  return img;
}

/**
 * @brief 引数の画像をRGBA方式に変換する。
 *
 * RGB形式との違いはAlpha値を見るか、否かなので
 * RGB形式に変換した後color_typeをRGBAに変換する。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_to_rgba(image_t *img) {
  switch (img->color_type) {
    case COLOR_TYPE_INDEX:
      img = image_index_to_rgb(img);
      img->color_type = COLOR_TYPE_RGBA;
      break;
    case COLOR_TYPE_GRAY:
      img = image_gray_to_rgb(img);
      img->color_type = COLOR_TYPE_RGBA;
      break;
    case COLOR_TYPE_RGB:
      img->color_type = COLOR_TYPE_RGBA;
      break;
    case COLOR_TYPE_RGBA:
      break;
  }
  return img;
}

/**
 * @brief インデックスカラー方式からRGB方式へ変換する。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_index_to_rgb(image_t *img) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_INDEX) {
    return NULL;
  }
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      if (p->i >= img->palette_num) {
        return NULL;
      }
      p->c = img->palette[p->i];
    }
  }
  img->color_type = COLOR_TYPE_RGB;
  free(img->palette);
  img->palette = NULL;
  img->palette_num = 0;
  return img;
}

/**
 * @brief RGB方式からインデックスカラー方式に変換する。
 *
 * 色数のカウントを行い、256色以上使用されている場合は失敗する。
 * 予め減色済みの画像を渡すこと。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_rgb_to_index(image_t *img) {
  uint32_t i, x, y;
  int num = 0;
  color_t *palette;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_RGB) {
    return NULL;
  }
  // 色数をカウントするとともにカラーパレットを作成
  palette = calloc(256, sizeof(color_t));
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      color_t *c = &img->map[y][x].c;
      for (i = 0; i < num; i++) {
        if (memcmp(c, &palette[i], sizeof(color_t)) == 0) {
          break;
        }
      }
      if (i == num) {
        // パレットにない色
        if (num == 256) {
          // 色数が256色以上あるとパレット形式にはできない
          free(palette);
          return NULL;
        }
        // パレットに追加
        palette[i] = *c;
        num++;
      }
    }
  }
  // カラーパレットが作成できたので、
  // 各ピクセルをカラーパレットのインデックスに置換
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      color_t *c = &p->c;
      for (i = 0; i < num; i++) {
        if (memcmp(c, &palette[i], sizeof(color_t)) == 0) {
          break;
        }
      }
      memset(p, 0, sizeof(pixcel_t));
      p->i = i;
    }
  }
  img->color_type = COLOR_TYPE_INDEX;
  img->palette_num = num;
  img->palette = palette;
  return img;
}

/**
 * @brief グレースケール方式からインデックスカラー方式に変換する。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_gray_to_index(image_t *img) {
  uint32_t i, x, y;
  color_t *palette;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_GRAY) {
    return NULL;
  }
  // グレイスケールの値がそのままインデックス値になるようにカラーパレットを作成
  palette = calloc(256, sizeof(color_t));
  for (i = 0; i < 256; i++) {
    palette[i].r = i;
    palette[i].g = i;
    palette[i].b = i;
    palette[i].a = 0xff;
  }
  // カラーパレットが作成できたので、
  // 各ピクセルをカラーパレットのインデックスに置換
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      uint8_t g = p->g;
      memset(p, 0, sizeof(pixcel_t));
      p->i = g;
    }
  }
  img->color_type = COLOR_TYPE_INDEX;
  img->palette_num = 256;
  img->palette = palette;
  return img;
}

/**
 * @brief RGBA方式からRGB方式に変換する。
 *
 * 指定した背景色にアルファブレンドした結果をRGB値に変換する。
 * 背景色のアルファ値は無視され、0xffのアルファ値を持つものとして扱われる。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @param[in] bg アルファブレンドを行う背景色
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_rgba_to_rgb(image_t *img, color_t bg) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_RGBA) {
    return NULL;
  }
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      const uint8_t a = p->c.a;
      p->c.r = (p->c.r * a + bg.r * (0xff - a) + 0x7f) / 0xff;
      p->c.g = (p->c.g * a + bg.g * (0xff - a) + 0x7f) / 0xff;
      p->c.b = (p->c.b * a + bg.b * (0xff - a) + 0x7f) / 0xff;
      p->c.a = 0xff;
    }
  }
  img->color_type = COLOR_TYPE_RGB;
  return img;
}

/**
 * @brief RGBA方式からRGB方式に変換する。
 *
 * アルファブレンドは行わず、
 * 単にアルファ値を無視して不透明化することでRGBへ変換する。
 * アルファブレンドを行わないため不自然な変換となる。
 * 実験用に作成。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_rgba_to_rgb_ignore_alpha(image_t *img) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_RGBA) {
    return NULL;
  }
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      img->map[y][x].c.a = 0xff;
    }
  }
  img->color_type = COLOR_TYPE_RGB;
  return img;
}

/**
 * @brief グレースケール方式からRGB方式に変換する。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_gray_to_rgb(image_t *img) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_GRAY) {
    return NULL;
  }
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      const uint8_t g = p->g;
      p->c.r = g;
      p->c.g = g;
      p->c.b = g;
      p->c.a = 0xff;
    }
  }
  img->color_type = COLOR_TYPE_RGB;
  return img;
}

/**
 * @brief RGB方式からグレースケール方式に変換する。
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_rgb_to_gray(image_t *img) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_RGB) {
    return NULL;
  }
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      const uint8_t r = p->c.r;
      const uint8_t g = p->c.g;
      const uint8_t b = p->c.b;
      // ITU-R BT.601規定の輝度計算で変換する
      const uint8_t gray = (uint8_t) (0.299f * r + 0.587f * g + 0.114f * b + 0.5f);
      memset(p, 0, sizeof(pixcel_t));
      p->g = gray;
    }
  }
  img->color_type = COLOR_TYPE_GRAY;
  return img;
}

/**
 * @brief グレースケール方式から2値白黒方式に変換する。
 *
 * pbm形式出力用
 *
 * 指定引数のimage_t型の内部表現を書き換えて戻すため、
 * 引数に指定したポインタと、成功時の戻り値は同じ値となる。
 * 元のimage_t型は保持されないため、
 * 必要があれば予めクローンを作成しておく。
 *
 * @param[in,out] img 変換するimage_t型へのポインタ
 * @return 変換に成功した場合、引数に指定されたポインタ、失敗した場合NULLが返る。
 */
image_t *image_gray_to_binary(image_t *img) {
  uint32_t x, y;
  if (img == NULL) {
    return NULL;
  }
  if (img->color_type != COLOR_TYPE_GRAY) {
    return NULL;
  }
  img->palette_num = 2;
  img->palette = calloc(256, sizeof(color_t));
  img->palette[0] = color_from_rgb(255, 255, 255);
  img->palette[1] = color_from_rgb(0, 0, 0);
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      pixcel_t *p = &img->map[y][x];
      p->i = (p->g < 128 ? 1 : 0);
    }
  }
  img->color_type = COLOR_TYPE_INDEX;
  return img;
}
