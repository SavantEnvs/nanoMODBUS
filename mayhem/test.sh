#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

fail=0
pass=0

# 1) Upstream's assertion suite: any failed expect() aborts it; every passing check prints a "Should ..." line.
[ -x build-tests/nanomodbus_tests ] || { echo "build-tests/nanomodbus_tests missing (build.sh bug)" >&2; emit_ctrf "nanomodbus_tests" 0 1 0; exit 1; }
out="$(timeout 300 build-tests/nanomodbus_tests 2>&1)"; rc=$?
echo "$out" | tail -5
checks=$(printf '%s\n' "$out" | grep -c '^[[:space:]]*Should ' || true)
if [ "$rc" -eq 0 ] && [ "$checks" -ge 100 ]; then
  pass=$((pass + checks))
else
  echo "FAIL nanomodbus_tests: rc=$rc, $checks 'Should' checks reported (expected >= 100)" >&2
  fail=$((fail + 1))
  pass=$((pass + checks))
fi

# 2) Known-answer probe: Modbus frames with independently derived responses (RTU CRCs computed by hand/python).
[ -x build-tests/kat ] || { echo "build-tests/kat missing (build.sh bug)" >&2; emit_ctrf "nanomodbus_tests" "$pass" $((fail + 1)) 0; exit 1; }
declare -A want=(
  [srv-tcp-read-holding]="00010000000701030414361537"
  [srv-tcp-write-then-read]="00020000000601060005beef000300000005010302beef"
  [srv-tcp-exception]="000400000003018302"
  [srv-tcp-read-coils]="00050000000401010105"
  [srv-tcp-read-input]="000600000005010402abcd"
  [srv-tcp-write-multiple-then-read]="00070000000601100010000200080000000701030400010002"
  [srv-rtu-read-holding]="0103021234b533"
  [srv-rtu-write-single]="01060005beefa9e7"
  [srv-rtu-bad-crc]="none"
  [srv-rtu-other-unit]="none"
  [cli-tcp-read-holding]="req=000100000006010300020002 err=0 regs=1436,1537"
  [cli-rtu-read-holding]="req=010300000001840a err=0 regs=1234,0000"
  [cli-rtu-bad-crc]="req=010300000001840a err=-5"
  [cli-tcp-exception]="req=000100000006010302580001 err=2"
)
declare -A got=()
while IFS= read -r line; do
  [[ "$line" == *": "* ]] && got["${line%%: *}"]="${line#*: }"
done < <(timeout 60 build-tests/kat 2>&1)
for k in "${!want[@]}"; do
  if [ "${got[$k]-}" == "${want[$k]}" ]; then
    pass=$((pass + 1))
  else
    echo "FAIL kat $k: got '${got[$k]-<missing>}' want '${want[$k]}'" >&2
    fail=$((fail + 1))
  fi
done

# 3) Upstream's feature-split ctests (test_server_disabled, test_client_disabled): built with -DNMBS_SERVER_DISABLED /
#    -DNMBS_CLIENT_DISABLED, each creates the other side's context and exits non-zero on error. One test each.
#    Upstream's fourth ctest, test_multi_server_rtu, is deliberately NOT run: it asserts nothing. Its checks compare
#    a buffer with itself (memcmp(coils, coils, ...) at tests/multi_server_rtu.c:182 and :202), errors only go to
#    stderr, and main always returns 0 (:212), so it cannot fail and would only inflate the pass count.
for t in server_disabled client_disabled; do
  if [ ! -x "build-tests/$t" ]; then
    echo "FAIL $t: build-tests/$t missing (build.sh bug)" >&2
    fail=$((fail + 1))
  elif timeout 60 "build-tests/$t"; then
    pass=$((pass + 1))
  else
    echo "FAIL $t: rc=$?" >&2
    fail=$((fail + 1))
  fi
done

emit_ctrf "nanomodbus_tests+kat+ctest-disabled" "$pass" "$fail" 0
