# ATL SDK build

`build-sdk.sh` + `.github/workflows/build-sdk.yml` compile atlas (this repo)
together with its whole native dependency chain and publish the result as a
reusable tarball, so click-packaging repos don't have to rebuild any of it.

## What gets built

The dominant CI cost of a click build is the dependency chain, not the app:

| component        | why it's here                                          |
|------------------|--------------------------------------------------------|
| `art_standalone` | ART runtime, `dex2oat`, boot classpath — **multi-hour**, self-hosted only, D8-desugared boot jars |
| `skia`           | atlas' rendering backend (meson subproject)            |
| `wolfSSL` (+JNI) | TLS; distro package ships JNI disabled                 |
| `GLFW 3.4`       | noble has 3.3, atlas needs the libdecor init hint      |
| `vixl`, `libunwind` | not in noble / too old                              |
| `bionic_translation` | bionic syscall/linker shim (UT-patched)            |
| `atlas`          | the runtime itself, from this checkout                 |

Dependency pins live at the top of `build-sdk.sh` (overridable via env). The
UT-specific patches for `vixl` and `bionic_translation` are vendored in
`ci/patches/`. The atlas toolchain/UT patches that used to live in the packaging
repos are already upstreamed here, so atlas builds clean in the container.

## Output

A tarball per arch: `atl-sdk-<arch>.tar.zst` (+ `.sha256`), containing

```
usr/                 # the runtime tree, prefix /opt/atl-sdk/current/usr
  bin/               #   android-translation-layer, dex2oat
  lib/               #   atlas libs, art, wolfssl, glfw, bionic libs, java/dex
  libexec/, share/   #   bionic_translation cfg.d, atlas data
meta/
  sdk-manifest.json  # arch + all component commits/versions
  stamps/*.done      # per-dependency build stamps
```

Published as a **prerelease** tagged `sdk-<atlas-sha>`, so it's versioned by
this repo's commit and downloadable without a login.

## Consuming it (from a click-packaging build.sh)

The runtime tree is built at the fixed prefix `/opt/atl-sdk/current/usr` and
relocates at runtime the same way the click does — the launcher's
`LD_LIBRARY_PATH` covers the libs and atlas finds its dex dir relative to
`libart.so`. So a consumer just drops the tree into its click and skips the
whole compile:

```bash
ATL_SDK_TAG="sdk-<atlas-sha>"      # pin to the atl-touch commit you want
ARCH="arm64"
url="https://github.com/NotKit/atl-touch/releases/download/${ATL_SDK_TAG}/atl-sdk-${ARCH}.tar.zst"

tmp=$(mktemp -d)
curl -Lf "$url" | zstd -d | tar -C "$tmp" -x

# 1. atlas + deps straight into the click
rsync -a "$tmp/usr/" "${INSTALL_DIR}/usr/"

# 2. (optional) let a fuller build.sh skip its own dep steps too
cp -a "$tmp/meta/stamps/." "${STAMPS}/"
rsync -a "$tmp/usr/" "${PREFIX}/"     # so the assemble step finds the libs
```

Then the consumer only does the app-specific work: fetch the APK, bundle
non-rootfs runtime libs, AOT-compile (`dex2oat` is in the tree), and pack the
click.

### Caveat: atlas data dir

atlas bakes `INSTALL_DATADIR` (its `fonts.xml` etc.) at `/opt/atl-sdk/current`.
Everything runs after relocation, but an app that opens Android's
`/system/etc/fonts.xml` directly won't get the redirect from a different click
prefix. Most apps bundle their own fonts or use fontconfig, so this is cosmetic.
If you need it exact, rebuild only atlas with your click prefix — the expensive
deps still come from the SDK stamps.

## Running it

- Triggers: push to `master`, or manual `workflow_dispatch`.
- Both arches build **natively** (`ubuntu-24.04-arm` / `ubuntu-24.04`) inside
  the matching `clickable/*-ut24.04-2.x-*` container — no qemu.
- The dependency stage is cached under `build/<arch>`, keyed by run id and
  saved even on failure, so a re-run resumes instead of restarting.
