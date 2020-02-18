#!/bin/bash

set -ex
. .travis/environment.sh

mkdir -p build/${INSTALL_DIR} && cd build

if [[ ${STATIC_LV2} == "true" ]]; then
  mkdir -p deps/vcpkg
  pushd deps/vcpkg
  git init
  git remote add origin https://github.com/Microsoft/vcpkg.git
  git fetch origin master
  git checkout -b master origin/master
  ./bootstrap-vcpkg.sh -disableMetrics
  popd

  touch linux-hidden.cmake
  if [[ ${TRAVIS_CPU_ARCH} == "amd64" ]]; then
    echo -e '\nset(VCPKG_TARGET_ARCHITECTURE x64)' >> linux-hidden.cmake
  elif [[ ${TRAVIS_CPU_ARCH} == "arm64" ]]; then
    echo -e '\nset(VCPKG_TARGET_ARCHITECTURE arm64)' >> linux-hidden.cmake
  fi
  echo -e '\nset(VCPKG_BUILD_TYPE release)' >> linux-hidden.cmake
  echo -e '\nset(VCPKG_CRT_LINKAGE static)' >> linux-hidden.cmake
  echo -e '\nset(VCPKG_LIBRARY_LINKAGE static)' >> linux-hidden.cmake
  echo -e '\nset(VCPKG_CMAKE_SYSTEM_NAME Linux)' >> linux-hidden.cmake
  echo -e '\nset(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -fvisibility=hidden")' >> linux-hidden.cmake
  echo -e '\nset(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} -fvisibility=hidden")' >> linux-hidden.cmake
  deps/vcpkg/vcpkg --overlay-triplets=. install libsndfile:linux-hidden
  cmake .. -DSFIZZ_JACK=OFF -DSFIZZ_SHARED=OFF -DCMAKE_INSTALL_PREFIX:PATH=${PWD}/${INSTALL_DIR} "-DCMAKE_TOOLCHAIN_FILE=deps/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=linux-hidden -DCMAKE_BUILD_TYPE=Release -DSFIZZ_USE_VCPKG=ON
  make -j$(nproc) sfizz_lv2
elif [[ ${CROSS_COMPILE} == "mingw32" ]]; then

  buildenv i686-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${PWD}/${INSTALL_DIR} -DSFIZZ_JACK=OFF ..
  buildenv make -j
elif [[ ${CROSS_COMPILE} == "mingw64" ]]; then

  buildenv x86_64-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${PWD}/${INSTALL_DIR} -DSFIZZ_JACK=OFF ..
  buildenv make -j
elif [[ ${TRAVIS_OS_NAME} == "linux" ]]; then

  buildenv cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${PWD}/${INSTALL_DIR} ..
  buildenv make -j$(nproc)
elif [[ ${TRAVIS_OS_NAME} == "osx" ]]; then

  buildenv cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${PWD}/${INSTALL_DIR} ..
  buildenv make -j$(sysctl -n hw.ncpu)

# Xcode not currently supported, see https://gitlab.kitware.com/cmake/cmake/issues/18088
# xcodebuild -project sfizz.xcodeproj -alltargets -configuration Debug build
fi
