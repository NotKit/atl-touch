#!/bin/bash
# Build a reusable "ATL SDK": the Android Translation Layer runtime (atlas)
# plus its whole native dependency chain, compiled once and packaged as a
# tarball. Click-packaging repos (ankidroid-click-packaging,
# mercurygram-click-packaging, ...) unpack it into their build stage and skip
# the long compile of art_standalone, skia, wolfSSL, GLFW, vixl, libunwind and
# bionic_translation — the dominant CI cost.
#
# Runs inside a clickable UT 24.04-2.x container (which supplies the UT-overlay
# toolchain: Qt 6.10 with WebEngine, the right glibc, etc.). art_standalone is
# self-hosted only, so the container must be NATIVE for the target arch — the
# CI job picks the matching arm64/amd64 runner and image.
#
# Everything is built to a fixed, app-independent prefix (SDK_PREFIX). The
# native libs relocate at runtime via LD_LIBRARY_PATH and art via dladdr, so a
# consumer can drop the tree straight into a click. atlas bakes its data dir
# (fonts.xml) at this prefix; a consumer that needs it click-local rebuilds
# only atlas — the multi-hour deps still come from the SDK.
set -euo pipefail

# --- where the finished SDK claims to live -------------------------------
# Mirrors a click's version-independent 'current' symlink so compiled-in
# paths stay valid wherever the tree is unpacked.
SDK_BASE="/opt/atl-sdk/current"
SDK_PREFIX="${SDK_BASE}/usr"

# --- pinned dependency sources (override via env for a bump) --------------
ART_URL="${ART_URL:-https://gitlab.com/android_translation_layer/art_standalone.git}"
ART_REF="${ART_REF:-e78bf68917bcaaf58fef3960cd88793b3b7f39cc}"   # + D8= patch below

BIONIC_URL="${BIONIC_URL:-https://gitlab.com/android_translation_layer/bionic_translation.git}"
BIONIC_REF="${BIONIC_REF:-ee37eb21c91409fe0eed833d0a5a0aa6b931bb7b}"

WOLFSSL_URL="${WOLFSSL_URL:-https://github.com/wolfSSL/wolfssl.git}"
WOLFSSL_REF="${WOLFSSL_REF:-decea12e223869c8f8f3ab5a53dc90b69f436eb2}"   # v5.8.2-stable

# r8/d8 desugars core-oj + core-libart lambdas at dex time, like AOSP; without
# it apps using core-internal lambdas (streams/Collectors) crash on the stub
# LambdaMetafactory. Pure Java, arch-independent, from Google Maven.
R8_VERSION="${R8_VERSION:-8.13.19}"
R8_SHA256="7712edbae6a71f35937ffc1bd5f4c202919ef2a5e58d3c3544015a868a3cf457"

GLFW_VERSION="3.4"
GLFW_SHA256="b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9"

VIXL_VERSION="8.0.0"
VIXL_SHA256="6aebbebcd9b66686ea246b450af529e1fc50fe25209522cc9ab42beae2377d38"

LIBUNWIND_VERSION="1.8.2"
LIBUNWIND_SHA256="7f262f1a1224f437ede0f96a6932b582c8f5421ff207c04e3d9504dfa04c8b82"

# --- environment ----------------------------------------------------------
# ROOT = atl-touch checkout (this repo). BUILD_DIR = scratch + output.
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo arm64)}"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build/${ARCH}}"
JOBS="${NUM_PROCS:-$(nproc)}"
PATCHES="${ROOT}/ci/patches"

case "${ARCH}" in
    arm64) ART_ARCH="arm64" ;;
    amd64) ART_ARCH="x86_64" ;;
    *) echo "unsupported arch: ${ARCH}" >&2; exit 1 ;;
esac

STAGE="${BUILD_DIR}/stage"          # build-time sysroot for the whole chain
PREFIX="${STAGE}/usr"
DL="${BUILD_DIR}/downloads"
STAMPS="${BUILD_DIR}/stamps"
SRC="${BUILD_DIR}/src"              # pinned dependency checkouts
OUT="${BUILD_DIR}/sdk"             # assembled SDK tree (packed into the tarball)
mkdir -p "${PREFIX}" "${DL}" "${STAMPS}" "${SRC}"

export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PREFIX}/share/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
export PATH="${PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${PREFIX}/lib:${PREFIX}/lib/art${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export LIBRARY_PATH="${PREFIX}/lib"
# art includes vixl headers as e.g. "aarch64/disasm-aarch64.h" (no vixl/ prefix)
export CPATH="${PREFIX}/include:${PREFIX}/include/vixl"
export CMAKE_PREFIX_PATH="${PREFIX}"
JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")"
export JAVA_HOME

log()   { echo -e "\033[1;34m[build-sdk]\033[0m $*"; }
stamp() { [ -f "${STAMPS}/$1.done" ]; }
done_stamp() { touch "${STAMPS}/$1.done"; }

fetch() { # fetch <url> <sha256> <outfile>
    local url="$1" sha="$2" out="${DL}/$3"
    if [ -f "${out}" ] && echo "${sha}  ${out}" | sha256sum -c - >/dev/null 2>&1; then
        return 0
    fi
    log "downloading $3"
    curl -Lf --retry 3 -o "${out}.tmp" "${url}"
    echo "${sha}  ${out}.tmp" | sha256sum -c -
    mv "${out}.tmp" "${out}"
}

clone() { # clone <url> <ref> <dir>
    local url="$1" ref="$2" dir="${SRC}/$3"
    if [ -d "${dir}/.git" ] && [ "$(git -C "${dir}" rev-parse HEAD 2>/dev/null)" = "${ref}" ]; then
        return 0
    fi
    log "cloning $3 @ ${ref:0:12}"
    rm -rf "${dir}"
    git clone "${url}" "${dir}"
    git -C "${dir}" checkout --detach "${ref}"
}

# Stamps carry the pinned ref (and, for bionic, the patch hash) so a stale
# cache can never mask a bump — the renamed stamp just doesn't exist.
BIONIC_STAMP="${BIONIC_REF:0:12}-$(cat "${PATCHES}/bionic_translation-ut.patch" "${PATCHES}/bionic_translation-stdio-glibc.patch" | sha256sum | cut -c1-8)"
ART_STAMP="${ART_REF:0:12}-$(sha256sum "${PATCHES}/art_standalone-d8.patch" | cut -c1-8)-d8${R8_VERSION}"

# --- 1. GLFW 3.4 (noble ships 3.3, atlas needs the libdecor init hint) ---

if ! stamp "glfw-${GLFW_VERSION}"; then
    log "building GLFW ${GLFW_VERSION}"
    fetch "https://github.com/glfw/glfw/releases/download/${GLFW_VERSION}/glfw-${GLFW_VERSION}.zip" \
          "${GLFW_SHA256}" "glfw-${GLFW_VERSION}.zip"
    rm -rf "${BUILD_DIR}/glfw"
    unzip -q "${DL}/glfw-${GLFW_VERSION}.zip" -d "${BUILD_DIR}"
    mv "${BUILD_DIR}/glfw-${GLFW_VERSION}" "${BUILD_DIR}/glfw"
    cmake -S "${BUILD_DIR}/glfw" -B "${BUILD_DIR}/glfw/build" -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=ON \
        -DGLFW_BUILD_WAYLAND=ON \
        -DGLFW_BUILD_X11=OFF \
        -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
    ninja -C "${BUILD_DIR}/glfw/build" -j"${JOBS}" install
    done_stamp "glfw-${GLFW_VERSION}"
    rm -rf "${BUILD_DIR}/glfw"
fi

# --- 2. wolfSSL with JNI (distro package has JNI disabled) ---------------

if ! stamp "wolfssl-${WOLFSSL_REF:0:12}"; then
    log "building wolfSSL"
    clone "${WOLFSSL_URL}" "${WOLFSSL_REF}" wolfssl
    rsync -a --delete --exclude=.git "${SRC}/wolfssl/" "${BUILD_DIR}/wolfssl/"
    (
        cd "${BUILD_DIR}/wolfssl"
        autoreconf -i
        ./configure --prefix="${PREFIX}" \
            --enable-shared --disable-opensslall --disable-opensslextra \
            --enable-aescbc-length-checks --enable-curve25519 --enable-ed25519 \
            --enable-ed25519-stream --enable-oldtls --enable-base64encode \
            --enable-tlsx --enable-scrypt --disable-examples --enable-crl \
            --enable-jni --enable-sessioncerts
        make -j"${JOBS}"
        make install
    )
    done_stamp "wolfssl-${WOLFSSL_REF:0:12}"
    rm -rf "${BUILD_DIR}/wolfssl"
fi

# --- 3. vixl (ARM codegen backend for the art compiler; not in noble) ----

if [ "${ARCH}" = "arm64" ] && ! stamp "vixl-${VIXL_VERSION}"; then
    log "building vixl ${VIXL_VERSION}"
    fetch "https://github.com/Linaro/vixl/archive/refs/tags/${VIXL_VERSION}.tar.gz" \
          "${VIXL_SHA256}" "vixl-${VIXL_VERSION}.tar.gz"
    rm -rf "${BUILD_DIR}/vixl"
    tar -xf "${DL}/vixl-${VIXL_VERSION}.tar.gz" -C "${BUILD_DIR}"
    mv "${BUILD_DIR}/vixl-${VIXL_VERSION}" "${BUILD_DIR}/vixl"
    patch -d "${BUILD_DIR}/vixl" -p1 < "${PATCHES}/vixl_meson_support.patch"
    meson setup "${BUILD_DIR}/vixl/build" "${BUILD_DIR}/vixl" \
        --prefix="${PREFIX}" --libdir=lib --buildtype=release -Dsimulator=none
    ninja -C "${BUILD_DIR}/vixl/build" -j"${JOBS}" install
    done_stamp "vixl-${VIXL_VERSION}"
    rm -rf "${BUILD_DIR}/vixl"
fi

# --- 4. libunwind >= 1.8 (bionic_translation requires it; noble has 1.6) --

if ! stamp "libunwind-${LIBUNWIND_VERSION}"; then
    log "building libunwind ${LIBUNWIND_VERSION}"
    fetch "https://github.com/libunwind/libunwind/releases/download/v${LIBUNWIND_VERSION}/libunwind-${LIBUNWIND_VERSION}.tar.gz" \
          "${LIBUNWIND_SHA256}" "libunwind-${LIBUNWIND_VERSION}.tar.gz"
    rm -rf "${BUILD_DIR}/libunwind"
    tar -xf "${DL}/libunwind-${LIBUNWIND_VERSION}.tar.gz" -C "${BUILD_DIR}"
    mv "${BUILD_DIR}/libunwind-${LIBUNWIND_VERSION}" "${BUILD_DIR}/libunwind"
    (
        cd "${BUILD_DIR}/libunwind"
        ./configure --prefix="${PREFIX}" --disable-tests --disable-documentation
        make -j"${JOBS}"
        make install
    )
    done_stamp "libunwind-${LIBUNWIND_VERSION}"
    rm -rf "${BUILD_DIR}/libunwind"
fi

# --- 5. bionic_translation ------------------------------------------------

if ! stamp "bionic_translation-${BIONIC_STAMP}"; then
    log "building bionic_translation"
    clone "${BIONIC_URL}" "${BIONIC_REF}" bionic_translation
    rsync -a --delete --exclude=.git "${SRC}/bionic_translation/" "${BUILD_DIR}/bionic_translation/"
    patch -d "${BUILD_DIR}/bionic_translation" -p1 < "${PATCHES}/bionic_translation-ut.patch"
    patch -d "${BUILD_DIR}/bionic_translation" -p1 < "${PATCHES}/bionic_translation-stdio-glibc.patch"
    meson setup "${BUILD_DIR}/bionic_translation/build" "${BUILD_DIR}/bionic_translation" \
        --prefix="${PREFIX}" --libdir=lib --buildtype=release
    ninja -C "${BUILD_DIR}/bionic_translation/build" -j"${JOBS}" install
    done_stamp "bionic_translation-${BIONIC_STAMP}"
    rm -rf "${BUILD_DIR}/bionic_translation"
fi

# --- 5b. r8/d8 launcher (used by the art build to desugar the boot jars) --

fetch "${R8_URL:-https://maven.google.com/com/android/tools/r8/${R8_VERSION}/r8-${R8_VERSION}.jar}" \
      "${R8_SHA256}" "r8-${R8_VERSION}.jar"
D8="${PREFIX}/bin/d8"
mkdir -p "${PREFIX}/bin"
cat > "${D8}" <<EOF
#!/bin/sh
exec java -Xmx2G -cp "${DL}/r8-${R8_VERSION}.jar" com.android.tools.r8.D8 "\$@"
EOF
chmod +x "${D8}"

# --- 6. art_standalone (ART runtime, dex2oat, dx, boot classpath) ---------
# The multi-hour stage. Self-hosted AOSP frankenbuild; D8=<launcher> makes the
# boot jars desugar lambdas like upstream AOSP.

if ! stamp "art_standalone-${ART_STAMP}"; then
    log "building art_standalone (self-hosted, d8-desugared boot jars)"
    clone "${ART_URL}" "${ART_REF}" art_standalone
    rsync -a --delete --exclude=.git "${SRC}/art_standalone/" "${BUILD_DIR}/art_standalone/"
    patch -d "${BUILD_DIR}/art_standalone" -p1 < "${PATCHES}/art_standalone-d8.patch"
    # art's envsetup.mk expects uname-style ARCH (aarch64) and reads 'arm64' as
    # 32-bit arm — override explicitly.
    make -C "${BUILD_DIR}/art_standalone" -j"${JOBS}" \
        ARCH="$(uname -m)" ____PREFIX="${PREFIX}" ____LIBDIR=lib D8="${D8}"
    make -C "${BUILD_DIR}/art_standalone" \
        ARCH="$(uname -m)" ____PREFIX="${PREFIX}" ____LIBDIR=lib D8="${D8}" install
    done_stamp "art_standalone-${ART_STAMP}"
    rm -rf "${BUILD_DIR}/art_standalone"
fi

# --- 7. atlas (this repo) -------------------------------------------------
# Built to the SDK prefix. Always re-run so a source change is picked up; the
# skia subproject clone (meson wrap) is preserved between runs.

log "building atlas"
rsync -a --delete \
    --exclude=.git --exclude=/build --exclude=/builddir --exclude=/subprojects/skia \
    "${ROOT}/" "${BUILD_DIR}/atlas-src/"
if [ ! -f "${BUILD_DIR}/atlas-build/build.ninja" ]; then
    meson setup "${BUILD_DIR}/atlas-build" "${BUILD_DIR}/atlas-src" \
        --prefix="${SDK_PREFIX}" --libdir=lib --buildtype=release
fi
ninja -C "${BUILD_DIR}/atlas-build" -j"${JOBS}"
rm -rf "${BUILD_DIR}/atlas-dest"
DESTDIR="${BUILD_DIR}/atlas-dest" ninja -C "${BUILD_DIR}/atlas-build" install

# --- 8. assemble the SDK tree --------------------------------------------
# usr/ = the SDK prefix (/opt/atl-sdk/current/usr): atlas + the staged runtime
# (art, boot classpath, wolfssl, glfw, bionic libs + cfg.d). No app APK, no
# click metadata, no AOT — those are app/prefix specific and stay in consumers.

log "assembling SDK tree in ${OUT}"
rm -rf "${OUT}"
mkdir -p "${OUT}/usr"

# atlas (already built at SDK_PREFIX)
cp -a "${BUILD_DIR}/atlas-dest${SDK_PREFIX}/." "${OUT}/usr/"

# staged runtime libs (drop build-only bits)
rsync -a \
    --exclude='pkgconfig' --exclude='cmake' \
    --exclude='*.a' --exclude='*.la' \
    "${PREFIX}/lib/" "${OUT}/usr/lib/"
[ -d "${PREFIX}/libexec" ] && rsync -a "${PREFIX}/libexec/" "${OUT}/usr/libexec/"
[ -d "${PREFIX}/share/bionic_translation" ] && \
    rsync -a "${PREFIX}/share/bionic_translation" "${OUT}/usr/share/"

# dex2oat so a consumer can AOT-compile against its final prefix on device/CI
install -D "${PREFIX}/bin/dex2oat" "${OUT}/usr/bin/dex2oat"
# dx is superseded by d8 for the boot jars and not needed at runtime
rm -f "${OUT}/usr/lib/java/dx.jar"

# ship the dep stamps so a consumer can drop them into its stage and skip the
# equivalent build steps outright
mkdir -p "${OUT}/meta/stamps"
cp -a "${STAMPS}"/*.done "${OUT}/meta/stamps/"

# provenance
ATLAS_SHA="$(git -C "${ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)"
cat > "${OUT}/meta/sdk-manifest.json" <<EOF
{
  "arch": "${ARCH}",
  "sdk_prefix": "${SDK_PREFIX}",
  "atlas": "${ATLAS_SHA}",
  "art_standalone": "${ART_REF}",
  "bionic_translation": "${BIONIC_REF}",
  "wolfssl": "${WOLFSSL_REF}",
  "r8": "${R8_VERSION}",
  "glfw": "${GLFW_VERSION}",
  "vixl": "${VIXL_VERSION}",
  "libunwind": "${LIBUNWIND_VERSION}"
}
EOF

# strip shared libs + binaries (keep art's oat/odex untouched — there are none)
find "${OUT}/usr" -type f \( -name '*.so' -o -name '*.so.*' \) \
    -exec strip --strip-unneeded {} + 2>/dev/null || true
strip --strip-unneeded "${OUT}/usr/bin/android-translation-layer" \
    "${OUT}/usr/bin/dex2oat" 2>/dev/null || true

# --- 9. pack --------------------------------------------------------------
mkdir -p "${BUILD_DIR}/artifacts"
TARBALL="${BUILD_DIR}/artifacts/atl-sdk-${ARCH}.tar.zst"
log "packing ${TARBALL}"
tar -C "${OUT}" -c usr meta | zstd -T0 -19 -o "${TARBALL}" -f
sha256sum "${TARBALL}" | tee "${TARBALL}.sha256"

log "done — $(du -sh "${TARBALL}" | cut -f1) SDK, atlas ${ATLAS_SHA:0:12}, arch ${ARCH}"
