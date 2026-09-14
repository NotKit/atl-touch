# App bundles: a base APK plus splits

A modern Play app is not one APK. It is installed as a *bundle*: a `base.apk`
plus a set of `split_<name>.apk` files, all with the same package name. The
splits carry assets, resources and native code; the code usually stays in the
base. Google Camera, for instance, ships as 18 APKs of which only `base.apk`
holds dex.

ATL presents such a bundle **as installed**, never as something to download.
There is no `SplitCompat` and no split installer: everything is there from the
start, and the app sees an ordinary installed app that happens to have splits.

## Running one

Either point ATL at the directory the installer left behind:

```sh
android-translation-layer /path/to/bundle -l com.example.MainActivity
```

where the directory holds `base.apk` and the splits (any `*.apk` file that is
not `base.apk` is a split; a directory with a single APK and no `base.apk` is
that APK). Or name the base APK and list the splits explicitly:

```sh
ATL_APK_SPLITS=/path/split_a.apk:/path/split_b.apk \
	android-translation-layer /path/base.apk -l com.example.MainActivity
```

The two are equivalent. An APK given on its own is unaffected by any of this.

## What the framework does with them

* **Code.** Base first, then the splits in the order above, all on the app
  classloader (they go on `java.class.path`). A split with no `classes.dex` is
  simply skipped by the runtime.
* **Resources and assets.** Same order, as extra `ApkAssets` on the
  `AssetManager`, so a split overrides the base rather than the other way
  round. A feature split's resources live in *its own* resource package
  (`<base package>.<split name>`, package id `0x80` and up), which is why
  `getIdentifier(name, type, defPackage)` needs that package name and not the
  app's.
* **Native libraries.** Extracted out of every APK into the app's lib dir, base
  first. `extractNativeLibs=false` makes no difference here: nothing else puts a
  `.so` where `System.loadLibrary` looks.
* **Bookkeeping.** `ApplicationInfo.splitSourceDirs`,
  `splitPublicSourceDirs` and `splitNames`, and `PackageInfo.splitNames`, all in
  the same order, reachable through `Context.getApplicationInfo()` and the
  `PackageManager`.

The split name is taken from the file name (`split_<name>.apk` → `<name>`),
which is what the package installer wrote it as. ATL does not re-parse each
split's manifest for it: reading a `split` attribute out of a binary manifest
would mean loading a second `AssetManager` per split, and for a 743 MB bundle
that is a real cost for a string that is already in the path.
