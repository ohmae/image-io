/**
 * @file bmp.c
 *
 * Copyright(c) 2015 大前良介(OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief BMPファイルの読み書き処理
 * @author <a href="mailto:ryo@mm2d.net">大前良介(OHMAE Ryosuke)</a>
 * @date 2015/02/07
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "image.h"

#define NO_ERROR      0 /**< エラー無し */
#define ERROR         1 /**< エラー */

#define BI_RGB        0 /**< RGB */
#define BI_RLE8       1 /**< 8bitRLE */
#define BI_RLE4       2 /**< 4bitRLE */
#define BI_BITFIELDS  3 /**< ビットフィールド */
#define BI_JPEG       4 /**< JEPG格納 */
#define BI_PNG        5 /**< PNG格納 */

#define FILE_TYPE 0x4D42 /**< "BM"をリトルエンディアンで解釈した値 */

#define FILE_HEADER_SIZE  14      /**< BMPファイルヘッダサイズ */
#define CORE_HEADER_SIZE  12      /**< OS/2ヘッダサイズ */
#define INFO2_HEADER_SIZE 64      /**< OS/2拡張ヘッダサイズ */
#define INFO_HEADER_SIZE  40      /**< Windowsヘッダサイズ */
#define V4_HEADER_SIZE    108     /**< WindowsV4拡張ヘッダサイズ */
#define V5_HEADER_SIZE    124     /**< WindowsV5拡張ヘッダサイズ */
#define PALET_SIZE_MAX    (4*256) /**< パレットの最大サイズ */

#define LCS_CALIBRATED_RGB                0
#define LCS_sRGB                 0x73524742
#define LCS_WINDOWS_COLOR_SPACE  0x57696E20
#define LCS_GM_BUSINESS                   1
#define LCS_GM_GRAPHICS                   2
#define LCS_GM_IMAGES                     4
#define LCS_GM_ABS_COLORIMETRIC           8

/** 標準のヘッダサイズ */
#define DEFAULT_HEADER_SIZE (FILE_HEADER_SIZE + INFO_HEADER_SIZE)
/** INFOヘッダ最大サイズ */
#define INFO_HEADER_SIZE_MAX V5_HEADER_SIZE
/** 想定されるヘッダの最大サイズ */
#define OFF_BITS_MAX (FILE_HEADER_SIZE + INFO_HEADER_SIZE_MAX + PALET_SIZE_MAX)

/**
 * @brief BMPファイルヘッダ
 *
 * メモリマップとして利用するには
 * pragmaが必要
 */
typedef struct BITMAPFILEHEADER {
  uint16_t bfType;      /**< ファイルタイプ、必ず"BM" */
  uint32_t bfSize;      /**< ファイルサイズ */
  uint16_t bfReserved1; /**< リザーブ */
  uint16_t bfReserved2; /**< リサーブ */
  uint32_t bfOffBits;   /**< 先頭から画像情報までのオフセット、ヘッダ構造体＋パレットサイズ */
} BITMAPFILEHEADER;

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
 * @brief 色情報を読み出すためのチャンル情報
 */
typedef struct channel_mask {
  uint32_t mask;  /**< マスク */
  uint32_t shift; /**< シフト量 */
  uint32_t max;   /**< 最大値 */
} channel_mask;

/**
 * @brief チャンネル情報を使ってRGB値を読み出すマクロ
 */
#define CMASK(d, m) (((((d) & (m).mask) >> (m).shift) * 255 + (m).max / 2) / (m).max)

/**
 * @brief BMPのヘッダ情報を集めた構造体
 */
typedef struct bmp_header_t {
  BITMAPFILEHEADER file;  /**< ファイルヘッダ */
  BITMAPINFOHEADER info;  /**< 情報ヘッダ */
  channel_mask cmasks[4]; /**< 色読み出し情報 */
} bmp_header_t;

/**
 * @brief 簡易バイトストリーム
 */
typedef struct bs_t {
  uint8_t *buffer; /**< バッファ */
  size_t offset;   /**< 現在のオフセット */
  size_t size;     /**< バッファのサイズ */
  int error;       /**< エラー */
} bs_t;

static void bs_init(uint8_t *buffer, size_t size, bs_t *bs);
static void bs_set_offset(bs_t *bs, int offset);
static uint8_t bs_read8(bs_t *bs);
static uint16_t bs_read16(bs_t *bs);
static uint32_t bs_read32(bs_t *bs);
static void bs_write8(bs_t *bs, uint8_t data);
static void bs_write16(bs_t *bs, uint16_t data);
static void bs_write32(bs_t *bs, uint32_t data);

static void set_default_color_masks(uint16_t bit_count, channel_mask *cmasks);
static void read_color_masks(uint32_t *masks, channel_mask *cmasks);
static result_t read_file_header(FILE *fp, bmp_header_t *header);
static result_t read_info_header(FILE *fp, bmp_header_t *header);
static result_t read_palette(FILE *fp, bmp_header_t *header, image_t *img);
static result_t read_bitmap_32(FILE *fp, bmp_header_t *header, int stride, image_t *img);
static result_t read_bitmap_24(FILE *fp, bmp_header_t *header, int stride, image_t *img);
static result_t read_bitmap_16(FILE *fp, bmp_header_t *header, int stride, image_t *img);
static result_t read_bitmap_index(FILE *fp, bmp_header_t *header, int stride, image_t *img);
static result_t read_bitmap_rle(FILE *fp, bmp_header_t *header, int stride, image_t *img);
static result_t read_bitmap(FILE *fp, bmp_header_t *header, image_t *img);

static result_t write_header(FILE *fp, image_t *img, int bc, int image_size, int compress);
static result_t write_palette(FILE *fp, image_t *img, int bc);
static result_t write_bitmap_32(FILE *fp, image_t *img, int bc, int stride);
static result_t write_bitmap_24(FILE *fp, image_t *img, int bc, int stride);
static result_t write_bitmap_index(FILE *fp, image_t *img, int bc, int stride);
static result_t write_bitmap_rle(FILE *fp, image_t *img, int bc, int stride);
static result_t write_bitmap(FILE *fp, image_t *img, int bc, int compress);

/**
 * @brief バイトストリームを初期化する
 *
 * @param[in] buffer バイトストリームに使用するバッファ
 * @param[in] size   バッファのサイズ
 * @param[out] bs    バイトストリーム
 */
static void bs_init(uint8_t *buffer, size_t size, bs_t *bs) {
  bs->buffer = buffer;
  bs->size = size;
  bs->offset = 0;
  bs->error = NO_ERROR;
}

/**
 * @brief バイトストリームのオフセットを変更する
 *
 * @param[in] bs バイトストリーム
 * @param[in] bs オフセット
 */
static void bs_set_offset(bs_t *bs, int offset) {
  bs->offset = offset;
}

/**
 * @brief バイトストリームから1バイト読み出し
 *
 * @param[in] bs バイトストリーム
 * @retrun 読みだしたデータ
 */
static uint8_t bs_read8(bs_t *bs) {
  uint8_t *b = &bs->buffer[bs->offset];
  if (bs->offset + 1 > bs->size) {
    bs->error = ERROR;
    return 0;
  }
  bs->offset += 1;
  return b[0];
}

/**
 * @brief バイトストリームから2バイト読み出し
 *
 * バイトストリームをリトルエンディアンとして処理する。
 *
 * @param[in] bs バイトストリーム
 * @retrun 読みだしたデータ
 */
static uint16_t bs_read16(bs_t *bs) {
  uint8_t *b = &bs->buffer[bs->offset];
  if (bs->offset + 2 > bs->size) {
    bs->error = ERROR;
    return 0;
  }
  bs->offset += 2;
  return b[0] | b[1] << 8;
}

/**
 * @brief バイトストリームから4バイト読み出し
 *
 * バイトストリームをリトルエンディアンとして処理する。
 *
 * @param[in] bs バイトストリーム
 * @retrun 読みだしたデータ
 */
static uint32_t bs_read32(bs_t *bs) {
  uint8_t *b = &bs->buffer[bs->offset];
  if (bs->offset + 4 > bs->size) {
    bs->error = ERROR;
    return 0;
  }
  bs->offset += 4;
  return b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;
}

/**
 * @brief バイトストリームへ1バイト書き出し
 *
 * @param[in,out] bs   バイトストリーム
 * @param[in]     data 書き出すデータ
 */
static void bs_write8(bs_t *bs, uint8_t data) {
  uint8_t *src = &data;
  uint8_t *dst = &bs->buffer[bs->offset];
  if (bs->offset + 1 > bs->size) {
    bs->error = ERROR;
    return;
  }
  bs->offset += 1;
  dst[0] = src[0];
}

/**
 * @brief バイトストリームへ2バイト書き出し
 *
 * リトルエンディアンで書き出しを行う
 *
 * @param[in,out] bs   バイトストリーム
 * @param[in]     data 書き出すデータ
 */
static void bs_write16(bs_t *bs, uint16_t data) {
  uint8_t *src = (uint8_t *) &data;
  uint8_t *dst = &bs->buffer[bs->offset];
  if (bs->offset + 2 > bs->size) {
    bs->error = ERROR;
    return;
  }
  dst[0] = src[0];
  dst[1] = src[1];
  bs->offset += 2;
}

/**
 * @brief バイトストリームへ1バイト書き出し
 *
 * リトルエンディアンで書き出しを行う
 *
 * @param[in,out] bs   バイトストリーム
 * @param[in]     data 書き出すデータ
 */
static void bs_write32(bs_t *bs, uint32_t data) {
  uint8_t *src = (uint8_t *) &data;
  uint8_t *dst = &bs->buffer[bs->offset];
  if (bs->offset + 4 > bs->size) {
    bs->error = ERROR;
    return;
  }
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
  bs->offset += 4;
}

/**
 * @brief ビット数に応じて色読み出しマスクを設定する。
 *
 * @param[in]  bit_count ビット数
 * @param[out] cmasks    色読み出しマスク
 */
static void set_default_color_masks(uint16_t bit_count, channel_mask *cmasks) {
  uint32_t masks[4];
  switch (bit_count) {
    case 32:  // Blue,Green,Red,Reserveの順に8bitずつ
      masks[0] = 0x00ff0000;
      masks[1] = 0x0000ff00;
      masks[2] = 0x000000ff;
      masks[3] = 0x00000000;
      read_color_masks(masks, cmasks);
      break;
    case 16:  // Blue,Green,Redの順に5bitずつ、1bit余る
      masks[0] = 0x7c00;
      masks[1] = 0x03e0;
      masks[2] = 0x001f;
      masks[3] = 0x0000;
      read_color_masks(masks, cmasks);
      break;
    default:
      break;
  }
}

/**
 * @brief ヘッダから読みだしたマスク情報から色読み出しマスクを設定する。
 *
 * @param[in]  masks  マスク情報
 * @param[out] cmasks 色読み出しマスク
 */
static void read_color_masks(uint32_t *masks, channel_mask *cmasks) {
  int i, b;
  for (i = 0; i < 4; i++) {
    cmasks[i].mask = masks[i];
    if (cmasks[i].mask == 0) {
      cmasks[i].shift = 0;
      cmasks[i].max = 0xff;
      continue;
    } else {
      for (b = 0; b < 32; b++) {
        if (cmasks[i].mask & (1 << b)) {
          cmasks[i].shift = b;
          cmasks[i].max = cmasks[i].mask >> cmasks[i].shift;
          break;
        }
      }
      if (cmasks[i].max == 0) {
        cmasks[i].max = 0xff;
      }
    }
  }
}

/**
 * @brief ファイルヘッダ読み込み
 *
 * @param[in]  fp     ファイルストリーム
 * @param[out] header ヘッダ情報
 * @return 成否
 */
static result_t read_file_header(FILE *fp, bmp_header_t *header) {
  bs_t bs;
  uint8_t buffer[FILE_HEADER_SIZE];
  if (fread(buffer, FILE_HEADER_SIZE, 1, fp) != 1) {
    return FAILURE;
  }
  bs_init(buffer, FILE_HEADER_SIZE, &bs);
  header->file.bfType = bs_read16(&bs);
  header->file.bfSize = bs_read32(&bs);
  header->file.bfReserved1 = bs_read16(&bs);
  header->file.bfReserved2 = bs_read16(&bs);
  header->file.bfOffBits = bs_read32(&bs);
  // ファイルタイプチェック
  if (header->file.bfType != FILE_TYPE) {
    return FAILURE;
  }
  if (header->file.bfOffBits > OFF_BITS_MAX) {
    return FAILURE;
  }
  return SUCCESS;
}

/**
 * @brief 情報ヘッダ読み込み
 *
 * @param[in]     fp     ファイルストリーム
 * @param[in,out] header ヘッダ
 * @param[out]    cmasks 読み出しマスクの格納先
 * @return 成否
 */
static result_t read_info_header(FILE *fp, bmp_header_t *header) {
  bs_t bs;
  uint8_t buffer[INFO_HEADER_SIZE_MAX];
  int buf_size = 4;
  // 先頭4byteを読み出す
  if (fread(buffer, buf_size, 1, fp) != 1) {
    return FAILURE;
  }
  bs_init(buffer, buf_size, &bs);
  header->info.biSize = bs_read32(&bs);
  if (header->info.biSize > INFO_HEADER_SIZE_MAX) {
    return FAILURE;
  }
  buf_size = header->info.biSize - buf_size;
  if (fread(buffer, buf_size, 1, fp) != 1) {
    return FAILURE;
  }
  bs_init(buffer, buf_size, &bs);
  if (header->info.biSize == CORE_HEADER_SIZE) {
    // OS/2 ビットマップ、この場合のみカラーパレットが3byte
    header->info.biWidth = bs_read16(&bs);  // 16bit
    header->info.biHeight = bs_read16(&bs);  // 16bit
    header->info.biPlanes = bs_read16(&bs);
    header->info.biBitCount = bs_read16(&bs);
    header->info.biCompression = 0;
    header->info.biSizeImage = 0;
    header->info.biXPelsPerMeter = 0;
    header->info.biYPelsPerMeter = 0;
    header->info.biClrUsed = 0;
    header->info.biClrImportant = 0;
  } else if (header->info.biSize == INFO_HEADER_SIZE
      || header->info.biSize == INFO2_HEADER_SIZE) {
    // Windowsビットマップ
    header->info.biWidth = bs_read32(&bs);
    header->info.biHeight = bs_read32(&bs);
    header->info.biPlanes = bs_read16(&bs);
    header->info.biBitCount = bs_read16(&bs);
    header->info.biCompression = bs_read32(&bs);
    header->info.biSizeImage = bs_read32(&bs);
    header->info.biXPelsPerMeter = bs_read32(&bs);
    header->info.biYPelsPerMeter = bs_read32(&bs);
    header->info.biClrUsed = bs_read32(&bs);
    header->info.biClrImportant = bs_read32(&bs);
    if (header->info.biCompression == BI_BITFIELDS) {
      // パレット部分にビットフィールドが格納されている
      uint32_t masks[4];
      buf_size = 4 * 3;  // RGBの3つのマスクを読み出す
      if (header->file.bfOffBits - FILE_HEADER_SIZE - header->info.biSize
          < buf_size) {
        // 読み出せるビットフィールドがない
        return FAILURE;
      }
      if (fread(buffer, buf_size, 1, fp) != 1) {
        return FAILURE;
      }
      bs_init(buffer, buf_size, &bs);
      masks[0] = bs_read32(&bs);
      masks[1] = bs_read32(&bs);
      masks[2] = bs_read32(&bs);
      masks[3] = 0;
      read_color_masks(masks, header->cmasks);
    } else if (header->info.biCompression == BI_RGB) {
      set_default_color_masks(header->info.biBitCount, header->cmasks);
    }
  } else if (header->info.biSize == V4_HEADER_SIZE
      || header->info.biSize == V5_HEADER_SIZE) {
    // V4/V5ヘッダ、読み込みマスクのみ利用する
    header->info.biWidth = bs_read32(&bs);
    header->info.biHeight = bs_read32(&bs);
    header->info.biPlanes = bs_read16(&bs);
    header->info.biBitCount = bs_read16(&bs);
    header->info.biCompression = bs_read32(&bs);
    header->info.biSizeImage = bs_read32(&bs);
    header->info.biXPelsPerMeter = bs_read32(&bs);
    header->info.biYPelsPerMeter = bs_read32(&bs);
    header->info.biClrUsed = bs_read32(&bs);
    header->info.biClrImportant = bs_read32(&bs);
    if (header->info.biCompression == BI_BITFIELDS) {
      // V4/V5ヘッダはヘッダ内にマスクがある
      uint32_t masks[4];
      masks[0] = bs_read32(&bs);
      masks[1] = bs_read32(&bs);
      masks[2] = bs_read32(&bs);
      masks[3] = bs_read32(&bs);
      read_color_masks(masks, header->cmasks);
    } else if (header->info.biCompression == BI_RGB) {
      set_default_color_masks(header->info.biBitCount, header->cmasks);
    }
  } else {
    // ヘッダサイズ異常
    return FAILURE;
  }
  // ヘッダの整合性チェック
  if (!(header->info.biBitCount == 1
      || header->info.biBitCount == 4
      || header->info.biBitCount == 8
      || header->info.biBitCount == 16
      || header->info.biBitCount == 24
      || header->info.biBitCount == 32)) {
    // 有効なビット数は1,4,8,16,24,32のみ
    return FAILURE;
  }
  if (!(header->info.biCompression == BI_RGB
      || (header->info.biBitCount == 4
          && header->info.biCompression == BI_RLE4)
      || (header->info.biBitCount == 8
          && header->info.biCompression == BI_RLE8)
      || (header->info.biBitCount == 16
          && header->info.biCompression == BI_BITFIELDS)
      || (header->info.biBitCount == 32
          && header->info.biCompression == BI_BITFIELDS))) {
    // 有効な圧縮形式か判定、RGBは有効なビット数全てOK
    // RLE4は4bitのみ、RLEは8bitのみ、BITFIELDSは16bitか32bitの時のみ有効
    // JPEGやPNGは対応外とする。
    return FAILURE;
  }
  if (header->info.biWidth <= 0
      || header->info.biHeight == 0
      || header->info.biHeight == INT32_MIN) {
    // サイズ異常、widthは正の値である必要がある。
    // heightは0でなければ正負どちらであっても良いが、
    // INT32_MINの場合は符号反転不可能のため異常扱い
    return FAILURE;
  }
  return SUCCESS;
}

/**
 * @brief パレット情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_palette(FILE *fp, bmp_header_t *header, image_t *img) {
  int i;
  bs_t bs;
  uint8_t buffer[PALET_SIZE_MAX];
  // OS/2形式ではRGBTRIPLE、それ以外はRGBQUAD
  int color_size = (header->info.biSize == CORE_HEADER_SIZE ? 3 : 4);
  int palette_size = header->file.bfOffBits - FILE_HEADER_SIZE
      - header->info.biSize;
  int palette_num = palette_size / color_size;
  int palette_max = (1 << header->info.biBitCount);
  if (palette_num < header->info.biClrUsed) {
    // 色数よりパレットが小さいので異常
    return FAILURE;
  }
  if (palette_num > palette_max) {
    // ビット数から計算された最大値より大きい場合はその値を最大値とする。
    palette_num = palette_max;
  }
  if (header->info.biClrUsed != 0 && header->info.biClrUsed < palette_num) {
    // 色数がヘッダに記載されている場合はそちらを優先
    palette_num = header->info.biClrUsed;
  }
  palette_size = palette_num * color_size;
  if (fread(buffer, palette_size, 1, fp) != 1) {
    return FAILURE;
  }
  bs_init(buffer, palette_size, &bs);
  img->palette_num = palette_num;
  for (i = 0; i < palette_num; i++) {
    img->palette[i].b = bs_read8(&bs);
    img->palette[i].g = bs_read8(&bs);
    img->palette[i].r = bs_read8(&bs);
    img->palette[i].a = 0xff;
    if (color_size != 3) {
      bs_read8(&bs);  // Reserve読み飛ばし
    }
  }
  return SUCCESS;
}

/**
 * @brief BitCount=32のビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[in]  stride 1行のサイズ
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap_32(
    FILE *fp, bmp_header_t *header, int stride, image_t *img) {
  int x, y;
  bs_t bs;
  uint8_t *buffer;
  int width = header->info.biWidth;
  int height = abs(header->info.biHeight);  // 高さは負の可能性がある
  if ((buffer = malloc(stride)) == NULL) {
    return FAILURE;
  }
  for (y = height - 1; y >= 0; y--) {
    if (fread(buffer, stride, 1, fp) != 1) {
      free(buffer);
      return FAILURE;
    }
    bs_init(buffer, stride, &bs);
    for (x = 0; x < width; x++) {
      uint32_t tmp = bs_read32(&bs);
      img->map[y][x].c.r = CMASK(tmp, header->cmasks[0]);
      img->map[y][x].c.g = CMASK(tmp, header->cmasks[1]);
      img->map[y][x].c.b = CMASK(tmp, header->cmasks[2]);
      if (header->cmasks[3].mask == 0) {
        img->map[y][x].c.a = 0xff;
      } else {
        img->map[y][x].c.a = CMASK(tmp, header->cmasks[3]);
      }
    }
  }
  free(buffer);
  return SUCCESS;
}

/**
 * @brief BitCount=24のビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[in]  stride 1行のサイズ
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap_24(
    FILE *fp, bmp_header_t *header, int stride, image_t *img) {
  int x, y;
  bs_t bs;
  uint8_t *buffer;
  int width = header->info.biWidth;
  int height = abs(header->info.biHeight);  // 高さは負の可能性がある
  if ((buffer = malloc(stride)) == NULL) {
    return FAILURE;
  }
  for (y = height - 1; y >= 0; y--) {
    if (fread(buffer, stride, 1, fp) != 1) {
      free(buffer);
      return FAILURE;
    }
    bs_init(buffer, stride, &bs);
    for (x = 0; x < width; x++) {
      img->map[y][x].c.b = bs_read8(&bs);
      img->map[y][x].c.g = bs_read8(&bs);
      img->map[y][x].c.r = bs_read8(&bs);
      img->map[y][x].c.a = 0xff;
    }
  }
  free(buffer);
  return SUCCESS;
}

/**
 * @brief BitCount=16のビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[in]  stride 1行のサイズ
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap_16(
    FILE *fp, bmp_header_t *header, int stride, image_t *img) {
  int x, y;
  bs_t bs;
  uint8_t *buffer;
  int width = header->info.biWidth;
  int height = abs(header->info.biHeight);  // 高さは負の可能性がある
  if ((buffer = malloc(stride)) == NULL) {
    return FAILURE;
  }
  for (y = height - 1; y >= 0; y--) {
    if (fread(buffer, stride, 1, fp) != 1) {
      free(buffer);
      return FAILURE;
    }
    bs_init(buffer, stride, &bs);
    for (x = 0; x < width; x++) {
      uint16_t tmp = bs_read16(&bs);
      img->map[y][x].c.r = CMASK(tmp, header->cmasks[0]);
      img->map[y][x].c.g = CMASK(tmp, header->cmasks[1]);
      img->map[y][x].c.b = CMASK(tmp, header->cmasks[2]);
      if (header->cmasks[3].mask == 0) {
        img->map[y][x].c.a = 0xff;
      } else {
        img->map[y][x].c.a = CMASK(tmp, header->cmasks[3]);
      }
    }
  }
  free(buffer);
  return SUCCESS;
}

/**
 * @brief インデックスカラーのビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[in]  stride 1行のサイズ
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap_index(
    FILE *fp, bmp_header_t *header, int stride, image_t *img) {
  int x, y;
  bs_t bs;
  uint8_t *buffer;
  uint8_t tmp;
  int bc = header->info.biBitCount;
  uint32_t mask = (1 << bc) - 1;
  int width = header->info.biWidth;
  int height = abs(header->info.biHeight);  // 高さは負の可能性がある
  if ((buffer = malloc(stride)) == NULL) {
    return FAILURE;
  }
  for (y = height - 1; y >= 0; y--) {
    int shift = 8;
    if (fread(buffer, stride, 1, fp) != 1) {
      free(buffer);
      return FAILURE;
    }
    bs_init(buffer, stride, &bs);
    tmp = bs_read8(&bs);
    for (x = 0; x < width; x++) {
      shift -= bc;
      img->map[y][x].i = (tmp >> shift) & mask;
      if (shift == 0) {
        shift = 8;
        tmp = bs_read8(&bs);
      }
    }
  }
  free(buffer);
  return SUCCESS;
}

/**
 * @brief RLE4/RLE8のビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[in]  stride 1行のサイズ
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap_rle(
    FILE *fp, bmp_header_t *header, int stride, image_t *img) {
  int x, y, i;
  bs_t bs;
  uint8_t buffer[256];
  uint8_t tmp;
  int bc = header->info.biBitCount;
  int mask = (1 << bc) - 1;
  int width = header->info.biWidth;
  y = abs(header->info.biHeight) - 1;
  x = 0;
  while (y >= 0 && x <= width) {
    if (fread(buffer, 2, 1, fp) != 1) {
      return FAILURE;
    }
    if (buffer[0] != 0) {  // エンコードデータ
      for (i = 0; i < buffer[0] && x < width;) {
        int shift = 8 - bc;
        for (; shift >= 0 && i < buffer[0] && x < width; shift -= bc) {
          img->map[y][x].i = (buffer[1] >> shift) & mask;
          x++;
          i++;
        }
      }
    } else if (buffer[1] > 2) {  // 絶対モード
      int shift = 8;
      int n = buffer[1];
      int c = (n * bc + 15) / 16 * 2;  // 2byte単位揃え
      if (fread(buffer, c, 1, fp) != 1) {
        return FAILURE;
      }
      bs_init(buffer, c, &bs);
      tmp = bs_read8(&bs);
      for (i = 0; i < n && x < width; i++) {
        shift -= bc;
        img->map[y][x].i = (tmp >> shift) & mask;
        x++;
        if (shift == 0) {
          shift = 8;
          tmp = bs_read8(&bs);
        }
      }
    } else if (buffer[1] == 2) {  // 移動
      if (fread(buffer, 2, 1, fp) != 1) {
        return FAILURE;
      }
      x += buffer[0];
      y -= buffer[1];
    } else if (buffer[1] == 1) {  // 終了
      break;
    } else {  // 行終了、次の行へ
      x = 0;
      y--;
    }
  }
  return SUCCESS;
}

/**
 * @brief ビットマップ情報を読み込む
 *
 * @param[in]  fp     ファイルストリーム
 * @param[in]  header ヘッダ情報
 * @param[out] img    画像構造体
 * @return 成否
 */
static result_t read_bitmap(FILE *fp, bmp_header_t *header, image_t *img) {
  int stride = (header->info.biWidth * header->info.biBitCount + 31) / 32 * 4;
  switch (header->info.biBitCount) {
    case 32:
      return read_bitmap_32(fp, header, stride, img);
    case 24:
      return read_bitmap_24(fp, header, stride, img);
    case 16:
      return read_bitmap_16(fp, header, stride, img);
    case 8:
    case 4:
    case 1:
      if (header->info.biCompression == BI_RGB) {
        return read_bitmap_index(fp, header, stride, img);
      }
      return read_bitmap_rle(fp, header, stride, img);
  }
  return FAILURE;
}

/**
 * @brief BMP形式のファイルを読み込む。
 *
 * @param[in] filename ファイル名
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_bmp_file(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror(filename);
    return NULL;
  }
  image_t *img = read_bmp_stream(fp);
  fclose(fp);
  return img;
}

/**
 * @brief BMP形式のファイルを読み込む。
 *
 * @param[in] fp 読み込むファイルストリームのポインタ
 * @return 読み込んだ画像、読み込みに失敗した場合NULL
 */
image_t *read_bmp_stream(FILE *fp) {
  image_t *img = NULL;
  bmp_header_t header;
  int height;
  uint16_t color_type;
  memset(&header, 0, sizeof(header));
  if ((read_file_header(fp, &header) != SUCCESS) ||
      (read_info_header(fp, &header) != SUCCESS)) {
    return NULL;
  }
  if (header.info.biBitCount <= 8) {
    color_type = COLOR_TYPE_INDEX;
  } else if (header.cmasks[3].mask == 0) {
    color_type = COLOR_TYPE_RGB;
  } else {
    color_type = COLOR_TYPE_RGBA;
  }
  height = abs(header.info.biHeight);  // 高さは負の可能性がある
  if ((img = allocate_image(header.info.biWidth, height, color_type)) == NULL) {
    return NULL;
  }
  if (color_type == COLOR_TYPE_INDEX) {
    if (read_palette(fp, &header, img) != SUCCESS) {
      goto error;
    }
  }
  if (fseek(fp, header.file.bfOffBits, SEEK_SET) != 0) {
    goto error;
  }
  if (read_bitmap(fp, &header, img) != SUCCESS) {
    goto error;
  }
  if (header.info.biHeight < 0) {
    // 高さが負の値の場合、トップダウン方式なので上下を反転させる
    int i;
    for (i = 0; i < height / 2; i++) {
      pixcel_t *tmp = img->map[i];
      img->map[i] = img->map[height - 1 - i];
      img->map[height - 1 - i] = tmp;
    }
  }
  return img;
  error:
  free_image(img);
  return NULL;
}

/**
 * @brief ファイルヘッダを書き出す
 *
 * @param[in] fp         ファイルストリーム
 * @param[in] img        画像データ
 * @param[in] bc         1色あたりのビット数
 * @param[in] image_size 画像サイズ
 * @param[in] compress   TRUEの時RLEを使用する
 * @return 成否
 */
static result_t write_header(
    FILE *fp, image_t *img, int bc, int image_size, int compress) {
  result_t result = FAILURE;
  bs_t bs;
  uint8_t *header = NULL;
  int info_header_size = (bc == 32 ? V5_HEADER_SIZE : INFO_HEADER_SIZE);
  int header_size = FILE_HEADER_SIZE + info_header_size;
  int palette_size = 0;
  if (bc <= 8) {
    palette_size = (1 << bc) * 4;
  }
  if ((header = calloc(header_size, 1)) == NULL) {
    return FAILURE;
  }
  // ヘッダ情報書き出し
  bs_init(header, header_size, &bs);
  bs_write16(&bs, FILE_TYPE);  // bfType
  bs_write32(&bs, header_size + palette_size + image_size);  // bfSize
  bs_write16(&bs, 0);  // bfReserved1
  bs_write16(&bs, 0);  // bfReserved2
  bs_write32(&bs, header_size + palette_size);  // bfOffBits
  bs_write32(&bs, info_header_size);  // biSize
  bs_write32(&bs, img->width);  // biWidth
  bs_write32(&bs, img->height);  // biHeight
  bs_write16(&bs, 1);  // biPlanes
  bs_write16(&bs, bc);  // biBitCount
  if (bc == 32) {  // biCompression
    bs_write32(&bs, BI_BITFIELDS);
  } else if (bc == 8 && compress) {
    bs_write32(&bs, BI_RLE8);
  } else if (bc == 4 && compress) {
    bs_write32(&bs, BI_RLE4);
  } else {
    bs_write32(&bs, BI_RGB);
  }
  bs_write32(&bs, image_size);  // biSizeImage
  bs_write32(&bs, 0);  // biXPelsPerMeter
  bs_write32(&bs, 0);  // biYPelsPerMeter
  bs_write32(&bs, img->palette_num);  // biClrUsed
  bs_write32(&bs, 0);  // biClrImportant
  if (bc == 32) {
    bs_write32(&bs, 0xff000000);  // bV5RedMask
    bs_write32(&bs, 0x00ff0000);  // bV5GreenMask
    bs_write32(&bs, 0x0000ff00);  // bV5BlueMask
    bs_write32(&bs, 0x000000ff);  // bV5AlphaMask
    bs_write32(&bs, LCS_sRGB);  // bV5CSType
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzRed.ciexyzX
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzRed.ciexyzY
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzRed.ciexyzZ
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzGreen.ciexyzX
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzGreen.ciexyzY
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzGreen.ciexyzZ
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzBlue.ciexyzX
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzBlue.ciexyzY
    bs_write32(&bs, 0);  // bV5Endpoints.ciexyzBlue.ciexyzZ
    bs_write32(&bs, 0);  // bV5GammaRed
    bs_write32(&bs, 0);  // bV5GammaGreen
    bs_write32(&bs, 0);  // bV5GammaBlue
    bs_write32(&bs, LCS_GM_GRAPHICS);  // bV5Intent
    bs_write32(&bs, 0);  // bV5ProfileData
    bs_write32(&bs, 0);  // bV5ProfileSize
    bs_write32(&bs, 0);  // bV5Reserved
  }
  if (fwrite(header, header_size, 1, fp) != 1) {
    goto error;
  }
  result = SUCCESS;
  error: free(header);
  return result;
}

/**
 * @brief パレット情報を書き出す
 *
 * @param[in] fp  ファイルストリーム
 * @param[in] img 画像データ
 * @param[in] bc  1色あたりのビット数
 * @return 成否
 */
static result_t write_palette(FILE *fp, image_t *img, int bc) {
  result_t result = FAILURE;
  bs_t bs;
  int i;
  uint8_t *buffer = NULL;
  int palette_size = (1 << bc) * 4;
  if ((buffer = calloc(palette_size, 1)) == NULL) {
    return FAILURE;
  }
  bs_init(buffer, palette_size, &bs);
  for (i = 0; i < img->palette_num; i++) {
    bs_write8(&bs, img->palette[i].b);
    bs_write8(&bs, img->palette[i].g);
    bs_write8(&bs, img->palette[i].r);
    bs_write8(&bs, 0);
  }
  if (fwrite(buffer, palette_size, 1, fp) != 1) {
    goto error;
  }
  result = SUCCESS;
  error:
  free(buffer);
  return result;
}

/**
 * @brief BitCount=32のビットマップ情報を書き出す
 *
 * @param[in] fp     ファイルストリーム
 * @param[in] img    画像データ
 * @param[in] bc     1色あたりのビット数
 * @param[in] stride 1行のサイズ
 * @return 成否
 */
static result_t write_bitmap_32(FILE *fp, image_t *img, int bc, int stride) {
  result_t result = FAILURE;
  bs_t bs;
  int x, y;
  uint8_t *row = NULL;
  if ((row = calloc(stride, 1)) == NULL) {
    goto error;
  }
  bs_init(row, stride, &bs);
  for (y = img->height - 1; y >= 0; y--) {
    bs_set_offset(&bs, 0);
    for (x = 0; x < img->width; x++) {
      bs_write8(&bs, img->map[y][x].c.a);
      bs_write8(&bs, img->map[y][x].c.b);
      bs_write8(&bs, img->map[y][x].c.g);
      bs_write8(&bs, img->map[y][x].c.r);
    }
    if (fwrite(row, stride, 1, fp) != 1) {
      goto error;
    }
  }
  result = SUCCESS;
  error:
  free(row);
  return result;
}

/**
 * @brief BitCount=24のビットマップ情報を書き出す
 *
 * @param[in] fp     ファイルストリーム
 * @param[in] img    画像データ
 * @param[in] bc     1色あたりのビット数
 * @param[in] stride 1行のサイズ
 * @return 成否
 */
static result_t write_bitmap_24(FILE *fp, image_t *img, int bc, int stride) {
  result_t result = FAILURE;
  bs_t bs;
  int x, y;
  uint8_t *row = NULL;
  if ((row = calloc(stride, 1)) == NULL) {
    goto error;
  }
  bs_init(row, stride, &bs);
  for (y = img->height - 1; y >= 0; y--) {
    bs_set_offset(&bs, 0);
    for (x = 0; x < img->width; x++) {
      bs_write8(&bs, img->map[y][x].c.b);
      bs_write8(&bs, img->map[y][x].c.g);
      bs_write8(&bs, img->map[y][x].c.r);
    }
    if (fwrite(row, stride, 1, fp) != 1) {
      goto error;
    }
  }
  result = SUCCESS;
  error:
  free(row);
  return result;
}

/**
 * @brief インデックスカラーのビットマップ情報を書き出す
 *
 * @param[in] fp     ファイルストリーム
 * @param[in] img    画像データ
 * @param[in] bc     1色あたりのビット数
 * @param[in] stride 1行のサイズ
 * @return 成否
 */
static result_t write_bitmap_index(FILE *fp, image_t *img, int bc, int stride) {
  result_t result = FAILURE;
  bs_t bs;
  int x, y;
  uint8_t *row = NULL;
  if ((row = calloc(stride, 1)) == NULL) {
    goto error;
  }
  bs_init(row, stride, &bs);
  for (y = img->height - 1; y >= 0; y--) {
    int shift = 8;
    uint8_t tmp = 0;
    bs_set_offset(&bs, 0);
    for (x = 0; x < img->width; x++) {
      shift -= bc;
      tmp |= img->map[y][x].i << shift;
      if (shift == 0) {
        shift = 8;
        bs_write8(&bs, tmp);
        tmp = 0;
      }
    }
    if (shift != 8) {
      bs_write8(&bs, tmp);
    }
    if (fwrite(row, stride, 1, fp) != 1) {
      goto error;
    }
  }
  result = SUCCESS;
  error:
  free(row);
  return result;
}

/**
 * @brief インデックスカラーのビットマップ情報をRLE形式で書き出す
 *
 * @param[in] fp     ファイルストリーム
 * @param[in] img    画像データ
 * @param[in] bc     1色あたりのビット数
 * @param[in] stride 1行のサイズ
 * @return 成否
 */
static result_t write_bitmap_rle(FILE *fp, image_t *img, int bc, int stride) {
  result_t result = FAILURE;
  bs_t bs;
  int i;
  int x, y;
  int num;
  int count;
  int reduction;
  uint8_t *raw = NULL;
  uint8_t *step = NULL;
  uint8_t *row = NULL;
  int image_size = 0;
  int cpb = 8 / bc; // 1Byteあたりの色数
  int count_max = 255 / cpb;
  stride = (img->width * bc + 7) / 8;
  raw = malloc(stride);
  step = malloc(stride);
  row = malloc(stride * 2);
  if (raw == NULL || step == NULL || row == NULL) {
    goto error;
  }
  for (y = img->height - 1; y >= 0; y--) {
    // 非圧縮データを作る
    int shift = 8;
    uint8_t tmp = 0;
    bs_init(raw, stride, &bs);
    for (x = 0; x < img->width; x++) {
      shift -= bc;
      tmp |= img->map[y][x].i << shift;
      if (shift == 0) {
        shift = 8;
        bs_write8(&bs, tmp);
        tmp = 0;
      }
    }
    if (shift != 8) {
      bs_write8(&bs, tmp);
    }
    // 非圧縮データの連続数を記録する
    for (x = 0; x < stride; x += count) {
      count = 0;
      tmp = raw[x];
      while ((x + count < stride)
          && (count < count_max)
          && (tmp == raw[x + count])) {
        count++;
      }
      step[x] = count;
    }
    // 圧縮データを作る
    bs_init(row, stride * 2, &bs);
    for (x = 0; x < stride; x += count) {
      if (step[x] < 2) {
        count = reduction = 0;
        while ((x + count < stride)
            && (count < count_max)
            && (step[x + count] <= 2)) {
          if (step[x + count] == 1) {
            reduction++;
          }
          count += step[x + count];
        }
        if (count * cpb > 255) { // 256になる可能性があるので戻す
          count -= 2;
        }
        if (reduction > 2) { // 絶対モード
          bs_write8(&bs, 0);
          num = count * cpb;
          if (num + x * cpb > img->width) {
            num--;
          }
          bs_write8(&bs, num);
          for (i = x; i < x + count; i++) {
            bs_write8(&bs, raw[i]);
          }
          if (count % 2) {
            bs_write8(&bs, 0);
          }
        } else { // エンコードモード
          for (i = x; i < x + count; i += step[i]) {
            num = step[i] * cpb;
            if (num + i * cpb > img->width) {
              num--;
            }
            bs_write8(&bs, num);
            bs_write8(&bs, raw[i]);
          }
        }
      } else { // エンコードモード
        count = step[x];
        num = count * cpb;
        if (num + x * cpb > img->width) {
          num--;
        }
        bs_write8(&bs, num);
        bs_write8(&bs, raw[x]);
      }
    }
    if (y == 0) { // EOF
      bs_write8(&bs, 0);
      bs_write8(&bs, 1);
    } else { // EOL
      bs_write8(&bs, 0);
      bs_write8(&bs, 0);
    }
    if (fwrite(row, bs.offset, 1, fp) != 1) {
      goto error;
    }
    image_size += bs.offset;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    goto error;
  }
  result = write_header(fp, img, bc, image_size, TRUE);
  error:
  free(raw);
  free(step);
  free(row);
  return result;
}

/**
 * @brief インデックスカラーのビットマップ情報をRLE形式で書き出す
 *
 * @param[in] fp       ファイルストリーム
 * @param[in] img      画像データ
 * @param[in] bc       1色あたりのビット数
 * @param[in] compress TRUEの時RLE圧縮を行う
 * @return 成否
 */
static result_t write_bitmap(FILE *fp, image_t *img, int bc, int compress) {
  int stride = (img->width * bc + 31) / 32 * 4;
  switch (bc) {
    case 32:
      return write_bitmap_32(fp, img, bc, stride);
    case 24:
      return write_bitmap_24(fp, img, bc, stride);
    case 8:
    case 4:
      if (compress) {
        return write_bitmap_rle(fp, img, bc, stride);
      } else {
        return write_bitmap_index(fp, img, bc, stride);
      }
    case 1:
      return write_bitmap_index(fp, img, bc, stride);
    default:
      break;
  }
  return FAILURE;
}

/**
 * @brief BMP形式としてファイルに書き出す。
 *
 * Windows形式（BITMAPINFOHEADER）のBI_RGBでのみ出力
 * COLOR_TYPE_PALETTEの場合は、
 * インデックスカラー形式として書き出す。
 *
 * @param[in] filename 書き出すファイル名
 * @param[in] img      画像データ
 * @param[in] compress TRUEの時RLE圧縮を行う
 * @return 成否
 */
result_t write_bmp_file(const char *filename, image_t *img, int compress) {
  result_t result = FAILURE;
  if (img == NULL) {
    return result;
  }
  FILE *fp = fopen(filename, "wb");
  if (fp == NULL) {
    perror(filename);
    return result;
  }
  result = write_bmp_stream(fp, img, compress);
  fclose(fp);
  return result;
}

/**
 * @brief BMP形式としてファイルに書き出す。
 *
 * Windows形式（BITMAPINFOHEADER）のBI_RGBでのみ出力
 * COLOR_TYPE_PALETTEの場合は、
 * インデックスカラー形式として書き出す。
 *
 * @param[in] fp       書き出すファイルストリームのポインタ
 * @param[in] img      画像データ
 * @param[in] compress TRUEの時RLE圧縮を行う
 * @return 成否
 */
result_t write_bmp_stream(FILE *fp, image_t *img, int compress) {
  result_t result = FAILURE;
  image_t *work = NULL;
  int bc;
  int size;
  if (img == NULL) {
    return FAILURE;
  }
  if (img->color_type == COLOR_TYPE_GRAY) {
    work = clone_image(img);
    if (work == NULL) {
      return FAILURE;
    }
    img = image_gray_to_index(work);
  }
  if (img->color_type == COLOR_TYPE_INDEX) {
    if (img->palette_num <= 2) {
      bc = 1;
    } else if (img->palette_num <= 16) {
      bc = 4;
    } else {
      bc = 8;
    }
  } else if (img->color_type == COLOR_TYPE_RGB) {
    bc = 24;
  } else if (img->color_type == COLOR_TYPE_RGBA) {
    bc = 32;
  } else {
    goto error;
  }
  size = (img->width * bc + 31) / 32 * 4 * img->height;
  if (write_header(fp, img, bc, size, compress) != SUCCESS) {
    goto error;
  }
  if (bc <= 8) {
    if (write_palette(fp, img, bc) != SUCCESS) {
      goto error;
    }
  }
  result = write_bitmap(fp, img, bc, compress);
  error:
  free_image(work);
  return result;
}
