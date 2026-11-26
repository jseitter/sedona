# Sedona [![Build Status](https://travis-ci.org/linsong/sedona.svg?branch=master)](https://travis-ci.org/linsong/sedona) [![Join the chat at https://gitter.im/linsong/sedona](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/linsong/sedona?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)



The Sedona framework is designed to make it easy to build smart, networked embedded devices. Read [Documentation](https://linsong.github.io/sedona/site/index.html).

This is a fork of the [official Sedona framework](http://www.sedonadev.org), we call it as **Sedona community branch**. Here's why we decided to fork:
* We love the Sedona framework and want it to become better and better, but there is no updates in years in the [official branch](http://www.sedonadev.org). So we make a branch here for everyone in the community to contribute; and at the same time, the whole community can get benifits from these contributions. **From the community, to the community**.
* We love git over Mercurial version control system, so we chose github :)

We use [Academic Free License("AFL")](https://linsong.github.io/sedona/doc/license.html), the same as the official Sedona framework. You can visit the official site for more details: https://linsong.github.io/sedona (The official homesite http://www.sedonadev.org is down for a while, seems it is not maintained anymore).


## Major Improvements/Features
* Remove jikes dependency for POSIX OS platforms
* Wireshark protocol dissector for Sox/Dasp [More Info](./tools/README.md)
* Linux Docker image [More Info](./tools/README.md)
* Mac OS X build support
* Testing in non-Windows environment

See all changes in the [ChangeLog](ChangeLog.md).

## How To Build
### Mac OS X 
1. make sure Xcode command line tools installed. you can run following command in terminal to install it:
```
 $ xcode-select --install
```
2. in terminal window, go to sedona's folder and initialize the build environment:
```
 $ cd /path/to/sedona/src && source adm/unix/init.sh 
```
3. in terminal window, start to build sedona zipball:
```
 $ ./adm/makedist.py
```

### GNU/Linux
We build a shell script that uses docker to build sedona under linux, read [here](./tools/README.md) for more details.

### Customize build platform
If it's necessary to cross-compile sedona to custom platform with own set of additional or modified kits, please follow instructions described [here](./adm/CustomBuild.md).

## How To Test
`makedev.py` builds everything and runs both test suites: the `sedonac` tests (compiler, Sox, Dasp) and the `svm` tests (the kits, run inside the Sedona VM):
```
 $ source adm/unix/init.sh
 $ ./adm/makedev.py
```
To get a result for every single test, run a suite through `testreport.py`. It writes the console output and a JUnit XML file to `build/test-results`, and fails if a test failed, crashed or hung:
```
 $ ./adm/makedev.py --test none
 $ ./adm/testreport.py --suite sedonac -- bin/sedonac.sh -test
 $ ./adm/testreport.py --suite svm -- bin/svm scode/x86-test.scode -test
```
Use `scode/unix64-test.scode` for a 64-bit VM, which is what `makedev.py` builds on macOS and on ARM64 Linux.

GitHub Actions runs the same steps for every push and pull request, see [ci.yml](.github/workflows/ci.yml): a 32-bit VM on x86-64 Linux, and a 64-bit VM on x86-64 Linux, ARM64 Linux and macOS. The summary page of a run lists the results, and the logs and JUnit files are attached to it as artifacts.

## Contribution
Contribution is always welcomed!  If you want to contribute(add new feature, fix bugs etc), please create an issue first so that if others are interested, related discussion can happen there. At last, please avoid sending huge pull request.

The *master* branch is used as base branch for development, if you want to contribute, branch from here.

 *stable* branch is used for stable features and for non-developer users, if you don't want to try bleeding edge features, you should use codes from this branch. *master* branch features will be merged to *stable* branch periodically before every release. 

Happy Hacking! :-)


