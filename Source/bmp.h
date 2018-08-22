// Bitmap File Format (.bmp)

#ifndef BMP_H_
#define BMP_H_

#include "memory.h"

bool bmp_write_file(const char* path, const uint8_t* pixels, int width, int height, Stack* stack);

#endif // BMP_H_
