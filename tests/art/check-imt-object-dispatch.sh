#!/bin/bash
# Interface calls to java.lang.Object methods must dispatch on the receiver.
# ART's optimizing compiler emitted an IMT-slot call for them, and the
# conflict trampoline cached the first resolution in the runtime's shared
# "unimplemented" IMT method, so every later receiver of any class ran the
# first class's toString/hashCode/equals. Google Camera died of it on every
# frame-drop log line (GCAM_HANDOVER.md, 2026-09-06).
#
# Runs the test on the installed art_standalone dalvikvm, which both AOT
# compiles the jar (dex2oat off PATH) and JIT compiles the hot method, so a
# bad compiler fails it either way. ART_TREE=<art_standalone/out/host/linux-x86>
# tests a freshly built ART instead of the installed one: its bin/ goes first
# on PATH (dalvikvm and the dex2oat it spawns) and lib64/ on LD_LIBRARY_PATH,
# with the installed jars as the boot class path. The boot image is then
# regenerated in ~/.cache/art by the new compiler, which takes a minute.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${ART_TEST_OUT:-${BUILDDIR:-$REPO/builddir}/tests-art}"
command -v dalvikvm >/dev/null || { echo "dalvikvm (art_standalone) is required" >&2; exit 1; }
command -v dx >/dev/null || { echo "dx is required" >&2; exit 1; }

mkdir -p "$OUT/classes-imt"
javac --release 8 -nowarn -d "$OUT/classes-imt" "$REPO/tests/art/ImtObjectDispatchTest.java"
dx --dex --output="$OUT/imt.jar" "$OUT/classes-imt"

# ART's own jars, core-oj first, and not ATL's boot class path: this is about
# the compiler, and the fork's dalvikvm throws on an unset BOOTCLASSPATH.
ART_JARS="$(dirname "$(dirname "$(readlink -f "$(command -v dalvikvm)")")")/lib/java/dex/art"
CORE_OJ="$(ls "$ART_JARS"/core-oj*.jar 2>/dev/null | head -1)"
[ -n "$CORE_OJ" ] || { echo "no ART boot jars in $ART_JARS" >&2; exit 1; }
BOOTCLASSPATH="$CORE_OJ"
for j in $(ls "$ART_JARS"/*.jar | LC_ALL=C sort); do
	[ "$j" = "$CORE_OJ" ] || BOOTCLASSPATH="$BOOTCLASSPATH:$j"
done
export BOOTCLASSPATH

# a stale AOT of the previous run (or of the previous compiler) would hide a
# change in either direction; this fork keeps its cache in ~/.cache/art
rm -f "$HOME"/.cache/art/*/*tests-art@imt.jar*
LOG="$OUT/imt-run.log"
if [ -n "${ART_TREE:-}" ]; then
	[ -x "$ART_TREE/bin/dalvikvm" ] || { echo "no dalvikvm in $ART_TREE/bin" >&2; exit 1; }
	export PATH="$ART_TREE/bin:$PATH" LD_LIBRARY_PATH="$ART_TREE/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
dalvikvm -Xusejit:true -cp "$OUT/imt.jar" ImtObjectDispatchTest >"$LOG" 2>&1 || true
# both compilers must have run: the AOT of the jar and the JIT in-process
grep -q "dex2oat.*tests-art@imt.jar" "$LOG" || echo "check-imt-object-dispatch: warning: the jar was not AOT compiled" >&2
# dalvikvm aborts during shutdown after main() returns, so judge the run by the
# marker and not by the exit code or the register dump that follows it
grep -qF "imt object dispatch: passed" "$LOG" || {
	echo "check-imt-object-dispatch: FAILED:" >&2
	grep -E "imt object dispatch|through the interface|differs for" "$LOG" | head -6 >&2
	grep -qE "imt object dispatch|through the interface" "$LOG" ||
		grep -v "^dex2oat\|^dalvikvm" "$LOG" | tail -8 >&2
	exit 1
}
echo "check-imt-object-dispatch: passed"
