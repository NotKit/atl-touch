#!/usr/bin/env python3
"""Framework API surface of an APK (or of api-impl.jar), straight from the dex.

Usage: tools/dex-fw-refs.py <apk-or-dex-jar> [more...] [-o outdir]

Writes <outdir>/fw-classes.txt (android.* classes referenced but not defined),
<outdir>/fw-methods.txt (class#method for those) and <outdir>/fw-fields.txt
(class#field - the camera2 Key constants live there, and a missing one is a
NoSuchFieldError in the app's <clinit>). Point it at builddir/api-impl.jar to get
what ATL defines instead, and diff the two with comm to get the gap.

No androguard on this box; this parses the dex string/type/method id tables
directly, which is enough for a gap map. It over-reports inherited methods
(a call through a subclass looks missing if only the parent defines it).
"""
import collections
import os
import struct
import sys
import zipfile


def u4(b, o):
	return struct.unpack_from('<I', b, o)[0]


def u2(b, o):
	return struct.unpack_from('<H', b, o)[0]


def uleb(b, o):
	r = s = 0
	while True:
		x = b[o]
		o += 1
		r |= (x & 0x7f) << s
		s += 7
		if not x & 0x80:
			return r, o


def parse(d):
	string_ids_off = u4(d, 60)
	type_ids_size, type_ids_off = u4(d, 64), u4(d, 68)
	field_ids_size, field_ids_off = u4(d, 80), u4(d, 84)
	method_ids_size, method_ids_off = u4(d, 88), u4(d, 92)
	class_defs_size, class_defs_off = u4(d, 96), u4(d, 100)

	def s(i):
		off = u4(d, string_ids_off + 4 * i)
		_, off = uleb(d, off)
		return d[off:d.index(b'\x00', off)].decode('utf-8', 'replace')

	types = [s(u4(d, type_ids_off + 4 * i)) for i in range(type_ids_size)]
	defined = {types[u4(d, class_defs_off + 32 * i)] for i in range(class_defs_size)}
	methods = [(types[u2(d, method_ids_off + 8 * i)], s(u4(d, method_ids_off + 8 * i + 4)))
	           for i in range(method_ids_size)]
	fields = [(types[u2(d, field_ids_off + 8 * i)], s(u4(d, field_ids_off + 8 * i + 4)))
	          for i in range(field_ids_size)]
	return types, defined, methods, fields


def java(desc):
	return desc[1:-1].replace('/', '.') if desc.startswith('L') and desc.endswith(';') else desc


def main(argv):
	outdir = '.'
	if '-o' in argv:
		i = argv.index('-o')
		outdir = argv[i + 1]
		argv = argv[:i] + argv[i + 2:]
	if not argv:
		print(__doc__)
		return 1

	methods = collections.defaultdict(set)
	fields = collections.defaultdict(set)
	defined, referenced = set(), set()
	for path in argv:
		z = zipfile.ZipFile(path)
		for name in z.namelist():
			if not name.endswith('.dex'):
				continue
			types, dfn, ms, fs = parse(z.read(name))
			defined |= dfn
			referenced |= set(types)
			for c, m in ms:
				methods[c].add(m)
			for c, f in fs:
				fields[c].add(f)

	external = {c for c in referenced - defined if c.startswith('Landroid')}
	with open(os.path.join(outdir, 'fw-classes.txt'), 'w') as f:
		f.write('\n'.join(sorted(java(c) for c in external)) + '\n')
	with open(os.path.join(outdir, 'fw-methods.txt'), 'w') as f:
		for c in sorted(external):
			for m in sorted(methods[c]):
				f.write(java(c) + '#' + m + '\n')
	with open(os.path.join(outdir, 'fw-fields.txt'), 'w') as f:
		for c in sorted(external):
			for name in sorted(fields[c]):
				f.write(java(c) + '#' + name + '\n')
	print('defined %d, referenced %d, framework classes %d'
	      % (len(defined), len(referenced), len(external)))
	return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv[1:]))
