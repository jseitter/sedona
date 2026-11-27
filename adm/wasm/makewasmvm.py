#! /usr/bin/env python3
#
# makewasmvm.py
#
#   Compile the Sedona VM to WebAssembly with Emscripten.
#
#     makewasmvm.py [-o <dir>]
#
#   1. sedonac stages platforms/src/wasm/wasm.xml (VM, natives and the
#      native table) into build/temp/wasm;
#   2. emcc compiles it into <dir>/svm.mjs and <dir>/svm.wasm (default bin/):
#      an ES module whose default export createSvm(options) instantiates
#      the VM (see platforms/src/wasm/native/main_wasm.c for its API).
#
#   The kits, including platWasm, must be compiled first (makedev.py or
#   compilekit on src/kits). The emcc command comes from $EMCC (default
#   "emcc"); it runs in the sedona home with relative paths, so a
#   container works as well:
#
#     EMCC="docker run --rm -v $sedona_home:/src -w /src emscripten/emsdk:7.0.1 emcc"
#

from __future__ import print_function
import argparse
import glob
import os
import shlex
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
if not os.environ.get("sedona_home"):
  os.environ["sedona_home"] = os.path.dirname(os.path.dirname(here))
sys.path.insert(0, os.path.dirname(here))

import env
import compilekit
import fileutil

def main():
  parser = argparse.ArgumentParser(description="Compile the Sedona VM to WebAssembly")
  parser.add_argument("-o", "--out", default="bin", help="output directory, relative to the sedona home (default bin)")
  args = parser.parse_args()

  home = env.home
  platFile = os.path.join("platforms", "src", "wasm", "wasm.xml")
  stage = os.path.join("build", "temp", "wasm")
  os.chdir(home)

  # 1. stage
  fileutil.rmdir(stage)
  compilekit.compile(os.path.join(home, platFile) + " -outDir " + os.path.join(home, stage))

  # 2. compile
  if not os.path.isdir(args.out):
    os.makedirs(args.out)
  emcc = shlex.split(os.environ.get("EMCC", "emcc"))
  cmd = emcc + [
    "-O2",
    "-fno-strict-aliasing",
    "-DSEDONA_LOCAL_H",
    "-DSEDONA_REF_SIZE=4",
    '-DPLAT_BUILD_VERSION="' + env.buildVersion() + '"',
    "-I" + stage,
    "-sMODULARIZE=1",
    "-sEXPORT_ES6=1",
    "-sEXPORT_NAME=createSvm",
    "-sENVIRONMENT=web,worker,node",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sFORCE_FILESYSTEM=1",
    "-sEXPORTED_RUNTIME_METHODS=FS,HEAPU8,HEAP32,ccall,cwrap,UTF8ToString",
    "-sEXPORTED_FUNCTIONS=_malloc,_free",
    "--js-library", os.path.join("platforms", "src", "wasm", "svm_lib.js"),
    "-o", os.path.join(args.out, "svm.mjs"),
  ] + sorted(glob.glob(os.path.join(stage, "*.c")))

  print(" ".join(shlex.quote(c) for c in cmd))
  status = subprocess.call(cmd)
  if status != 0:
    raise env.BuildError("FATAL: emcc failed")
  print("Built " + os.path.join(args.out, "svm.mjs") + " and svm.wasm")
  return 0

if __name__ == "__main__":
  try:
    sys.exit(main())
  except env.BuildError as err:
    print("**")
    print("** " + str(err.value))
    print("** FAILED")
    print("**")
    sys.exit(1)
