//
// Copyright (c) 2007 Tridium, Inc.
// Licensed under the Academic Free License version 3.0
//
// History:
//   28 Mar 07  Brian Frank  Creation
//

#include "sedona.h"
#include <time.h>

//////////////////////////////////////////////////////////////////////////
// String Formating
//////////////////////////////////////////////////////////////////////////

// shared buffer
static char strbuf[32];

// Str Sys.intStr(int)
Cell sys_Sys_intStr(SedonaVM* vm, Cell* params)
{
  int32_t i = params[0].ival;
  Cell result;

  sprintf(strbuf, "%d", i);

  result.aval = strbuf;
  return result;
}

// Str Sys.hexStr(int)
Cell sys_Sys_hexStr(SedonaVM* vm, Cell* params)
{
  int32_t i = params[0].ival;
  Cell result;

  sprintf(strbuf, "%x", i);

  result.aval = strbuf;
  return result;
}

// Str Sys.longStr(int)
Cell sys_Sys_longStr(SedonaVM* vm, Cell* params)
{
  int64_t i = *(int64_t*)params;
  Cell result;

#ifdef _WIN32
  sprintf(strbuf, "%I64d", i);
#else
  sprintf(strbuf, "%lld", i);
#endif

  result.aval = strbuf;
  return result;
}

// Str Sys.longHexStr(int)
Cell sys_Sys_longHexStr(SedonaVM* vm, Cell* params)
{         
  int64_t i = *(int64_t*)params;  
  Cell result;

#ifdef _WIN32
  sprintf(strbuf, "%I64x", i);
#else
  sprintf(strbuf, "%llx", i);
#endif

  result.aval = strbuf;
  return result;
}

// Str Sys.floatStr(float)
Cell sys_Sys_floatStr(SedonaVM* vm, Cell* params)
{
  float f = params[0].fval;
  Cell result;

  sprintf(strbuf, "%f", f);

  result.aval = strbuf;
  return result;
}

// Str Sys.doubleStr(float)
Cell sys_Sys_doubleStr(SedonaVM* vm, Cell* params)
{
  double d = *(double*)params;
  Cell result;      

  sprintf(strbuf, "%lf", d);

  result.aval = strbuf;
  return result;
}

//////////////////////////////////////////////////////////////////////////
// Utils
//////////////////////////////////////////////////////////////////////////

// Fill the buffer with random bytes from the operating system.
// Return zero if it has no source of them.
static int osRandom(void* buf, size_t len)
{
#if defined(__UNIX__)
  static FILE* urandom = NULL;
  if (urandom == NULL)
  {
    urandom = fopen("/dev/urandom", "rb");
    if (urandom == NULL) return 0;
    setvbuf(urandom, NULL, _IONBF, 0);
  }
  return fread(buf, 1, len, urandom) == len;
#else
  return 0;
#endif
}

// int Sys.rand()
//
// The numbers are used for nonces and session ids, so they must not
// repeat from one start of the VM to the next.  Use the random source
// of the operating system where there is one.  Otherwise fall back to
// rand(), seeded once from the clock: rand() without a seed returns
// the same sequence on every start.
Cell sys_Sys_rand(SedonaVM* vm, Cell* params)
{
  static int seeded = 0;
  Cell result;
  int32_t r;

  if (osRandom(&r, sizeof(r)))
  {
    result.ival = r;
    return result;
  }

  if (!seeded)
  {
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    seeded = 1;
  }

  // rand() may only have 15 random bits
  r = (rand() & 0x7fff) | ((rand() & 0x7fff) << 15) | ((rand() & 0x3) << 30);
  result.ival = r;
  return result;
}


