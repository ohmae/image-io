/**
 * @file main.c
 *
 * Copyright (c) 2015 大前良介 (OHMAE Ryosuke)
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/MIT
 *
 * @brief テスト用メイン関数
 * @author <a href="mailto:ryo@mm2d.net">大前良介 (OHMAE Ryosuke)</a>
 * @date 2015/02/08
 */
#include <stdio.h>
#include <string.h>
#include "image.h"

static char *get_extension(char *name) {
  int i;
  for (i = strlen(name) - 1; i >= 0; i--) {
    if (name[i] == '.') {
      return &name[i + 1];
    }
  }
  return NULL;
}

int main(int argc, char**argv) {
  int i;
  char *name;
  char *ext;
  char outname[64];
  image_t *img = NULL;
  image_t *b = NULL;
  for (i = 1; i < argc; i++) {
    name = argv[i];
    ext = get_extension(name);
    if (ext == NULL) {
      continue;
    }
    if (strcmp("bmp", ext) == 0) {
      img = read_bmp_file(name);
    } else if (strcmp("jpg", ext) == 0 || strcmp("jpeg", ext) == 0) {
      img = read_jpeg_file(name);
    } else if (strcmp("png", ext) == 0) {
      img = read_png_file(name);
    } else if (strcmp("ppm", ext) == 0 || strcmp("pbm", ext) == 0
        || strcmp("pgm", ext) == 0) {
      img = read_pnm_file(name);
    }
    if (img != NULL) {
      LOG("%s", name);
      dump_image_info(img);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".a.pbm");
      write_pnm_file(outname, img, 1);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".a.pgm");
      write_pnm_file(outname, img, 2);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".a.ppm");
      write_pnm_file(outname, img, 3);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".b.pbm");
      write_pnm_file(outname, img, 4);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".b.pgm");
      write_pnm_file(outname, img, 5);
      strcpy(outname, "out/0-");
      strcat(outname, name);
      strcat(outname, ".b.ppm");
      write_pnm_file(outname, img, 6);
      img = image_to_rgba(img);
      strcpy(outname, "out/a-");
      strcat(outname, name);
      strcat(outname, ".png");
      write_png_file(outname, img);
      img = image_to_rgb(img);
      strcpy(outname, "out/b-");
      strcat(outname, name);
      strcat(outname, ".png");
      write_png_file(outname, img);
      strcpy(outname, "out/b-");
      strcat(outname, name);
      strcat(outname, ".jpg");
      write_jpeg_file(outname, img);
      strcpy(outname, "out/b-");
      strcat(outname, name);
      strcat(outname, ".bmp");
      write_bmp_file(outname, img, FALSE);
      strcpy(outname, "out/simple-");
      strcat(outname, name);
      strcat(outname, ".bmp");
      write_bmp_simple_file(outname, img);
      b = img;
      img = image_to_index(img);
      if (img != NULL) {
        strcpy(outname, "out/c-");
        strcat(outname, name);
        strcat(outname, ".png");
        write_png_file(outname, img);
        strcpy(outname, "out/c-");
        strcat(outname, name);
        strcat(outname, ".bmp");
        write_bmp_file(outname, img, FALSE);
      }
      img = image_to_gray(b);
      strcpy(outname, "out/d-");
      strcat(outname, name);
      strcat(outname, ".png");
      write_png_file(outname, img);
      free_image(img);
      img = NULL;
    } else {
      printf("read fail %s\n", name);
    }
  }
  return 0;
}
