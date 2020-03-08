#!/bin/sh

clang-tidy \
  src/sfizz/ADSREnvelope.cpp \
  src/sfizz/Effects.cpp \
  src/sfizz/EQPool.cpp \
  src/sfizz/EventEnvelopes.cpp \
  src/sfizz/FilePool.cpp \
  src/sfizz/FilterPool.cpp \
  src/sfizz/FloatEnvelopes.cpp \
  src/sfizz/Logger.cpp \
  src/sfizz/MidiState.cpp \
  src/sfizz/Opcode.cpp \
  src/sfizz/Oversampler.cpp \
  src/sfizz/Parser.cpp \
  src/sfizz/sfizz.cpp \
  src/sfizz/Region.cpp \
  src/sfizz/SfzHelpers.cpp \
  src/sfizz/SIMDSSE.cpp \
  src/sfizz/Synth.cpp \
  src/sfizz/Voice.cpp \
  src/sfizz/effects/Lofi.cpp \
  src/sfizz/effects/Nothing.cpp \
  -- -Iexternal/abseil-cpp -Isrc/external -Isrc/external/pugixml/src -Isrc/sfizz -Isrc
