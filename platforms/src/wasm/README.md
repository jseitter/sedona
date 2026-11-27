# Sedona VM for WebAssembly

The Sedona VM compiled with [Emscripten](https://emscripten.org) to an ES
module (`svm.mjs` + `svm.wasm`) that runs in browsers, web workers and
Node. It is meant for simulation: the host decides when each scan cycle
runs, controls the clock, watches slot values and talks Sox to the VM.

## Build

```sh
source adm/unix/init.sh
python3 adm/makedev.py --test none          # or build sedonac and the kits otherwise
bin/sedonac.sh scode/wasm.xml               # image for apps
bin/sedonac.sh scode/wasm-test.xml          # image with the test suite
bin/sedonac.sh apps/wasm.sax                # an example app
EMCC="docker run --rm -v $PWD:/src -w /src emscripten/emsdk:7.0.1 emcc" \
  python3 adm/wasm/makewasmvm.py            # -> bin/svm.mjs, bin/svm.wasm
```

`EMCC` defaults to `emcc` on the PATH.

## Run under Node

```sh
node adm/wasm/runwasm.mjs bin/svm.mjs scode/wasm-test.scode -test
node adm/wasm/runwasm.mjs bin/svm.mjs scode/wasm.scode apps/wasm.sab --cycles 50
```

## What is different from a native VM

- **Platform:** apps must use `platWasm::WasmPlatformService` (`Sys.platformType`
  returns it, so any other platform service fails with `badPlatformService`).
  It yields after every scan cycle: `App.run` returns `Err.yield` and the host
  calls `svm_resume()` for the next cycle.
- **Clock:** `Sys.ticks` is `Module.svmNowNs()` if the host sets it (for a
  virtual clock), else `performance.now()`. `Sys.sleep` cannot block in
  JavaScript, so it moves the VM clock forward; running apps never sleep.
- **Files:** Emscripten's in-memory file system. The host writes the image and
  the app before `svm_start` and can read files (e.g. a saved `app.sab`) after.
- **Network** (`svm_lib.js`): UDP and TCP between sockets of the VM work over
  loopback. UDP datagrams to other addresses go to
  `Module.svmNet.onUdpSend(fromPort, toAddr, toPort, data)`; the host delivers
  datagrams with `Module.svmNet.deliverUdp(toPort, fromAddr, fromPort, data)`.
  Addresses are 16 bytes as in `inet::IpAddr`. TCP to other addresses, and
  multicast, are not available, so the `web` and `serial` kits are not in the
  images.
- **Watches:** `svm_set_watches(comps, slots, n)` sets a list of component and
  slot ids; at the end of every cycle `WasmPlatformService` reads them with
  reflection into the probe buffer at `svm_probe()`: 16 bytes per watch, the
  kind in byte 0 (0 none, 1 int, 2 long, 3 float, 4 double) and the value at
  byte 8.

## API (`main_wasm.c`)

| Function | |
|---|---|
| `int svm_start(const char* scode, const char* arg)` | load the image and run `Sys.main`; `arg` is an app path or `-test`. Returns `253` (yield) after an app's first cycle |
| `int svm_resume(void)` | run the next cycle; `253` again, or the app's exit code |
| `double svm_yield_ns(void)` | nanoseconds until the next cycle is due (negative if late) |
| `int svm_set_watches(const uint16_t*, const uint8_t*, int)` / `void* svm_probe(void)` | watches, see above |
| `int svm_assert_successes(void)`, `int svm_assert_failures(void)` | test results |
| `void svm_stop(void)` | free the VM |
