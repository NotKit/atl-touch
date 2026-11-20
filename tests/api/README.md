Run the Java compatibility checks after building the framework:

```sh
bash tests/api/check-upstream-compatibility.sh
```

The script uses `builddir/src/api-impl/hax.jar` and the installed ART compile
classpath. Override `API_IMPL_JAR` and `CORE_ALL_JAR` for other build layouts.

Run the native checks with a built JVM runtime:

```sh
JAVA_HOME=/path/to/jdk21 \
ATLAS_RUNTIME=/path/to/runtime \
APP_APK=/path/to/app.apk \
SHIM_JAR=/path/to/shim.jar \
bash tests/api/check-upstream-runtime.sh
```

The runtime directory must contain the hotspot launcher, the framework class
jar, framework-res.apk, native libraries and system fonts. With no display the
script starts Xvfb. Use a D-Bus session if the runtime requires portal services.
Results are written under `builddir/tests-api-runtime`.
