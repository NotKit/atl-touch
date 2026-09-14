#!/usr/bin/env python3
"""Generate the camera2 metadata tag bridge from the vendored NDK header.

Reads third_party/android-headers/camera/NdkCameraMetadataTags.h and writes:

  src/api-impl-jni/camera/camera2_tags.inc                  name <-> tag <-> type
  src/api-impl/android/hardware/camera2/CameraMetadata.java  the enum constants

Both outputs are committed; re-run this after updating the header.

The NDK tag ids are the framework's own (ACAMERA_X == ANDROID_X), so the table
doubles as the java-name mapping: section index << 16 | offset is the tag, and
"android.<section>.<lowerCamelCase suffix>" is the Key name apps use.
"""

import os
import re
import sys

# Java prefix per header section. The suffix->lowerCamelCase rule is mechanical,
# the section names are not (colorCorrection is one word, sensor.info is two), so
# they come from AOSP's camera_metadata_tag_info.c. A section missing here is an
# error rather than a guess.
SECTIONS = {
    "ACAMERA_COLOR_CORRECTION": "colorCorrection",
    "ACAMERA_CONTROL": "control",
    "ACAMERA_DEMOSAIC": "demosaic",
    "ACAMERA_EDGE": "edge",
    "ACAMERA_FLASH": "flash",
    "ACAMERA_FLASH_INFO": "flash.info",
    "ACAMERA_HOT_PIXEL": "hotPixel",
    "ACAMERA_JPEG": "jpeg",
    "ACAMERA_LENS": "lens",
    "ACAMERA_LENS_INFO": "lens.info",
    "ACAMERA_NOISE_REDUCTION": "noiseReduction",
    "ACAMERA_QUIRKS": "quirks",
    "ACAMERA_REQUEST": "request",
    "ACAMERA_SCALER": "scaler",
    "ACAMERA_SENSOR": "sensor",
    "ACAMERA_SENSOR_INFO": "sensor.info",
    "ACAMERA_SHADING": "shading",
    "ACAMERA_STATISTICS": "statistics",
    "ACAMERA_STATISTICS_INFO": "statistics.info",
    "ACAMERA_TONEMAP": "tonemap",
    "ACAMERA_LED": "led",
    "ACAMERA_INFO": "info",
    "ACAMERA_BLACK_LEVEL": "blackLevel",
    "ACAMERA_SYNC": "sync",
    "ACAMERA_REPROCESS": "reprocess",
    "ACAMERA_DEPTH": "depth",
    "ACAMERA_LOGICAL_MULTI_CAMERA": "logicalMultiCamera",
    "ACAMERA_DISTORTION_CORRECTION": "distortionCorrection",
    "ACAMERA_HEIC": "heic",
    "ACAMERA_HEIC_INFO": "heic.info",
    "ACAMERA_AUTOMOTIVE": "automotive",
    "ACAMERA_AUTOMOTIVE_LENS": "automotive.lens",
    "ACAMERA_EXTENSION": "extension",
    "ACAMERA_JPEGR": "jpegr",
}

TYPES = {
    "byte": "ATL_CAMERA2_TYPE_BYTE",
    "int32": "ATL_CAMERA2_TYPE_INT32",
    "float": "ATL_CAMERA2_TYPE_FLOAT",
    "int64": "ATL_CAMERA2_TYPE_INT64",
    "double": "ATL_CAMERA2_TYPE_DOUBLE",
    "rational": "ATL_CAMERA2_TYPE_RATIONAL",
}

# a tag whose name is too long for the column the header comments in gets its
# type on the line below, so the comment is optional here and read separately
TAG_RE = re.compile(r"^    (ACAMERA_[A-Z0-9_]+) =\s*(?://\s*(\S+).*)?$")
TYPE_RE = re.compile(r"^\s*//\s*(\S+)")
VALUE_RE = re.compile(r"^\s+(ACAMERA_[A-Z0-9_]+)_START(?:\s*\+\s*(\d+))?,")
ENUM_START_RE = re.compile(r"^typedef enum acamera_metadata_enum_")
ENUM_CONST_RE = re.compile(r"^\s+(ACAMERA_[A-Z0-9_]+)\s*=\s*(0x[0-9a-fA-F]+|-?\d+)")


def camel(words):
    return words[0].lower() + "".join(w.capitalize() for w in words[1:])


def java_name(tag, section):
    suffix = tag[len(section) + 1:]
    return "android." + SECTIONS[section] + "." + camel(suffix.split("_"))


def parse(header):
    lines = header.splitlines()

    order = []
    in_sections = False
    for line in lines:
        if line.startswith("typedef enum acamera_metadata_section {"):
            in_sections = True
            continue
        if in_sections:
            if line.startswith("}"):
                break
            name = line.strip().rstrip(",").split(" ")[0]
            if name.startswith("ACAMERA_") and not name.endswith("_COUNT") and name != "ACAMERA_VENDOR":
                order.append(name)
    section_index = {name: i for i, name in enumerate(order)}

    missing = [s for s in order if s not in SECTIONS]
    if missing:
        sys.exit("unknown metadata sections %s: add them to SECTIONS" % missing)

    tags = []
    for i, line in enumerate(lines):
        match = TAG_RE.match(line)
        if not match:
            continue
        name, ctype = match.group(1), match.group(2)
        rest = i + 1
        if ctype is None:
            comment = TYPE_RE.match(lines[rest])
            if not comment:
                sys.exit("no type for %s" % name)
            ctype = comment.group(1)
            rest += 1
        value = VALUE_RE.match(lines[rest])
        if not value:
            sys.exit("no value for %s" % name)
        section = value.group(1)
        tag = (section_index[section] << 16) + int(value.group(2) or 0)
        base = ctype.split("[")[0]
        if base == "Deprecated!":
            # the header drops the type of deprecated tags; they stay nameless
            # and are still readable by tag id
            continue
        if base not in TYPES:
            sys.exit("unknown type %r for %s" % (ctype, name))
        tags.append((tag, java_name(name, section), TYPES[base]))

    constants = {}
    in_enum = False
    for line in lines:
        if ENUM_START_RE.match(line):
            in_enum = True
            continue
        if in_enum:
            if line.startswith("}"):
                in_enum = False
                continue
            match = ENUM_CONST_RE.match(line)
            if match:
                name = match.group(1)[len("ACAMERA_"):]
                value = int(match.group(2), 0)
                if constants.get(name, value) != value:
                    sys.exit("conflicting values for %s" % name)
                constants[name] = value

    return sorted(tags), constants


# Enum values the platform has but the vendored NDK header does not: the header
# is older than the android.jar the api-impl is checked against, and dropping
# these would take constants off CameraMetadata that master already ships.
EXTRA_CONSTANTS = {
    "COLOR_CORRECTION_MODE_CCT": 3,
    "CONTROL_AE_PRIORITY_MODE_OFF": 0,
    "CONTROL_AE_PRIORITY_MODE_SENSOR_EXPOSURE_TIME_PRIORITY": 2,
    "CONTROL_AE_PRIORITY_MODE_SENSOR_SENSITIVITY_PRIORITY": 1,
    "CONTROL_SCENE_MODE_HIGH_SPEED_VIDEO": 17,
    "CONTROL_ZOOM_METHOD_AUTO": 0,
    "CONTROL_ZOOM_METHOD_ZOOM_RATIO": 1,
    "EXTENSION_NIGHT_MODE_INDICATOR_OFF": 1,
    "EXTENSION_NIGHT_MODE_INDICATOR_ON": 2,
    "EXTENSION_NIGHT_MODE_INDICATOR_UNKNOWN": 0,
    "INFO_DEVICE_TYPE_BUILT_IN": 0,
    "INFO_DEVICE_TYPE_EXTERNAL": 1,
    "INFO_DEVICE_TYPE_UNKNOWN": 3,
    "INFO_DEVICE_TYPE_VIRTUAL": 2,
    "REQUEST_AVAILABLE_CAPABILITIES_COLOR_SPACE_PROFILES": 20,
    "REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO": 9,
    "REQUEST_AVAILABLE_CAPABILITIES_DYNAMIC_RANGE_TEN_BIT": 18,
    "REQUEST_AVAILABLE_CAPABILITIES_OFFLINE_PROCESSING": 15,
    "REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING": 4,
    "REQUEST_AVAILABLE_CAPABILITIES_REMOSAIC_REPROCESSING": 17,
    "REQUEST_AVAILABLE_CAPABILITIES_ULTRA_HIGH_RESOLUTION_SENSOR": 16,
    "REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING": 7,
    "SENSOR_READOUT_TIMESTAMP_HARDWARE": 1,
    "SENSOR_READOUT_TIMESTAMP_NOT_SUPPORTED": 0,
}

PREAMBLE = "/* Generated by tools/gen-camera2-tags.py, do not edit. */"

JAVA_HEAD = """package android.hardware.camera2;

import android.hardware.camera2.impl.CameraMetadataNative;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Generated by tools/gen-camera2-tags.py, do not edit.
 *
 * The metadata enum constants, straight from the NDK tag header. Subclasses
 * (CameraCharacteristics, CaptureRequest, CaptureResult) inherit them and
 * override getKeys() and getAtlBag().
 */
public abstract class CameraMetadata<TKey> {
\tprotected CameraMetadata() {
\t}

\tpublic List<TKey> getKeys() {
\t\treturn Collections.emptyList();
\t}

\t/** the bag these keys come from; null only for a subclass that has none. */
\tCameraMetadataNative getAtlBag() {
\t\treturn null;
\t}

\t/**
\t * AOSP's package-private key enumerator. Camera apps call it by reflection
\t * to reach the vendor keys, so the signature has to match theirs exactly.
\t *
\t * Every subclass here already lists just the keys its own bag holds, and
\t * ATL has no synthetic keys, so only the tag filter is left to apply. A
\t * subclass with no bag cannot resolve tags and keeps the whole list.
\t */
\tstatic <TKey> ArrayList<TKey> getKeys(Class<?> type, Class<TKey> keyClass,
\t    CameraMetadata<TKey> instance, int[] filterTags, boolean includeSynthetic) {
\t\tArrayList<TKey> keys = new ArrayList<TKey>(instance.getKeys());
\t\tCameraMetadataNative bag = instance.getAtlBag();

\t\tif (filterTags == null || bag == null)
\t\t\treturn keys;

\t\tArrayList<TKey> filtered = new ArrayList<TKey>();
\t\tfor (TKey key : keys) {
\t\t\tint tag = bag.getTag(nameOf(key));

\t\t\tfor (int want : filterTags) {
\t\t\t\tif (want == tag) {
\t\t\t\t\tfiltered.add(key);
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t}
\t\t}
\t\treturn filtered;
\t}

\t/** the three Key classes share no supertype, only a getName(). */
\tprivate static String nameOf(Object key) {
\t\tif (key instanceof CameraCharacteristics.Key)
\t\t\treturn ((CameraCharacteristics.Key<?>)key).getName();
\t\tif (key instanceof CaptureRequest.Key)
\t\t\treturn ((CaptureRequest.Key<?>)key).getName();
\t\tif (key instanceof CaptureResult.Key)
\t\t\treturn ((CaptureResult.Key<?>)key).getName();
\t\treturn "";
\t}

"""


def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    header_path = os.path.join(repo, "third_party/android-headers/camera/NdkCameraMetadataTags.h")
    with open(header_path) as f:
        tags, constants = parse(f.read())

    inc = [PREAMBLE, "", "static const struct atl_camera2_tag_info atl_camera2_tags[] = {"]
    for tag, name, ctype in tags:
        inc.append('\t{0x%08x, "%s", %s},' % (tag, name, ctype))
    inc += ["};", ""]
    with open(os.path.join(repo, "src/api-impl-jni/camera/camera2_tags.inc"), "w") as f:
        f.write("\n".join(inc))

    for name, value in EXTRA_CONSTANTS.items():
        constants.setdefault(name, value)

    java = [JAVA_HEAD]
    for name in sorted(constants):
        java.append("\tpublic static final int %s = %d;\n" % (name, constants[name]))
    java.append("}\n")
    with open(os.path.join(repo, "src/api-impl/android/hardware/camera2/CameraMetadata.java"), "w") as f:
        f.write("".join(java))

    print("%d tags, %d constants" % (len(tags), len(constants)))


if __name__ == "__main__":
    main()
