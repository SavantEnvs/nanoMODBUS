#!/usr/bin/env bash
#
# mayhem/build.sh — builds the two libFuzzer harnesses (+ standalone reproducers) and the test oracle.
# Runs in the commit image as `mayhem` in /mayhem; idempotent and air-gapped.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
# Alignment is the only UBSan check relaxed: nanomodbus.c casts odd offsets of its byte buffer to uint16_t* in the
# file-record handlers (see mayhem/fuzz_server/known-findings/), which would abort every FC 20/21 input and starve
# those paths of coverage. ASan and every other UBSan check stay fatal. Appended unconditionally: the base ENV
# already sets SANITIZER_FLAGS, so an assign-if-unset default would never run.
SANITIZER_FLAGS="$SANITIZER_FLAGS -fno-sanitize=alignment"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
rm -rf build-fuzz build-tests
mkdir -p build-fuzz build-tests

# 1) The library, instrumented (sanitizers + SanCov coverage) and with DWARF <= 3.
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link -I"$SRC" \
    -c "$SRC/nanomodbus.c" -o build-fuzz/nanomodbus.o

# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o build-fuzz/lsan_off.o

# 2) Each harness twice: libFuzzer binary and a run-once standalone reproducer.
for t in fuzz_server fuzz_client; do
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link $LIB_FUZZING_ENGINE \
        -I"$SRC" -I"$SRC/mayhem" "$SRC/mayhem/$t.c" build-fuzz/nanomodbus.o build-fuzz/lsan_off.o \
        -o "$SRC/$t"
    # shellcheck disable=SC2086
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link \
        -I"$SRC" -I"$SRC/mayhem" "$STANDALONE_FUZZ_MAIN" "$SRC/mayhem/$t.c" \
        build-fuzz/nanomodbus.o build-fuzz/lsan_off.o -o "$SRC/$t-standalone"
done

# 3) The oracle: upstream's own assertion suite and a known-answer probe, with normal flags.
# shellcheck disable=SC2086
$CC -O1 $COVERAGE_FLAGS -I"$SRC" -I"$SRC/tests" "$SRC/nanomodbus.c" "$SRC/tests/nanomodbus_tests.c" \
    -lpthread $COVERAGE_FLAGS -o build-tests/nanomodbus_tests
# shellcheck disable=SC2086
$CC -O1 $COVERAGE_FLAGS -I"$SRC" -I"$SRC/mayhem" "$SRC/nanomodbus.c" "$SRC/mayhem/kat.c" \
    $COVERAGE_FLAGS -o build-tests/kat
# Upstream's two feature-split ctests (test_server_disabled / test_client_disabled): each compiles the library with
# one side #ifdef'd out and creates the other side's context, so a patch that breaks the
# NMBS_SERVER_DISABLED / NMBS_CLIENT_DISABLED split fails to build here.
# shellcheck disable=SC2086
$CC -O1 $COVERAGE_FLAGS -DNMBS_SERVER_DISABLED -I"$SRC" "$SRC/nanomodbus.c" "$SRC/tests/server_disabled.c" \
    $COVERAGE_FLAGS -o build-tests/server_disabled
# shellcheck disable=SC2086
$CC -O1 $COVERAGE_FLAGS -DNMBS_CLIENT_DISABLED -I"$SRC" "$SRC/nanomodbus.c" "$SRC/tests/client_disabled.c" \
    $COVERAGE_FLAGS -o build-tests/client_disabled
