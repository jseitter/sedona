//
// Licensed under the Academic Free License version 3.0
//
// sys natives that depend on the platform: the clock comes from the
// JavaScript host (svm_now_ns, real or virtual time), and Sys.sleep moves
// that clock forward (svm_sleep_ns) because JavaScript cannot block.
//

#include "sedona.h"
#include "sedonaPlatform.h"
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#include <unistd.h>
#include <string.h>

// Provided by the JavaScript library (svm_lib.js).
extern double svm_now_ns(void);
extern void svm_sleep_ns(double ns);

Cell sys_Sys_platformType(SedonaVM* vm, Cell* params)
{
  Cell result;
  // Apps must use WasmPlatformService: any other platform service would
  // sleep instead of yielding, and the VM would never return to JavaScript.
  result.aval = "platWasm::WasmPlatformService";
  return result;
}

int64_t sys_Sys_ticks(SedonaVM* vm, Cell* params)
{
  return (int64_t)svm_now_ns();
}

Cell sys_Sys_sleep(SedonaVM* vm, Cell* params)
{
  // JavaScript cannot block: the VM clock moves forward instead. A running
  // app never gets here, WasmPlatformService yields every cycle.
  int64_t ns;
  memcpy(&ns, params, sizeof(ns));
  if (ns > 0) svm_sleep_ns((double)ns);
  return nullCell;
}

Cell sys_PlatformService_doPlatformId(SedonaVM* vm, Cell* params)
{
  Cell result;
  result.aval = PLATFORM_ID;
  return result;
}

Cell sys_PlatformService_getPlatVersion(SedonaVM* vm, Cell* params)
{
  Cell result;
  result.aval = PLAT_BUILD_VERSION;
  return result;
}

int64_t sys_PlatformService_getNativeMemAvailable(SedonaVM* vm, Cell* params)
{
  return (int64_t)emscripten_get_heap_max() - (int64_t)(uintptr_t)sbrk(0);
}
