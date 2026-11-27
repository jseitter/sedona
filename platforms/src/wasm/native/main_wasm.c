//
// Licensed under the Academic Free License version 3.0
//
// Entry points of the Sedona VM compiled to WebAssembly. They replace
// main() of src/native/vm/main.c, which is compiled out because
// USE_STANDARD_MAIN is not defined.
//
// The JavaScript host writes the scode image and the app into
// Emscripten's in-memory file system, then
//
//   svm_start(scodePath, arg)   runs Sys.main: arg is the .sab path for an
//                               app, or "-test" for the test suite. An app
//                               returns ERR_YIELD after its first cycle.
//   svm_resume()                runs the next cycle; ERR_YIELD again, or the
//                               app's exit code.
//   svm_yield_ns()              time until the next cycle is due, as passed
//                               to WasmPlatformService.yield.
//

#include "sedona.h"
#include "errorcodes.h"
#include <stdio.h>
#include <stdlib.h>
#include <emscripten/emscripten.h>

extern NativeMethod* nativeTable[];

int64_t yieldNs = 0;

static SedonaVM vm;
static const char* vmArgs[1];

static void onAssertFailure(const char* location, uint16_t linenum)
{
  printf("ASSERT FAILURE: %s [Line %d]\n", location, linenum);
}

static int loadFile(const char* filename, uint8_t** paddr, size_t* psize)
{
  FILE* file = fopen(filename, "rb");
  long size;
  uint8_t* addr;
  if (file == NULL) return ERR_INPUT_FILE_NOT_FOUND;
  if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return ERR_CANNOT_READ_INPUT_FILE; }
  size = ftell(file);
  rewind(file);
  addr = (uint8_t*)malloc(size);
  if (addr == NULL) { fclose(file); return ERR_MALLOC_IMAGE; }
  if (fread(addr, 1, size, file) != (size_t)size)
  {
    fclose(file);
    free(addr);
    return ERR_CANNOT_READ_INPUT_FILE;
  }
  fclose(file);
  *paddr = addr;
  *psize = size;
  return 0;
}

static void freeVm(void)
{
  if (vm.codeBaseAddr != NULL) free((void*)vm.codeBaseAddr);
  if (vm.stackBaseAddr != NULL) free((void*)vm.stackBaseAddr);
  memset((void*)&vm, 0, sizeof(SedonaVM));
}

EMSCRIPTEN_KEEPALIVE
int svm_start(const char* scodePath, const char* arg)
{
  int result;
  freeVm();
  yieldNs = 0;

  result = loadFile(scodePath, (uint8_t**)&vm.codeBaseAddr, &vm.codeSize);
  if (result != 0)
  {
    printf("Cannot load input file (%d): %s\n", result, scodePath);
    return result;
  }

  vm.stackMaxSize  = 4096 * sizeof(Cell);
  vm.stackBaseAddr = (uint8_t*)malloc(vm.stackMaxSize);
  if (vm.stackBaseAddr == NULL) return ERR_MALLOC_STACK;

  vm.onAssertFailure = onAssertFailure;
  vm.nativeTable     = nativeTable;
  vm.call            = vmCall;
  vmArgs[0]          = arg;
  vm.args            = vmArgs;
  vm.argsLen         = arg != NULL ? 1 : 0;

  return vmRun(&vm);
}

EMSCRIPTEN_KEEPALIVE
int svm_resume(void)
{
  yieldNs = 0;
  return vmResume(&vm);
}

EMSCRIPTEN_KEEPALIVE
double svm_yield_ns(void)
{
  return (double)yieldNs;
}

EMSCRIPTEN_KEEPALIVE
int svm_assert_successes(void) { return vm.assertSuccesses; }

EMSCRIPTEN_KEEPALIVE
int svm_assert_failures(void) { return vm.assertFailures; }

EMSCRIPTEN_KEEPALIVE
void svm_stop(void)
{
  freeVm();
}
