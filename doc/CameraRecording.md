# Recording a camera2 session, and replaying it on a desktop

`ATL_CAMERA_RECORD` keeps every camera2 event of a session in a ring in memory
and writes a burst around a capture out to one file; `ATL_CAMERA_BACKEND=replay`
serves that file back to an app as a camera. The point is the debug cycle: a
scene is captured once on the phone, and every merge, demosaic or tone map that
is argued about afterwards runs against the same frames on a laptop.

What a recording holds is a whole session, not a pile of frames: the recorded
camera's characteristics and key lists, the streams as configured, every
request the app submitted with its settings, and per frame the capture-started
event, each stream's buffer, and the result. That is what makes it replayable
into an app that has never seen the device.

## Recording

```sh
ATL_UGLY_ENABLE_CAMERA=1 ATL_CAMERA_BACKEND=camera2ndk \
ATL_CAMERA_RECORD=/home/phablet/captures \
    ./android-translation-layer app.apk -l com.example.Camera
```

A burst is written when the app takes a picture - any non-repeating request, or
only one whose `CONTROL_CAPTURE_INTENT` is `STILL_CAPTURE` with
`ATL_CAMERA_RECORD_TRIGGER=intent`. `touch /home/phablet/captures/trigger`
triggers one by hand, which is how a session that never takes a picture (a
viewfinder, a video mode) gets recorded. Each burst is
`capture-<date>-<n>.atlcam`; the recorder keeps going afterwards, so a run can
produce several.

The newest repeating request is kept outside the ring and written with every
file: it is submitted once and would otherwise age out, and it is the 3A the
app asked for on every frame that followed (`atl-camrec.py requests` prints
it). One-shot requests live in the ring like everything else, and the one that
triggered the burst is part of it.

The ring is what decides how much comes *before* the trigger:
`ATL_CAMERA_RECORD_MB` (default 512) holds roughly 32 full-sensor RAW10 frames,
about four seconds at the 8 fps a 4080x3072 stream runs at. When it is full the
oldest events go first, so a recording always ends at the capture and may start
late. `ATL_CAMERA_RECORD_AFTER` (default 12) is how many frames are kept after
the trigger before the file is written.

Two things cost frame rate while recording:

* **every buffer is copied** on the delivery thread, because a HAL buffer has to
  go back to its `AImageReader` or the stream stalls. A 4080x3072 RAW10 frame is
  15.7 MB of `memcpy`; compression happens on a writer thread of its own.
* **a PRIVATE viewfinder buffer is locked and downsampled** to an NV21 reference
  (`ATL_CAMERA_RECORD_PREVIEW`, default 640 on the long edge, 0 to keep none).
  It is there so a burst can be looked at beside what the app was pointed at -
  it is not something to merge, and the file says so with a flag.

Expect little from zstd on raw Bayer: sensor noise is close to incompressible,
so a burst of RAW10 lands within a few per cent of its size in memory. The win
is one self-describing file with the metadata attached, not the bytes.

## Reading one

```
$ tools/atl-camrec.py info captures/capture-20260914-221241-0.atlcam
recording off backend 'camera2ndk'
camera 0: facing 0, orientation 90, 119 characteristics, 61 request / 87 result keys
camera 1: facing 1, orientation 270, 104 characteristics, 59 request / 85 result keys
stream 0: 1280x960 PRIVATE, 11 buffer(s)
stream 1: 1280x960 YUV_420_888, 52 buffer(s)
stream 2: 4080x3072 RAW10, 37 buffer(s), physical camera 5
...
8 frame(s), 21 buffer(s), 124.5 MiB of pixels, 1 request(s)
burst at event 20: 4 frame(s) before it, 4 after
0.2 s of sensor time, 30.0 frames/s
```

(Google Camera on caiman, 128 MiB ring: its seven-stream session, four of them
RAW10 on four physical cameras. 124.5 MiB of frames went into 91.5 MiB on disk
- raw Bayer, as promised.)

`list` counts the chunks, `meta` prints a camera's characteristics the way
`ATL_CAMERA_DUMP_METADATA` does, `requests` prints each recorded request's
settings (the app's own AE/AF/AWB decisions, vendor tags and all), `frames` is
one line per buffer, `dump` writes
every buffer's planes out as files (a RAW10 plane goes straight into whatever
demosaic is being argued about), and `png` writes the NV21 references.

## Replaying

```sh
ATL_UGLY_ENABLE_CAMERA=1 ATL_CAMERA_BACKEND=replay \
ATL_CAMERA_REPLAY=captures/capture-20260914-211603-0.atlcam \
    ./android-translation-layer app.apk -l com.example.Camera
```

The app sees the recorded camera - its ids, its characteristics, its vendor
tags - and a session whose streams match the recorded ones is fed the recorded
buffers at the recorded intervals: the pre-roll on a loop while the repeating
request runs, and the burst once for each still capture, giving way at the next
frame so a shutter press does not wait out a loop. `ATL_CAMERA_REPLAY_SPEED`
scales the intervals.

### Watching it

```sh
./tests/camera/replay-visual.sh captures/capture-*.atlcam
```

Open Camera on the desktop with its viewfinder fed by the phone's frames, and
its HUD reading the phone's own numbers (`ISO 239  1/30s` off the recorded
result bags). Its shutter saves the recording's own 4080x3072 JPEG - the still
that came out of the phone's HAL - under
`/tmp/atl-replay-data/<apk>_/DCIM/OpenCamera`. Any camera2 app works the same
way; Open Camera is just the one that needs no Pixel-only native code, and a
recording made by *that app on the phone* is the one whose streams match it
best.

Three things are worth knowing before trusting a replay:

* **It is open-loop.** Every request the app submits is recorded and can be read
  out of the file, and on replay the app's own AE/AF decisions are answered with
  the recorded results - the pixels do not change with them. Merges, demosaics
  and tone maps are exactly reproducible; 3A convergence is the one thing this
  cannot debug.
* **A payload stream has to match** in format and size: a raw frame or a JPEG
  is not something to scale, so those are served only where the recording holds
  that exact stream. A **viewfinder is scaled** instead (YUV_420_888, PRIVATE),
  because on a desktop it is whatever size the app's window made it and never
  the phone's - the recorded scene, coarser. Where several recorded streams
  match - a Pixel's four RAW10 streams, one per physical camera - the one with
  frames in it wins, since only the active lens' stream has any. The log names
  the recorded stream behind each one, its physical camera and whether it was
  scaled. A stream that cannot be served at all refuses the session, and
  `camera_streams.c` then emulates it off the recorded frames: a working
  viewfinder, but fabricated raw.
* **The whole file is read into memory**, so a 600 MB burst is 600 MB of RSS.

## Where the pieces are

| file | what it is |
| --- | --- |
| `camera_recording.h/.c` | the container: chunk types, the metadata bag encoding, zstd |
| `camera_record.c` | the ring, the trigger and the writer thread |
| `camera_replay.c` | the recording as a backend, and the playback thread |
| `camera_streams.c` | the tap: `atl_camera_record_*` on the session's own events |
| `tools/atl-camrec.py` | reading a file without an app |
| `tests/camera/check-record.sh` | record off the gst backend, replay, and compare the pixels |
| `tests/camera/replay-visual.sh` | play a recording into Open Camera, to watch it |

The tap is `camera_streams.c` rather than one backend, so a session is recorded
whether the backend serves its streams itself (camera2ndk on a device) or the
emulation stands in for them (gst on a desktop) - which is also what makes the
end-to-end check runnable with no phone:

```sh
ninja -C builddir && ./tests/camera/check-record.sh
```

It drives a headless camera2 session under `dalvikvm` against a videotestsrc
pattern, writes a recording, replays it into the same test, and fails unless the
first replayed frame's luma is the recorded one.
