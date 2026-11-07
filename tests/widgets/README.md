Run the rendering checks against a built JVM ATL runtime:

```sh
JAVA_HOME=/path/to/jdk21 \
ATLAS_RUNTIME=/path/to/runtime \
APP_APK=/path/to/app.apk \
SHIM_JAR=/path/to/shim.jar \
bash tests/widgets/check-text-view-geometry.sh
```

The runtime directory must contain `android-translation-layer-hotspot`,
`api-impl.jar`, `framework-res.apk`, the native libraries, and `system/fonts`.
The script compiles this checkout's widgets into a copy of that framework jar.
It uses an isolated data directory and runs the test class without starting
the APK's application. With no display, it starts Xvfb using `xvfb-run`.

The checks compare rendered pixels for ordinary and pretranslated editors,
single and multiline hints, and top, center, and bottom gravity. They also
check default styling, cursor bounds, wrap-content height, maxLines padding,
and layout invalidation after changing font padding.

Output and the runtime log are in `builddir/tests-widgets`; set
`WIDGET_TEST_OUT` to override that directory.
