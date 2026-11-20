#!/bin/bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
framework="${API_IMPL_JAR:-$repo/builddir/src/api-impl/hax.jar}"
core="${CORE_ALL_JAR:-$(pkg-config --variable=libdir art-standalone)/java/core-all_classes.jar}"
out="$repo/builddir/tests-api"
mkdir -p "$out"
"${JAVA_HOME:+$JAVA_HOME/bin/}javac" --release 8 -cp "$framework" -d "$out" \
    "$repo/tests/api/UpstreamCompatibilityTest.java"
"${JAVA_HOME:+$JAVA_HOME/bin/}java" -cp "$out:$framework:$core" UpstreamCompatibilityTest
