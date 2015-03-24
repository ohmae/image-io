/**
 * @file pnm.c
 *
 * Copyright(c) 2015 大前良介(OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief PNM(PPM/PGM/PBM)ファイルの読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介(OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include "image.h"

/**
 * @brief 2値の最小値を返すマクロ
 */
#define MIN(x, y) ((x) < (y) ? (x) : (y))

static uint8_t normalize(int value, int max);
static int get_next_non_space_char(FILE *fp);
static int get_next_token(FILE *fp, char *buf, size_t size);
static int parse_int(const char *str);
static int get_next_int(FILE *fp);

static result_t read_p1(FILE *fp, image_t *img);
static result_t read_p2(FILE *fp, image_t *img, int max);
static result_t read_p3(FILE *fp, image_t *img, int max);
static result_t read_p4(FILE *fp, image_t *img);
static result_t read_p5(FILE *fp, image_t *img, int max);
static result_t read_p6(FILE *fp, image_t *img, int max);

static result_t write_p1(FILE *fp, image_t *img);
static result_t write_p2(FILE *fp, image_t *img);
static result_t write_p3(FILE *fp, image_t *img);
static result_t write_p4(FILE *fp, image_t *img);
static result_t write_p5(FILE *fp, image_t *img);
static result_t write_p6(FILE *fp, image_t *img);

/**
 * @brief [0,max]の値を[0,255]の範囲に正規化する
 */
static uint8_t normalize(int value, int max) {
  // valueがmaxを超える場合はmaxとする
  return (MIN(value, max) * 255 + max / 2) / max;
}

/**
 * @brief 次のトークンをintにパースしたものを返す。
 *
 * 次のトークンがない、
 * トークンに数字以外が含まれる場合
 *
 * @return 次の整数値、エラー時0
 */
static int get_next_int(FILE *fp) {
  char token[11];
  get_next_token(fp, token, sizeof(token));
  return parse_int(token);
}

/**
 * @brief atoiのエラー判定を厳しくした処理。
 *
 * 空の場合、数字以外の文字が含まれる場合に
 * エラーとして-1を返す。
 *
 * @param[in] str 読み込む文字列
 * return パース結果
 */
static int parse_int(const char *str) {
  int i;
  int r = 0;
  if (str[0] == 0) {
    return -1;
  }
  for (i = 0; str[i] != 0; i++) {
    if (!isdigit((int)str[i])) {
      return -1;
    }
    r *= 10;
    r += str[i] - '0';
  }
  return r;
}

/**
 * @brief 空白で区切られた次のトークンを返す。
 *
 * 空白とコメントを読み飛ばす。
 * トークンの末尾の空白は読み込み済みとなる。
 *
 * @param[in]     fp   ファイルストリーム
 * @param[in,out] buf  トークン格納先
 * @param[in]     size トークン格納先のサイズ
 * @return トークンのサイズ、エラー時0
 */
static int get_next_token(FILE *fp, char *buf, size_t size) {
  int i = 0;
  int c = get_next_non_space_char(fp);
  while (c != EOF && !isspace(c) && i < size - 1) {
    buf[i++] = c;
    c = getc(fp);
  }
  buf[i] = 0;
  return i;
}

/**
 * @brief 空白文字とコメントを読み飛ばした次の文字を返す。
 *
 * @param[in] fp ファイルストリーム
 * @return 次の文字、EOFに到達した場合はEOF
 */
static int get_next_non_space_char(FILE *fp) {
  int c;
  int comment = FALSE;
  while ((c = getc(fp)) != EOF) {
    if (comment) {
      if (c == '\n' || c == '\r') {
        comment = FALSE;
      }
      continue;
    }
    if (c == '#') {
      comment = TRUE;
      continue;
    }
    if (!isspace(c)) {
      break;
    }
  }
  return c;
}

/**
 * @brief PNM(PPM/PGM/PBM)形式のファイルを読み込む。
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_pnm_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_pnm_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief P1の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @return 成否
 */
static result_t read_p1(FILE *fp, image_t *img) {
  int x, y;
  int tmp;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      tmp = get_next_non_space_char(fp);
      if (tmp == '0') {
        img->map[y][x].i = 0;
      } else if (tmp == '1') {
        img->map[y][x].i = 1;
      } else {
        return FAILURE;
      }
    }
  }
  return SUCCESS;
}

/**
 * @brief P2の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @param[in]     max 輝度最大値
 * @return 成否
 */
static result_t read_p2(FILE *fp, image_t *img, int max) {
  int x, y;
  int tmp;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      if ((tmp = get_next_int(fp)) < 0) {
        return FAILURE;
      }
      img->map[y][x].g = normalize(tmp, max);
    }
  }
  return SUCCESS;
}

/**
 * @brief P3の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @param[in]     max 輝度最大値
 * @return 成否
 */
static result_t read_p3(FILE *fp, image_t *img, int max) {
  int x, y;
  int tmp;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      if ((tmp = get_next_int(fp)) < 0) {
        return FAILURE;
      }
      img->map[y][x].c.r = normalize(tmp, max);
      if ((tmp = get_next_int(fp)) < 0) {
        return FAILURE;
      }
      img->map[y][x].c.g = normalize(tmp, max);
      if ((tmp = get_next_int(fp)) < 0) {
        return FAILURE;
      }
      img->map[y][x].c.b = normalize(tmp, max);
      img->map[y][x].c.a = 0xff;
    }
  }
  return SUCCESS;
}

/**
 * @brief P4の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @return 成否
 */
static result_t read_p4(FILE *fp, image_t *img) {
  int x, y;
  uint8_t *row;
  int row_size;
  row_size = (img->width + 7) / 8;
  if ((row = malloc(row_size)) == NULL) {
    return FAILURE;
  }
  for (y = 0; y < img->height; y++) {
    int pos = 0;
    int shift = 8;
    if (fread(row, row_size, 1, fp) != 1) {
      free(row);
      return FAILURE;
    }
    for (x = 0; x < img->width; x++) {
      shift--;
      img->map[y][x].i = (row[pos] >> shift) & 1;
      if (shift == 0) {
        shift = 8;
        pos++;
      }
    }
  }
  free(row);
  return SUCCESS;
}

/**
 * @brief P5の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @param[in]     max 輝度最大値
 * @return 成否
 */
static result_t read_p5(FILE *fp, image_t *img, int max) {
  int x, y;
  int tmp;
  uint8_t *row;
  uint8_t *row_base;
  int row_size;
  int bpc = max > 255 ? 2 : 1;
  row_size = img->width * bpc;
  if ((row_base = malloc(row_size)) == NULL) {
    return FAILURE;
  }
  for (y = 0; y < img->height; y++) {
    if (fread(row_base, row_size, 1, fp) != 1) {
      free(row_base);
      return FAILURE;
    }
    row = row_base;
    if (bpc == 1) {
      for (x = 0; x < img->width; x++) {
        img->map[y][x].g = normalize(*row++, max);
      }
    } else {
      for (x = 0; x < img->width; x++) {
        tmp = *row++ << 8;
        tmp |= *row++;
        img->map[y][x].g = normalize(tmp, max);
      }
    }
  }
  free(row_base);
  return SUCCESS;
}

/**
 * @brief P6の画像データを読み込む。
 *
 * @param[in]     fp  ファイルストリーム
 * @param[in,out] img 画像構造体
 * @param[in]     max 輝度最大値
 * @return 成否
 */
static result_t read_p6(FILE *fp, image_t *img, int max) {
  int x, y;
  int tmp;
  uint8_t *row;
  uint8_t *row_base;
  int row_size;
  int bpc = max > 255 ? 2 : 1;
  row_size = img->width * 3 * bpc;
  if ((row_base = malloc(row_size)) == NULL) {
    return FAILURE;
  }
  for (y = 0; y < img->height; y++) {
    if (fread(row_base, row_size, 1, fp) != 1) {
      free(row_base);
      return FAILURE;
    }
    row = row_base;
    if (bpc == 1) {
      for (x = 0; x < img->width; x++) {
        img->map[y][x].c.r = normalize(*row++, max);
        img->map[y][x].c.g = normalize(*row++, max);
        img->map[y][x].c.b = normalize(*row++, max);
        img->map[y][x].c.a = 0xff;
      }
    } else {
      for (x = 0; x < img->width; x++) {
        tmp = *row++ << 8;
        tmp |= *row++;
        img->map[y][x].c.r = normalize(tmp, max);
        tmp = *row++ << 8;
        tmp |= *row++;
        img->map[y][x].c.g = normalize(tmp, max);
        tmp = *row++ << 8;
        tmp |= *row++;
        img->map[y][x].c.b = normalize(tmp, max);
        img->map[y][x].c.a = 0xff;
      }
    }
  }
  free(row_base);
  return SUCCESS;
}

/**
 * @brief PNM(PPM/PGM/PBM)形式のファイルを読み込む。
 *
 * @param[in] fp ファイルストリーム
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_pnm_stream(FILE *fp) {
  char token[4];
  int type;
  int width;
  int height;
  int max = 0;
  result_t result = FAILURE;
  image_t *img = NULL;
  get_next_token(fp, token, sizeof(token));
  type = token[1] - '0';
  if (token[0] != 'P' || type < 1 || type > 6 || token[2] != 0) {
    return NULL;
  }
  width = get_next_int(fp);
  height = get_next_int(fp);
  if (width <= 0 || height <= 0) {
    return NULL;
  }
  if (type != 1 && type != 4) {
    max = get_next_int(fp);
    if (max < 1 || max > 65535) {
      return NULL;
    }
  }
  // タイプに応じて初期化
  switch (type) {
    case 1:
    case 4:// pbmは2色のカラーパレット形式で表現する
      if ((img = allocate_image(width, height, COLOR_TYPE_INDEX)) == NULL) {
        return NULL;
      }
      img->palette_num = 2;
      img->palette[0] = color_from_rgb(255, 255, 255);
      img->palette[1] = color_from_rgb(0, 0, 0);
      break;
    case 2:
    case 5:
      if ((img = allocate_image(width, height, COLOR_TYPE_GRAY)) == NULL) {
        return NULL;
      }
      break;
    case 3:
    case 6:
      if ((img = allocate_image(width, height, COLOR_TYPE_RGB)) == NULL) {
        return NULL;
      }
      break;
  }
  switch (type) {
    case 1:  // ASCII 2値
      result = read_p1(fp, img);
      break;
    case 2:  // ASCII グレースケール
      result = read_p2(fp, img, max);
      break;
    case 3:  // ASCII RGB
      result = read_p3(fp, img, max);
      break;
    case 4:  // バイナリ 2値
      result = read_p4(fp, img);
      break;
    case 5:  // バイナリ グレースケール
      result = read_p5(fp, img, max);
      break;
    case 6:  // バイナリ RGB
      result = read_p6(fp, img, max);
      break;
  }
  if (result != SUCCESS) {
    free_image(img);
    return NULL;
  }
  return img;
}

/**
 * @brief PNM(PPM/PGM/PBM)形式としてファイルに書き出す。
 *
 * 入力された画像データが出力タイプと合致しない場合、
 * 内部で変換して出力を行う。
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @param[in] type     出力タイプ、1～6を指定
 * @return 成否
 */
result_t write_pnm_file(const char *filename, image_t *img, int type) {
  result_t result = FAILURE;
  if (img == NULL) {
    return result;
  }
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror(filename);
    return result;
  }
  result = write_pnm_stream(fp, img, type);
  fclose(fp);
  return result;
}

/**
 * @brief P1の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
static result_t write_p1(FILE *fp, image_t *img) {
  int x, y;
  for (y = 0; y < img->height; y++) {
    int line = 0;
    for (x = 0; x < img->width; x++) {
      if(++line > 69) {
        putc('\n', fp);
        line = 1;
      }
      putc('0' + img->map[y][x].i, fp);
    }
    putc('\n', fp);
  }
  return SUCCESS;
}

/**
 * @brief P2の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
static result_t write_p2(FILE *fp, image_t *img) {
  int x, y;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      fprintf(fp, "%u\n", img->map[y][x].g);
    }
  }
  return SUCCESS;
}

/**
 * @brief P3の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
static result_t write_p3(FILE *fp, image_t *img) {
  int x, y;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      fprintf(fp, "%u %u %u\n",
          img->map[y][x].c.r,
          img->map[y][x].c.g,
          img->map[y][x].c.b);
    }
  }
  return SUCCESS;
}

/**
 * @brief P4の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
static result_t write_p4(FILE *fp, image_t *img) {
  int x, y;
  uint8_t p;
  for (y = 0; y < img->height; y++) {
    int pos = 0;
    int shift = 8;
    p = 0;
    // 上位ビットから詰め込み、1byte分たまったら出力
    for (x = 0; x < img->width; x++) {
      shift--;
      p |= img->map[y][x].i << shift;
      if (shift == 0) {
        putc(p, fp);
        shift = 8;
        pos++;
        p = 0;
      }
    }
    // 端があればここで出力
    if (shift != 8) {
      putc(p, fp);
    }
  }
  return SUCCESS;
}

/**
 * @brief P5の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @return 成否
 */
static result_t write_p5(FILE *fp, image_t *img) {
  int x, y;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      putc(img->map[y][x].g, fp);
    }
  }
  return SUCCESS;
}

/**
 * @brief P6の画像データを読み込む。
 *
 * @param[in] fp  書き出すファイルストリームのポインタ
 * @param[in] img 画像データ
 * @param 成否
 */
static result_t write_p6(FILE *fp, image_t *img) {
  int x, y;
  for (y = 0; y < img->height; y++) {
    for (x = 0; x < img->width; x++) {
      putc(img->map[y][x].c.r, fp);
      putc(img->map[y][x].c.g, fp);
      putc(img->map[y][x].c.b, fp);
    }
  }
  return SUCCESS;
}

/**
 * @brief PNM(PPM/PGM/PBM)形式としてファイルに書き出す。
 *
 * 入力された画像データが出力タイプと合致しない場合、
 * 内部で変換して出力を行う。
 *
 * @param[in] fp    書き出すファイルストリームのポインタ
 * @param[in] img   画像データ
 * @param[in] type  出力タイプ、1～6を指定
 * @return 成否
 */
result_t write_pnm_stream(FILE *fp, image_t *img, int type) {
  image_t *work = NULL;
  if (img == NULL) {
    return FAILURE;
  }
  if (type < 1 || type > 6) {
    return FAILURE;
  }
  // 出力形式を指定するので、所望の形式でない場合は自動的に変換する
  switch (type) {
    case 1:
    case 4:
      if (img->palette_num != 2) {
        work = clone_image(img);
        img = image_to_gray(work);
        img = image_gray_to_binary(img);
      }
      break;
    case 2:
    case 5:
      if (img->color_type != COLOR_TYPE_GRAY) {
        work = clone_image(img);
        img = image_to_gray(work);
      }
      break;
    case 3:
    case 6:
      if (img->color_type != COLOR_TYPE_RGB) {
        work = clone_image(img);
        img = image_to_rgb(work);
      }
      break;
  }
  // ヘッダ出力、コメントなし
  fprintf(fp, "P%d\n", type);
  fprintf(fp, "%u %u\n", img->width, img->height);
  if (type != 1 && type != 4) {
    fprintf(fp, "255\n");
  }
  switch (type) {
    case 1:  // ASCII 2値
      write_p1(fp, img);
      break;
    case 2:  // ASCII グレースケール
      write_p2(fp, img);
      break;
    case 3:  // ASCII RGB
      write_p3(fp, img);
      break;
    case 4:  // バイナリ 2値
      write_p4(fp, img);
      break;
    case 5:  // バイナリ グレースケール
      write_p5(fp, img);
      break;
    case 6:  // バイナリ RGB
      write_p6(fp, img);
      break;
  }
  free_image(work);
  return SUCCESS;
}
