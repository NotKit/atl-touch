#!/bin/bash
set -euo pipefail

: "${ATLAS_RUNTIME:?Set ATLAS_RUNTIME to a JVM ATL runtime directory}"
: "${APP_APK:?Set APP_APK to an APK for resource initialization}"
: "${SHIM_JAR:?Set SHIM_JAR to the JVM compatibility shim.jar}"

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    exec xvfb-run -a bash "$0"
fi

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="$repo/builddir/tests-api-runtime"
mkdir -p "$out/classes" "$out/data"
"${JAVA_HOME:+$JAVA_HOME/bin/}javac" -cp "$ATLAS_RUNTIME/api-impl.jar" \
    -d "$out/classes" "$repo/tests/api/UpstreamRuntimeTest.java"
export LD_LIBRARY_PATH="${JAVA_HOME:+$JAVA_HOME/lib/server:}$ATLAS_RUNTIME${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ATL_NO_GPU=1 ANDROID_APP_DATA_DIR="$out/data"
export ATLAS_FONTS_DIR="$ATLAS_RUNTIME/system/fonts"
export ATLAS_FONTS_XML="$ATLAS_RUNTIME/system/etc/fonts.xml"
timeout 60 "$ATLAS_RUNTIME/android-translation-layer-hotspot" \
    --api-impl-jar "$ATLAS_RUNTIME/api-impl.jar" \
    --framework-res "$ATLAS_RUNTIME/framework-res.apk" \
    --natives-dir "$ATLAS_RUNTIME" --classpath "$SHIM_JAR:$out/classes" \
    --run-class UpstreamRuntimeTest "$APP_APK" >"$out/check.log" 2>&1 || {
    tail -40 "$out/check.log" >&2
    exit 1
}
grep 'upstream runtime: passed' "$out/check.log"
