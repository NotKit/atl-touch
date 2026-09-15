#!/usr/bin/env python3
"""Read a camera recording ATL_CAMERA_RECORD wrote (see doc/CameraRecording.md).

    atl-camrec.py list <file>              what is in it, chunk by chunk
    atl-camrec.py info <file>              cameras, streams and the burst
    atl-camrec.py meta <file> [camera]     one camera's characteristics
    atl-camrec.py frames <file>            one line per recorded buffer
    atl-camrec.py requests <file>          the requests the app submitted
    atl-camrec.py dump <file> <dir>        every buffer's planes, as files
    atl-camrec.py png <file> <dir>         the NV21 reference frames, as PNGs

The container is the one camera_recording.h describes; zstd comes from
libzstd through ctypes, so nothing needs installing.
"""

import ctypes
import ctypes.util
import os
import struct
import sys

MAGIC = b"ATLCAM2R"
VERSION = 1

CAMERA, SESSION, REQUEST, STARTED, RESULT, FAILED, LOST, BUFFER, TRIGGER = range(1, 10)
NAMES = {CAMERA: "camera", SESSION: "session", REQUEST: "request", STARTED: "started",
         RESULT: "result", FAILED: "failed", LOST: "lost", BUFFER: "buffer",
         TRIGGER: "trigger"}
TYPES = {0: ("byte", 1), 1: ("int32", 4), 2: ("float", 4), 3: ("int64", 8),
         4: ("double", 8), 5: ("rational", 8)}
FORMATS = {0x20: "RAW_SENSOR", 0x22: "PRIVATE", 0x23: "YUV_420_888", 0x25: "RAW10",
           0x26: "RAW12", 0x100: "JPEG", 17: "NV21"}

def tag_names():
    """The generated tag table, when this tool sits in its own checkout: a
    standard tag carries no name in the file, only its id."""
    names = {}
    inc = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src",
                       "api-impl-jni", "camera", "camera2_tags.inc")
    try:
        with open(inc) as f:
            for line in f:
                line = line.strip()
                if line.startswith("{0x"):
                    tag, name = line.split(",")[0], line.split('"')[1]
                    names[int(tag[1:], 16)] = name
    except OSError:
        pass
    return names


TAGS = tag_names()

_zstd = ctypes.CDLL(ctypes.util.find_library("zstd") or "libzstd.so.1")
_zstd.ZSTD_decompress.restype = ctypes.c_size_t
_zstd.ZSTD_isError.restype = ctypes.c_uint


def unzstd(data, size):
    out = ctypes.create_string_buffer(size)
    got = _zstd.ZSTD_decompress(out, ctypes.c_size_t(size), data, ctypes.c_size_t(len(data)))
    if _zstd.ZSTD_isError(got) or got != size:
        raise ValueError("a chunk did not decompress")
    return out.raw[:size]


class Cursor:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def u32(self):
        value, = struct.unpack_from("<I", self.data, self.pos)
        self.pos += 4
        return value

    def i64(self):
        value, = struct.unpack_from("<q", self.data, self.pos)
        self.pos += 8
        return value

    def string(self):
        length = self.u32()
        if length == 0xffffffff:
            return None
        out = self.data[self.pos:self.pos + length].decode("utf-8", "replace")
        self.pos += length
        return out

    def blob(self, length):
        out = self.data[self.pos:self.pos + length]
        self.pos += length
        return out

    def bag(self):
        entries = []
        for _ in range(self.u32()):
            tag, kind, count = self.u32(), self.u32(), self.u32()
            name = self.string()
            data = self.blob(self.u32())
            entries.append((tag, kind, count, name, data))
        return entries


def chunks(path):
    with open(path, "rb") as f:
        magic = f.read(8)
        version, _flags, wall, backend = struct.unpack("<IIq16s", f.read(32))
        if magic != MAGIC:
            sys.exit("%s is not a camera recording" % path)
        if version != VERSION:
            sys.exit("%s is version %d, this tool reads %d" % (path, version, VERSION))
        yield ("header", wall, backend.split(b"\0")[0].decode())
        while True:
            head = f.read(16)
            if len(head) < 16:
                return
            kind, flags, stored, size = struct.unpack("<IIII", head)
            data = f.read(stored)
            if flags & 1:
                data = unzstd(data, size)
            yield (kind, data)


def bag_value(kind, count, data):
    name, width = TYPES.get(kind, ("?", 0))
    if not width or len(data) < width * count:
        return "<%d bytes>" % len(data)
    fmt = {0: "b", 1: "i", 2: "f", 3: "q", 4: "d"}.get(kind)
    if kind == 5:
        pairs = struct.unpack("<%di" % (count * 2), data[:8 * count])
        return " ".join("%d/%d" % pairs[i:i + 2] for i in range(0, len(pairs), 2))
    values = struct.unpack("<%d%s" % (count, fmt), data[:width * count])
    text = " ".join(str(v) for v in values[:12])
    return text + (" ..." if count > 12 else "")


def read_all(path):
    """(header, cameras, streams, events) - events in the recorded order."""
    header, cameras, streams, events, burst = None, [], [], [], None
    for chunk in chunks(path):
        if chunk[0] == "header":
            header = chunk[1:]
            continue
        kind, data = chunk
        cur = Cursor(data)
        if kind == CAMERA:
            cameras.append({"id": cur.string(), "facing": cur.u32(), "orientation": cur.u32(),
                            "bag": cur.bag(),
                            "keys": [[cur.u32() for _ in range(cur.u32())] for _ in range(3)]})
        elif kind == SESSION:
            for _ in range(cur.u32()):
                streams.append({"width": cur.u32(), "height": cur.u32(), "format": cur.u32(),
                                "max_buffers": cur.u32(), "usage": cur.i64(),
                                "physical": cur.string()})
        elif kind == TRIGGER:
            burst = len(events)
            events.append({"kind": kind, "request": cur.u32(), "at": cur.i64()})
        elif kind == REQUEST:
            events.append({"kind": kind, "request": cur.u32(), "repeating": cur.u32(),
                           "targets": cur.u32(), "reprocess": cur.u32(),
                           "input": cur.i64(), "at": cur.i64(), "bag": cur.bag()})
        elif kind == STARTED:
            events.append({"kind": kind, "request": cur.u32(), "frame": cur.i64(),
                           "timestamp": cur.i64()})
        elif kind == RESULT:
            events.append({"kind": kind, "request": cur.u32(), "frame": cur.i64(),
                           "bag": cur.bag()})
        elif kind in (FAILED, LOST):
            event = {"kind": kind, "request": cur.u32(), "frame": cur.i64()}
            if kind == LOST:
                event["stream"] = cur.u32()
            events.append(event)
        elif kind == BUFFER:
            event = {"kind": kind, "stream": cur.u32(), "width": cur.u32(), "height": cur.u32(),
                     "format": cur.u32(), "timestamp": cur.i64(), "flags": cur.u32(),
                     "stream_width": cur.u32(), "stream_height": cur.u32(), "planes": []}
            for _ in range(cur.u32()):
                event["planes"].append({"len": cur.u32(), "row_stride": cur.u32(),
                                        "pixel_stride": cur.u32()})
            for plane in event["planes"]:
                plane["data"] = cur.blob(plane["len"])
            events.append(event)
    return header, cameras, streams, events, burst


def fmt_name(value):
    return FORMATS.get(value, "0x%x" % value)


def cmd_list(path):
    counts = {}
    for chunk in chunks(path):
        if chunk[0] == "header":
            print("recording off backend '%s', written %d" % (chunk[2], chunk[1]))
            continue
        counts[chunk[0]] = counts.get(chunk[0], 0) + 1
    for kind, n in sorted(counts.items()):
        print("%8s  %d" % (NAMES.get(kind, kind), n))


def cmd_info(path):
    header, cameras, streams, events, burst = read_all(path)
    print("recording off backend '%s'" % header[1])
    for camera in cameras:
        print("camera %s: facing %d, orientation %d, %d characteristics, "
              "%d request / %d result keys" %
              (camera["id"], camera["facing"], camera["orientation"], len(camera["bag"]),
               len(camera["keys"][1]), len(camera["keys"][2])))
    for i, stream in enumerate(streams):
        print("stream %d: %dx%d %s, %d buffer(s)%s" %
              (i, stream["width"], stream["height"], fmt_name(stream["format"]),
               stream["max_buffers"],
               ", physical camera %s" % stream["physical"] if stream["physical"] else ""))
    frames = [e for e in events if e["kind"] == STARTED]
    buffers = [e for e in events if e["kind"] == BUFFER]
    pixels = sum(p["len"] for e in buffers for p in e["planes"])
    print("%d frame(s), %d buffer(s), %.1f MiB of pixels, %d request(s)" %
          (len(frames), len(buffers), pixels / 1048576.0,
           len([e for e in events if e["kind"] == REQUEST])))
    if burst is None:
        print("no trigger: the whole file is pre-roll")
    else:
        after = len([e for e in events[burst:] if e["kind"] == STARTED])
        print("burst at event %d: %d frame(s) before it, %d after" %
              (burst, len(frames) - after, after))
    if len(frames) > 1:
        span = (frames[-1]["timestamp"] - frames[0]["timestamp"]) / 1e9
        if span > 0:
            print("%.1f s of sensor time, %.1f frames/s" % (span, (len(frames) - 1) / span))


def cmd_meta(path, which=None):
    _header, cameras, _streams, _events, _burst = read_all(path)
    for camera in cameras:
        if which and camera["id"] != which:
            continue
        print("== camera %s: %d entries ==" % (camera["id"], len(camera["bag"])))
        for tag, kind, count, name, data in camera["bag"]:
            print("%s (0x%08x) %s[%d] = %s" %
                  (name or TAGS.get(tag, "<tag>"), tag, TYPES.get(kind, ("?",))[0], count,
                   bag_value(kind, count, data)))


def cmd_frames(path):
    _header, _cameras, streams, events, burst = read_all(path)
    index = 0
    for i, event in enumerate(events):
        if event["kind"] != BUFFER:
            continue
        planes = "+".join(str(p["len"]) for p in event["planes"])
        print("%4d %s stream %d %dx%d %-11s ts %d %s bytes%s%s" %
              (index, "burst" if burst is not None and i >= burst else "  pre",
               event["stream"], event["width"], event["height"], fmt_name(event["format"]),
               event["timestamp"], planes or "0",
               " (reference)" if event["flags"] & 1 else "",
               "" if event["stream"] < len(streams) else " <unknown stream>"))
        index += 1


def cmd_requests(path):
    _header, _cameras, _streams, events, burst = read_all(path)
    index = 0
    for i, event in enumerate(events):
        if event["kind"] != REQUEST:
            continue
        print("request %d: %s, targets 0x%x%s, %d setting(s)%s" %
              (event["request"], "repeating" if event["repeating"] else "one-shot",
               event["targets"], ", reprocess" if event["reprocess"] else "", len(event["bag"]),
               "" if burst is None or i < burst else "  <- the burst's"))
        for tag, kind, count, name, data in sorted(event["bag"], key=lambda e: e[0]):
            print("    %s (0x%08x) = %s" %
                  (name or TAGS.get(tag, "<tag>"), tag, bag_value(kind, count, data)))
        index += 1
    if not index:
        print("no requests in this recording")


def cmd_dump(path, out):
    _header, _cameras, _streams, events, _burst = read_all(path)
    os.makedirs(out, exist_ok=True)
    index = 0
    for event in events:
        if event["kind"] != BUFFER:
            continue
        for n, plane in enumerate(event["planes"]):
            name = "%s/frame%04d-stream%d-%s-%dx%d-stride%d.plane%d" % (
                out, index, event["stream"], fmt_name(event["format"]), event["width"],
                event["height"], plane["row_stride"], n)
            with open(name, "wb") as f:
                f.write(plane["data"])
            print(name)
        index += 1


def nv21_to_png(nv21, width, height, stride, path):
    """BT.601 video range, and a PNG written by hand - no pillow needed."""
    import zlib
    rows = []
    for y in range(height):
        row = bytearray([0])
        luma = y * stride
        chroma = (height + y // 2) * stride
        for x in range(width):
            yy = (nv21[luma + x] - 16) * 1.164
            cr = nv21[chroma + (x & ~1)] - 128
            cb = nv21[chroma + (x & ~1) + 1] - 128
            row += bytes((min(255, max(0, int(yy + 1.596 * cr))),
                          min(255, max(0, int(yy - 0.391 * cb - 0.813 * cr))),
                          min(255, max(0, int(yy + 2.018 * cb)))))
        rows.append(bytes(row))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 6)))
        f.write(chunk(b"IEND", b""))


def cmd_png(path, out):
    _header, _cameras, _streams, events, _burst = read_all(path)
    os.makedirs(out, exist_ok=True)
    index, written = 0, 0
    for event in events:
        if event["kind"] != BUFFER:
            continue
        plane = event["planes"][0] if event["planes"] else None
        if plane and event["flags"] & 1:
            name = "%s/frame%04d-stream%d.png" % (out, index, event["stream"])
            nv21_to_png(plane["data"], event["width"], event["height"], plane["row_stride"], name)
            print(name)
            written += 1
        index += 1
    if not written:
        print("no reference frames in this recording "
              "(ATL_CAMERA_RECORD_PREVIEW=0 turns them off)")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    command, path, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    if command == "list":
        cmd_list(path)
    elif command == "info":
        cmd_info(path)
    elif command == "meta":
        cmd_meta(path, rest[0] if rest else None)
    elif command == "frames":
        cmd_frames(path)
    elif command == "requests":
        cmd_requests(path)
    elif command == "dump":
        cmd_dump(path, rest[0] if rest else "frames")
    elif command == "png":
        cmd_png(path, rest[0] if rest else "frames")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
