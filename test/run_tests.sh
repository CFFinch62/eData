#!/usr/bin/env sh
# Host-side tests: NMEA parsing/framing plus the display's pure-logic layer
# (PageLayout compaction, DataCatalog formatting/ETA-TTG). No hardware, no
# PlatformIO, no toolchain beyond g++.
#
#   test/run_tests.sh
#
# Everything compiled here is pure C++ apart from millis(), which
# test/stubs/Arduino.h supplies - that is what makes this possible. Keep it
# that way: pull String, Serial or any other Arduino type into src/nmea/ or
# src/ui/{PageLayout,DataCatalog}.cpp and these tests stop compiling.
set -e
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/edata_host_test"
g++ -std=c++20 -Wall -Wextra -Isrc -Itest/stubs -Itest/nmea_parser \
    test/nmea_parser/*.cpp test/ui/*.cpp src/nmea/*.cpp src/ui/PageLayout.cpp src/ui/DataCatalog.cpp -o "$OUT"
exec "$OUT"
