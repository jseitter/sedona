//
// Licensed under the Academic Free License version 3.0
//
// Platform definitions for the Sedona VM compiled to WebAssembly
// (included by sedona.h when SEDONA_LOCAL_H is defined). Like __UNIX__,
// but without USE_STANDARD_MAIN: main_wasm.c provides the entry points.
//

#ifndef SEDONA_LOCAL_WASM_H
#define SEDONA_LOCAL_WASM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

#define SCODE_DEBUG
#define IS_LITTLE_ENDIAN

#define SCODE_BLOCK_SIZE 4
#define block2addr(cb, block) ((cb) + (block<<2))
#define _chdir chdir

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

#define ISNANF(f) (isnan(f))
#define ISNAN(d)  (isnan(d))

#endif
