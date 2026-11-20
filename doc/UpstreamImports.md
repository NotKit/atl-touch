# Upstream ATL imports, November 2026

Reviewed upstream master through `22a96b28ee007d610f610c6f37aa81509195e288`
against atl-touch `937f637c01d2766dbb88a8f7e7a7e5cffba8abf4`.
The review covers all 190 upstream commits after the common ancestor `1f041f58`.

GTK/GSK/Pango rendering, GTK activity/window switching, the fork’s camera
backends and its input-method integration remain intact. Mixed API commits
contribute missing APIs without replacing working fork implementations with
upstream stubs. The table records decisions for every upstream commit.

The main adaptations use the fork’s codec backend for decoder flushing,
FileDescriptorUtils for MemoryFile on ART and stock JVMs, Java view coordinates,
and GApplication rather than GtkWindow for keyring application identity.
libsecret is optional: persistent keys/accounts require both the library and a
running Secret Service. Asymmetric keys remain in memory for the session.

The ghax class-loader override and upstream’s generated JNI wrapper migration
are deferred. Timed MediaCodec frame release and WebKit navigation signal
wiring still need ports to the fork’s presentation and WebView interfaces.

## Validation

- Full native, Java and dex build with Meson/Ninja on x86_64, using prebuilt Skia.
- `bash tests/api/check-upstream-compatibility.sh`: parcel, Binder/Messenger,
  URI, content-provider batch, AtomicFile recovery and network capability checks.
- `bash tests/api/check-upstream-runtime.sh`: Skia pixels, view coordinates,
  drawable states, disabled focus, MemoryFile and decoder flushing on the JVM.
- Existing TextView rendering suite: 51 assertions passed under Xvfb.

The optional libsecret-enabled path, Wayland Vulkan presentation, ARM64/ART
execution and physical camera/keyboard behavior were not exercised locally.

## Decisions

5 adapted, 6 deferred, 72 imported, 81 retained, 26 skipped.

| Upstream | Change | Decision | Reason |
| --- | --- | --- | --- |
| `6d8d12dd` | api-impl: Put applications into their own class loader | Retained | Already imported before this series, possibly with fork adaptations. |
| `0187190f` | api-impl: Improve and fixup AssetManager.extractFromAPK() | Retained | Covered by the fork’s existing implementation. |
| `5ef613de` | api-impl: Add Configuration.colorMode field | Retained | Already imported before this series, possibly with fork adaptations. |
| `08aa42af` | api-impl: Add stub implementation of IntentService | Retained | Already imported before this series, possibly with fork adaptations. |
| `c1e1f497` | api-impl: Fix context always being null in Notification.Builder | Retained | Already imported before this series, possibly with fork adaptations. |
| `43694729` | api-impl: Make providers load order independent. | Retained | Already imported before this series, possibly with fork adaptations. |
| `4b8d7599` | api-impl: Implement View.setEnabled() and partially View.setFocusable() | Adapted | Keep Java enabled/focus state; reject focus on disabled, hidden or nonfocusable views. |
| `a9815761` | api-impl: Implement Intent.parseIntent() | Retained | Already imported before this series, possibly with fork adaptations. |
| `135585bc` | api-impl: Read checked attribute on CompoundButton | Retained | Already imported before this series, possibly with fork adaptations. |
| `9e73e595` | api-impl-jni: Fix GTK Signal Handler memory leak for a few widgets | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `a3c9afb3` | api-impl: Implement extra package loading | Retained | Already imported before this series, possibly with fork adaptations. |
| `68f23a49` | api-impl: Forward MODE_NO_LOCALIZED_COLLATORS in openOrCreateDatabase() | Retained | Already imported before this series, possibly with fork adaptations. |
| `c304181c` | main-executable: Properly redirect to fonts.xml when in builddir. | Retained | Already imported before this series, possibly with fork adaptations. |
| `b4baa05c` | api-impl: Add TextView.append() implementation. | Retained | Already imported before this series, possibly with fork adaptations. |
| `fc1e9b20` | api-impl: add MutableContextWrapper | Retained | Already imported before this series, possibly with fork adaptations. |
| `de537e78` | api-impl: ViewGroup.MarginLayoutParams: stub isMarginRelative | Retained | Already imported before this series, possibly with fork adaptations. |
| `f0851214` | api-impl: add NetworkInfo isFailover | Retained | Already imported before this series, possibly with fork adaptations. |
| `6ecd7f99` | api-impl: Set ro.product.cpu.abi for aarch64 systems as well | Retained | Covered by the fork’s existing implementation. |
| `3ced0992` | api-impl: ApplicationInfo: actually set primaryCpuAbi | Retained | Already imported before this series, possibly with fork adaptations. |
| `c7126cb9` | api-impl: Configuration: initialize to default locale instead of null | Retained | Already imported before this series, possibly with fork adaptations. |
| `a472a2e9` | libandroid: stub AAsset_isAllocated | Retained | Already imported before this series, possibly with fork adaptations. |
| `6c2a45ff` | libandroid: implement eglCreateImageKHR/eglDestroyImageKHR | Retained | Already imported before this series, possibly with fork adaptations. |
| `db53d330` | LayerDrawable: Correctly handle the theme during inflation | Retained | Covered by the fork’s existing implementation. |
| `b3bee931` | View: Correctly handle the theme in View.setBackgroundResource and ImageView.setImageResource | Retained | Covered by the fork’s existing implementation. |
| `d9dce319` | TypedArray: Fix double multiplication of STYLE_NUM_ENTRIES in error paths | Retained | Already imported before this series, possibly with fork adaptations. |
| `fbb10830` | Activity: Defer window close via g_idle_add to allow pending startActivity to repopulate backlog | Retained | Already imported before this series, possibly with fork adaptations. |
| `bb408b73` | Import android.util.Patterns from AOSP | Retained | Already imported before this series, possibly with fork adaptations. |
| `9e8f53fe` | api-impl: misc stubs for latest WhatsApp update | Retained | Covered by the fork’s existing implementation. |
| `a5ca8c90` | api-impl: Add missing meson paths | Retained | Covered by the fork’s existing implementation. |
| `bc7a0344` | api-impl-jni: Implement XmlBlock.nativeCreate. | Retained | Already imported before this series, possibly with fork adaptations. |
| `04cbeef6` | api-impl: Make XmlBlock public, implement AutoCloseable, and minor fixups. | Retained | Already imported before this series, possibly with fork adaptations. |
| `c3a25ec1` | api-impl: Add EarlyPackageParser ATL Helper to read minSdkInt of an apk. | Retained | Already imported before this series, possibly with fork adaptations. |
| `00852385` | api-impl: Set Build.VERSION.SDK_INT depending on the apk minSdk version. | Retained | Already imported before this series, possibly with fork adaptations. |
| `768ebefb` | api-impl: Add sanity check in AssetManager. | Retained | Already imported before this series, possibly with fork adaptations. |
| `ebe8de4b` | api-impl: Add ATL helper to add GMS signatures to a parsed package. | Retained | Already imported before this series, possibly with fork adaptations. |
| `c7c933b7` | api-impl: Apply GMS signature to all GMS packages | Retained | Already imported before this series, possibly with fork adaptations. |
| `c84d49b2` | gstub: Add initial gstub.jar file building in ATL | Retained | Already imported before this series, possibly with fork adaptations. |
| `4552ecbd` | api-impl: Add ATLGStub helper to load gstub.jar as GMS packages | Retained | Already imported before this series, possibly with fork adaptations. |
| `a0ebae3f` | api-impl: Redirect failed GMS package loads to the ATLGStub helper | Retained | Already imported before this series, possibly with fork adaptations. |
| `12e8cd37` | android_view_View.c: fix long -> jlong | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `ef34ac94` | api-impl: Stub PackageInstaller.registerSessionCallback() | Retained | Already imported before this series, possibly with fork adaptations. |
| `e1497e2f` | api-impl: Add Debug.getMemoryInfo().getTotalPss() stub | Retained | Already imported before this series, possibly with fork adaptations. |
| `f4622e36` | android_media_AudioRecord.c: fix long -> jlong | Retained | Already imported before this series, possibly with fork adaptations. |
| `89ecf0c7` | api-impl: Spoof GMS signature when parsing installed apk | Retained | Already imported before this series, possibly with fork adaptations. |
| `b146b32b` | main-executable: Don't reset BIONIC_LD_LIBRARY_PATH | Retained | Already imported before this series, possibly with fork adaptations. |
| `eba391b1` | Context.bindService(): call serviceConnection.onServiceConnected() again | Retained | Covered by the fork’s existing implementation. |
| `316ab58b` | Context.createConfigurationContext(): keep non overridden configs from base context | Retained | Already imported before this series, possibly with fork adaptations. |
| `02ab0967` | Configuration: set screenLayout config as early as possible | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `cf2c759f` | api-impl: implement getSpacingAdd and getAlignment | Retained | Covered by the fork’s existing implementation. |
| `c393508b` | api-impl: implement ContentResolver.acquire[Unstable]ContentProviderClient | Retained | Already imported before this series, possibly with fork adaptations. |
| `3c56e8cf` | api-impl: implement View.canResolveLayoutDirection | Retained | Covered by the fork’s existing implementation. |
| `b7bedbb8` | api-impl: add Build.VERSION.PREVIEW_SDK_INT | Retained | Already imported before this series, possibly with fork adaptations. |
| `aa455940` | api-impl: misc stubs and additions for NewPipe 0.29 | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `790cd447` | implement android.opengl.EGL14 as wrapper for EGLImpl | Retained | Covered by the fork’s existing implementation. |
| `853185c8` | MediaCodec: implement flush using avcodec_flush_buffers | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `5cb6460b` | Mediacodec: handle presentationTime parameter in releaseOutputBuffer | Skipped | Schedules GTK video textures; the fork presents Skia frames through Surface. Timed release remains unported. |
| `f37aa2fa` | MediaCodec: raise output buffer count to 4 to match ExoPlayer's expectation | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `8c3243ba` | MediaCodecInfo: set CodecCapabilities for video/avc, so that ExoPlayer enables adaptive livestream playback features | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `f02d8167` | intent_deserialize: use application class loader | Retained | Covered by the fork’s existing implementation. |
| `923b6df1` | RenderNode: add missing method to make NewPipe use RenderNodes | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `2312ede7` | main-executable: Override /storage/emulated/0 | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `50d41346` | libmediandk: Define all AMEDIAFORMAT_KEY values | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `8793486e` | api-impl: AudioManager: Add flag values for getDevices | Retained | Covered by the fork’s existing implementation. |
| `c0e35a22` | api-impl: Environment: Always return true from isExternalStorageManager | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `99de5598` | api-impl: CaptioningManager: Stub addCaptioningChangeListener | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `6e8c12c8` | api-impl: Display: Stub getSupportedModes | Retained | Covered by the fork’s existing implementation. |
| `be864431` | api-impl: View: Stub stuff related to window insets | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `7a388dfa` | api-impl: add classes and methods needed for the CTS os tests | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `50c5a337` | LevelListDrawable: copy from AOSP | Retained | Covered by the fork’s existing implementation. |
| `aa58f892` | android.os.Debug: add allocation counting stub implementation | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `26d7f755` | Message.java: implement Parcelable serialization and fix Parcel exception handling | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `84921ce1` | StringBuilderPrinter: add minimal implementation needed for CTS tests | Retained | Covered by the fork’s existing implementation. |
| `84ec432a` | MessageQueue: fix SIGSEGV and hang in CTS tests | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `7b5b9614` | android.os: add missing implementations for the CTS os test suite | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `5f983d91` | Looper.prepareMainLooper: throw only for apps targeting Jelly Bean or later | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `09b0f82d` | MemoryFile: implement using memfd_create + mmap | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `70d36ffa` | SystemClock.setCurrentTimeMillis: add missing JNI implementation | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `458ff777` | Instrumentation.create: add null pointer handling | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `0055e25f` | ATLLoadedApp.stopService: null-guard intent.getComponent | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `c95a59fd` | AssetManager: handle null and empty asset paths | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `3e686d14` | MediaMetadata.writeToParcel(): write one bundle for compatibility with MediaMetadataCompat | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `ee573a73` | api-impl: misc stubs for WhatsApp 2.26.37.74 | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `b4bd4038` | MutableContextWrapper: actually allow changing the base Context | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `c0a9a1d5` | generated_headers: regenerate stale AudioManager header | Adapted | Regenerate AudioManager JNI declarations from the fork’s Java API. |
| `cae92830` | main.c: ignore appmenu button when parsing gtk-decoration-layout | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `27fbbe9f` | Wayland: only override application id with package name when no gapplication app id was given | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `f5d812ba` | main: set StartupNotify=true in installed desktop entries to get a splash screen on phosh | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `d7a6b02e` | main: mark installed .desktop files as mobile compatible using 'X-Purism-FormFactor' | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `4222d82f` | main: install DBus service files to make DBusActivatable work properly | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `2ac9b521` | Dialog: drop dead "response" signal connection | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `09e0f70f` | Dialog: implement setCanceledOnTouchOutside | Retained | Covered by the fork’s existing implementation. |
| `fb7b1ee2` | AndroidKeyStore: implement secure key storage using libsecret | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `6240d032` | api-impl: misc stubs for Signal Messenger | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `b0c6480b` | RenderNode: gracefully handle rendering a node with already discarded display list | Retained | Covered by the fork’s existing implementation. |
| `4542a8b8` | egl: fall back to desktop GL compatibility context for GLES1 contexts | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `400f3cb7` | meson.build: add androidfw include dir to libandroid | Retained | Covered by the fork’s existing implementation. |
| `3b8e6006` | NotificationManager: null-guard the notification icon | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `12053cd7` | AssetManager: handle a null ZipEntry in extractFromAPK | Retained | Covered by the fork’s existing implementation. |
| `53f49655` | api-impl: add Build.SUPPORTED_32_BIT_ABIS and SUPPORTED_64_BIT_ABIS | Retained | Covered by the fork’s existing implementation. |
| `d503f89f` | api-impl: add View.setForegroundGravity/getForegroundGravity | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `28464580` | api-impl: implement TextSwitcher.setText/setCurrentText | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `39077fdf` | ViewSwitcher: fix getNextView to toggle between the two children | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `dd91ec5d` | api-impl: Add ghax submodule for overriding GMS client library classes | Deferred | ghax assumes an ART PathClassLoader override. The fork also supports JVM and native-image launchers. |
| `aa80e740` | api-impl: Add stub and hack for android WearOS sample app | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `aea11650` | GVariantBuilderParcel: avoid DBus incompatible "maybe string" type | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `310fe50d` | Google Cloud Messaging: implement two-way Messenger interface | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `86f31628` | Button/ImageButton: only consume touch events if onClick is wired | Retained | Covered by the fork’s existing implementation. |
| `6c382ce9` | SoundPool: fix loading assets by resId | Adapted | Use the actual resource path while retaining asynchronous GStreamer load callbacks. |
| `c98bfc63` | Resources: set mDrawableCache correctly for every Resource object | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `bdb4759f` | gstub: stub maps module to prevent WhatsApp crash | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `5280f9ce` | api-impl: misc stubs for WhatsApp | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `8c1aee20` | ghax: add missing DynamiteMeasurementManager.getInterfaceDescriptor() | Deferred | Companion fix for the unimported ghax module. |
| `c63c3c27` | api-impl: implement ConnectivityManager.getNetworkCapabilities | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `068d4052` | main: handle multiple APK files for APK bundles | Retained | Covered by the fork’s existing implementation. |
| `5e2998c8` | api-impl: misc stubs and fixes for Deutsche Bahn Navigator | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `3c226d2e` | Context.getDatabasePath(): handle already absolute file paths | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `c048a000` | ViewTreeObserver: implement onPreDraw | Retained | Covered by the fork’s existing implementation. |
| `49c3b2e9` | View: handle onLayout not allocating all children | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `dd157882` | BitmapFactory: handle opts.inBitmap and opts.inJustDecodeBounds | Retained | Covered by the fork’s existing implementation. |
| `38172b76` | android.text.Layout: fix per-line top/bottom measurement | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `c176ea2d` | ScrollView: set natural scroll policy on the GtkViewport | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `46666ea6` | View: implement removeCallbacks() | Retained | Covered by the fork’s existing implementation. |
| `e78fe252` | AtomicFile: copy from AOSP | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `dc710de5` | SurfaceView.unlockCanvasAndPost: set viewport size | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `d44e2068` | AudioFocusRequest: import from AOSP android-14.0.0_r1 | Retained | Covered by the fork’s existing implementation. |
| `f9e2280c` | api-impl: add AUDIOFOCUS_* constants and audio focus methods | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `b169670c` | api-impl: misc stubs and fixes for Signal Messenger | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `34a980f3` | View: implement onDescendantInvalidated callback | Retained | Covered by the fork’s existing implementation. |
| `31b5c35b` | View: handle touch events for GTK widget inside ComposeUI layouts | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `8320b96a` | WebView: implement WebViewClient.shouldOverrideUrlLoading | Adapted | Add the Java callback API. GTK WebKit signal wiring does not apply to the fork’s WPE/Qt backend ABI. |
| `7e49c336` | View: implement OnLayoutChangeListener | Retained | Already imported before this series, possibly with fork adaptations. |
| `d6c9605a` | Canvas: prevent integer overflow in the GSK renderer when drawing unbounded Color | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `e4035a9a` | ViewGroup: implement offsetDescendantRectToMyCoords | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `ee2f45a0` | GskCanvas: implement clipPath() | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `349dbcdc` | GskCanvas.drawRect: skip when xfermode is set | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `c33a7642` | View: wire isAttachedToWindow to GtkWidget::root instead of map/unmap | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `0e9c3592` | Activity: make use of GtkStack for activity switching | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `c651b6d4` | main: enable GtkStack animations for Activity switching | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `c467a3df` | Paint: implement getFontMetrics properly | Retained | Covered by the fork’s existing implementation. |
| `51c3a58d` | Canvas: synchronize Bitmap access | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `0578d591` | build: Use correct formatting for build.xml and meson.build | Skipped | Formatting-only changes to build files; keep the fork’s build definitions. |
| `ac5be6bd` | doc: Mention find exec command to run clang-format on an entire folder. | Skipped | Documentation-only formatting guidance; keep the repository’s existing instructions. |
| `a3fad654` | api-impl: stub ContextThemeWrapper.applyOverrideConfiguration | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `2bedb464` | api-impl: stubs and new classes for LibreTube 32.1 | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `07c34aba` | api-impl: add Activity.isInPictureInPictureMode | Retained | Covered by the fork’s existing implementation. |
| `7309366d` | Region: implement using cairo_region_t | Retained | Covered by the fork’s existing implementation. |
| `b34d9a17` | Path: Implement op() using Region.op() + special case for composeUI hit testing | Retained | Covered by the fork’s existing implementation. |
| `f1f86bcf` | GskCanvas: don't hardcode the path fill rule | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `80ffde94` | View: Move JavaWidget definition to the top of the file | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `4163c2d9` | View: don't set GTK sensitivity for JavaWidgets | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `4b3aade9` | LayoutInflater: cache View constructors | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `0dec47bb` | Resources: enable XmlBlock cache | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `ff520eb0` | Resources: actually use the Drawable cache | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `fe0dba90` | implement Log.isLoggable() and guard performance critical log prints | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `d658957d` | Dialog: avoid double unref of touch controller | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `62e45efd` | MediaSession: fix the Callback inner class | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `4704f93e` | api-impl: add Export annotations | Deferred | Export annotations belong to the upstream JNI wrapper migration. |
| `2507a605` | tools: add jni_upcall_gen | Deferred | JNI generator depends on the upstream export inventory; the fork has a different JNI surface. |
| `b17fe9a5` | upcalls: refactor all C code to use generated wrappers | Deferred | Rewrites JNI calls throughout the removed GTK widgets and the upstream export inventory. |
| `4e0a1f77` | api-impl: misc stubs to make CoMaps launch | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `83226db3` | AssetManager.openAssetFd(): handle non-existing paths | Retained | Covered by the fork’s existing implementation. |
| `45a80eb4` | looper: implement ALooper_addFd for main thread using g_source_add_unix_fd | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `58384f7c` | native_window: make Vulkan compatible with MapLibre | Adapted | Port Vulkan surface queries and proc-address wrappers to the existing Wayland native windows. |
| `41960847` | api-impl: misc stubs for MapLibre test app | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `0cedff74` | AssetManager.seekAsset: map whence values properly | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `f47b3200` | StateListDrawable: implement ConstantState.newDrawable() | Retained | Covered by the fork’s existing implementation. |
| `b52baa89` | ViewGroup: implement touch event propagation in Java | Retained | Covered by the fork’s existing implementation. |
| `972d4e5c` | View.layout(): Check re-entrancy to prevent GTK crash | Skipped | Targets GTK, GSK, Pango or their widget lifecycle; replaced in this fork. |
| `08cb8a48` | api-impl: misc stubs for WhatsApp voice note recorder | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `099f77f3` | SoundPool: remove initialization hack | Retained | Covered by the fork’s existing implementation. |
| `c65b2723` | MediaCodec: increase ouput buffer size | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `37b8ed08` | tools/jni_upcall_gen: allow manually specifying paths to distro-installed dependencies | Deferred | Build options for the unimported JNI generator. |
| `c9c63fc6` | ATLSigHelper: replace signatures of Play Services instead of adding to them | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `f3b39276` | api-impl: Binder: dispatch transact to onTransact. | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `576ec0a8` | api-impl: Parcel: return zero/null when reading past the end to match aosp. | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `bbb4d469` | api-impl: implement basic AccountManager | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `f02f15ca` | Drawable: add isAutoMirrored() stub | Retained | Covered by the fork’s existing implementation. |
| `be728420` | SizeF: add constructor and getters | Retained | Covered by the fork’s existing implementation. |
| `6db66279` | Bitmap: implement getGenerationId() | Retained | Covered by the fork’s existing implementation. |
| `cf927d5f` | api-impl: Make Uri parcelable, same way as aosp | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `d55c9260` | api-impl: implement ContentProvider batch operations. Code from AOSP android-latest-release commit b356380. | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `c0149c2b` | api-impl: implement ContentProvider.call | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `45e675b8` | api-impl: run sync adapters on requestSync | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `19919518` | api-impl: Activity lifecycle callbacks. | Retained | Covered by the fork’s existing implementation. |
| `f9101762` | View: implement pressed/selected/activated/hovered setters | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `0e74d4b1` | View: implement getDrawableState() and onCreateDrawableState() | Retained | Covered by the fork’s existing implementation. |
| `0c9d8ab9` | ViewGroup: refresh children with duplicateParentState | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `a5dfe537` | TextView: apply ColorStateList for the current drawable state | Retained | Covered by the fork’s existing implementation. |
| `a89317ac` | ImageView: pass drawable state to the image | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
| `22a96b28` | CompoundButton: add checked state to drawable state | Imported | Cherry-picked or adapted; source is recorded in the import commit. |
