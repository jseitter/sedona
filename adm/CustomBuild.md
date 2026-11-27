# Customize target platform

It's possible to cross-compile sedona to new target platform and add new kits with even native methods without any changes to upstream platform. It's possible to overwrite even existing kits by own version.

For example, following directory structure exists:

```
<some dir>
    upstream-sedona
        adm
        apps
        bin
        build
        ...
        src
        tools

    new-target-platform
        apps
            defaultApp.sax
        platforms
            db
            src
                generic
                    unix
                        generic-unix.xml
                        ...
        scode
            defaultKits.xml
        src
            kits
                newKit
                newNativeKit
                serial
                sys
                dir.xml
```

Following commands allows to build sedona for new target platform

```
$ export SVM_CFLAGS=-DFANCY_FEATURE_EN=1
$ export SVM_LDFLAGS='-lm -pthread'
$ export SVM_CROSS_COMPILE=arm-linux-gnueabihf-
$ upstream-sedona/adm/makedev.py -t none -p new-target-platform/platforms/src/generic/unix/generic-unix.xml -k new-target-platform/src/kits/dir.xml -s new-target-platform/scode/defaultKits.xml -a new-target-platform/apps/defaultApp.sax
```

The VM is compiled with stack canaries and checked libc functions (`-fstack-protector-strong -D_FORTIFY_SOURCE=2`), and on Linux linked as a position independent executable with a read-only relocation table and a non-executable stack. This is the default for a native build only: a cross compiler may be too old for these options or lack the runtime support for them. Set `SVM_HARDENING=1` to use them with `SVM_CROSS_COMPILE` as well, or `SVM_HARDENING=0` to build without them.

There are some things that have to be done to make this directory structure to work. Most of them covers usage of relative paths to source files.

    In platform description 'generic-unix.xml' paths to source code for native kits should be given relative to 'upstream-sedona' directory.

```
<sedonaPlatform...
    ...
    <nativeSource path="/src/kits/inet/native/sha1" />
    <nativeSource path="/src/kits/datetimeStd/native/std" />
    
    <nativeSource path="/../new-target-platform/src/kits/serial/native/linux" />
    <nativeSource path="/../new-target-platform/src/kits/serial/native" />
    ...
</sedonaPlatform>
```
    To build external kits new '-k' option of makedev.py is used. It points to XML with description of all additional kits, including platform kit.

<sedonaDir>
     <!-- overwritten kits -->
     <target name="sys"/>
     <target name="serial" />

     <!-- new kits -->
     <target name="newKit" />
     <target name="newNativeKit" />

     <!-- platform kits -->
     <target name="../../platforms/src/generic/unix" />

</sedonaDir>

All build results are located in 'upstream-sedona/build' and 'upstream-sedona/bin' directories and with external shell scripts it's possible to do with results whatever is needed: build release support package, move to place for firmware generation, run built runtime on development machine and so on.


