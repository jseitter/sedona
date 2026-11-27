//
// Licensed under the Academic Free License version 3.0
//
// platWasm::WasmPlatformService natives: the yield time and the watch
// probe. The host sets a watch list (component id, slot id) with
// svm_set_watches; at the end of every cycle the service writes each
// watched value into the probe buffer, which the host reads after
// svm_resume returns.
//

#include "sedona.h"
#include <stdlib.h>
#include <string.h>
#include <emscripten/emscripten.h>

extern int64_t yieldNs;

#define MAX_WATCHES 1024

// Probe entry kinds.
#define PROBE_NONE   0
#define PROBE_INT    1
#define PROBE_LONG   2
#define PROBE_FLOAT  3
#define PROBE_DOUBLE 4

// One entry per watch: kind in the first byte, the value at offset 8.
typedef struct
{
  uint8_t kind;
  uint8_t pad[7];
  union { int32_t i; int64_t l; float f; double d; } v;
} ProbeEntry;

static int      watchN = 0;
static uint16_t watchComps[MAX_WATCHES];
static uint8_t  watchSlots[MAX_WATCHES];
static ProbeEntry probe[MAX_WATCHES];

EMSCRIPTEN_KEEPALIVE
int svm_set_watches(const uint16_t* comps, const uint8_t* slots, int n)
{
  if (n < 0 || n > MAX_WATCHES) return -1;
  memcpy(watchComps, comps, n * sizeof(uint16_t));
  memcpy(watchSlots, slots, n * sizeof(uint8_t));
  memset(probe, 0, sizeof(probe));
  watchN = n;
  return 0;
}

EMSCRIPTEN_KEEPALIVE
ProbeEntry* svm_probe(void)
{
  return probe;
}

Cell platWasm_WasmPlatformService_doYield(SedonaVM* vm, Cell* params)
{
  yieldNs = *(int64_t*)params;
  return nullCell;
}

Cell platWasm_WasmPlatformService_watchCount(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.ival = watchN;
  return r;
}

Cell platWasm_WasmPlatformService_watchComp(SedonaVM* vm, Cell* params)
{
  Cell r;
  int i = params[0].ival;
  r.ival = (i >= 0 && i < watchN) ? watchComps[i] : 0xffff;
  return r;
}

Cell platWasm_WasmPlatformService_watchSlot(SedonaVM* vm, Cell* params)
{
  Cell r;
  int i = params[0].ival;
  r.ival = (i >= 0 && i < watchN) ? watchSlots[i] : 0xff;
  return r;
}

static ProbeEntry* entry(Cell* params)
{
  int i = params[0].ival;
  return (i >= 0 && i < watchN) ? &probe[i] : NULL;
}

Cell platWasm_WasmPlatformService_reportNone(SedonaVM* vm, Cell* params)
{
  ProbeEntry* e = entry(params);
  if (e) e->kind = PROBE_NONE;
  return nullCell;
}

Cell platWasm_WasmPlatformService_reportInt(SedonaVM* vm, Cell* params)
{
  ProbeEntry* e = entry(params);
  if (e) { e->kind = PROBE_INT; e->v.i = params[1].ival; }
  return nullCell;
}

Cell platWasm_WasmPlatformService_reportLong(SedonaVM* vm, Cell* params)
{
  ProbeEntry* e = entry(params);
  int64_t v;
  memcpy(&v, params + 1, sizeof(v));
  if (e) { e->kind = PROBE_LONG; e->v.l = v; }
  return nullCell;
}

Cell platWasm_WasmPlatformService_reportFloat(SedonaVM* vm, Cell* params)
{
  ProbeEntry* e = entry(params);
  if (e) { e->kind = PROBE_FLOAT; e->v.f = params[1].fval; }
  return nullCell;
}

Cell platWasm_WasmPlatformService_reportDouble(SedonaVM* vm, Cell* params)
{
  ProbeEntry* e = entry(params);
  double v;
  memcpy(&v, params + 1, sizeof(v));
  if (e) { e->kind = PROBE_DOUBLE; e->v.d = v; }
  return nullCell;
}
