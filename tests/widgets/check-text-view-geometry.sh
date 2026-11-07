#!/bin/bash
# Uses a JVM ATL runtime's natives with the widget sources from this checkout.
set -euo pipefail

: "${ATLAS_RUNTIME:?Set ATLAS_RUNTIME to a JVM ATL runtime directory}"
: "${APP_APK:?Set APP_APK to an APK for resource initialization}"
: "${SHIM_JAR:?Set SHIM_JAR to the JVM compatibility shim.jar}"

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
	exec xvfb-run -a bash "$0"
fi

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="${WIDGET_TEST_OUT:-$repo/builddir/tests-widgets}"
mkdir -p "$out/classes" "$out/data"
out="$(cd "$out" && pwd)"
cp "$ATLAS_RUNTIME/api-impl.jar" "$out/api-impl.jar"
"${JAVA_HOME:+$JAVA_HOME/bin/}javac" -nowarn -cp "$out/api-impl.jar" -d "$out/classes" \
	"$repo/src/api-impl/android/widget/TextView.java" \
	"$repo/src/api-impl/android/widget/EditText.java" \
	"$repo/tests/widgets/TextViewGeometryTest.java"
"${JAVA_HOME:+$JAVA_HOME/bin/}jar" uf "$out/api-impl.jar" -C "$out/classes" android/widget

export LD_LIBRARY_PATH="${JAVA_HOME:+$JAVA_HOME/lib/server:}$ATLAS_RUNTIME${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export ATL_NO_GPU=1 ANDROID_APP_DATA_DIR="$out/data"
export ATLAS_FONTS_DIR="$ATLAS_RUNTIME/system/fonts"
export ATLAS_FONTS_XML="$ATLAS_RUNTIME/system/etc/fonts.xml"
timeout 60 "$ATLAS_RUNTIME/android-translation-layer-hotspot" \
	--api-impl-jar "$out/api-impl.jar" \
	--framework-res "$ATLAS_RUNTIME/framework-res.apk" \
	--natives-dir "$ATLAS_RUNTIME" \
	--classpath "$SHIM_JAR:$out/classes" \
	--run-class TextViewGeometryTest "$APP_APK" >"$out/check.log" 2>&1 || {
	tail -40 "$out/check.log" >&2
	exit 1
}
grep 'text view geometry: passed' "$out/check.log"
