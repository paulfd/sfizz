#!/bin/bash

set -ex
. .travis/environment.sh

if [[ ${CROSS_COMPILE} == "mingw32" || ${CROSS_COMPILE} == "mingw64" ]]; then
  buildenv pacman -Sqy --noconfirm
  buildenv pacman -Sq --noconfirm base-devel wget mingw-w64-cmake mingw-w64-gcc mingw-w64-pkg-config mingw-w64-libsndfile
  buildenv i686-w64-mingw32-gcc -v && buildenv i686-w64-mingw32-g++ -v && buildenv i686-w64-mingw32-cmake --version
elif [[ ${TRAVIS_OS_NAME} == "linux" ]]; then
  sudo apt-get update
  sudo apt-get install libjack-jackd2-dev lv2-dev pkg-config automake gettext libtool-bin cmake
  apt-get source libogg-dev libvorbis-dev libflac-dev libsndfile
  flags="-fPIC -DPIC -fvisibility=hidden"
  pushd libogg-1.3.2
  CFLAGS=$flags CXXFLAGS=$flags ./configure --disable-shared
  make -j$(nproc)
  sudo make install
  popd
  pushd libvorbis-1.3.5
  ./autogen.sh
  CFLAGS=$flags CXXFLAGS=$flags ./configure --disable-shared
  make -j$(nproc)
  sudo make install
  popd
  pushd flac-1.3.2
  ./autogen.sh
  CFLAGS=$flags CXXFLAGS=$flags ./configure --disable-shared
  make -j$(nproc)
  sudo make install
  popd
  pushd libsndfile-1.0.28
  CFLAGS=$flags CXXFLAGS=$flags ./configure --disable-shared --disable-full-suite
  make -j$(nproc)
  sudo make install
  popd
  gcc -v && g++ -v && cmake --version && $SHELL --version
fi
