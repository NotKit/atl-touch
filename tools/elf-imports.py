#!/usr/bin/env python3
# List an ELF shared object's DT_NEEDED entries and its undefined (imported)
# dynamic symbols, with no readelf/nm on the machine that runs it.
#
#   tools/elf-imports.py <library> [--needed-only]
#
# Written for the device: caiman has no binutils, and the library under test
# (Google Camera's libgcastartup.so) is 175 MB, so copying it to a desktop to
# run nm on it is not the cheap answer.  Only the ELF header, the program
# headers and the dynamic section's string/symbol tables are read.
import struct
import sys


def sections(f):
	f.seek(0)
	eh = f.read(64)
	if eh[:4] != b"\x7fELF" or eh[4] != 2:
		raise SystemExit("not a 64-bit ELF")
	phoff, = struct.unpack_from("<Q", eh, 0x20)
	phentsize, phnum = struct.unpack_from("<HH", eh, 0x36)
	f.seek(phoff)
	ph = f.read(phentsize * phnum)
	loads, dyn = [], None
	for i in range(phnum):
		o = i * phentsize
		p_type, = struct.unpack_from("<I", ph, o)
		p_off, p_va = struct.unpack_from("<QQ", ph, o + 8)
		p_filesz, = struct.unpack_from("<Q", ph, o + 32)
		if p_type == 1:
			loads.append((p_va, p_off, p_filesz))
		elif p_type == 2:
			dyn = (p_off, p_filesz)
	if dyn is None:
		raise SystemExit("no PT_DYNAMIC")
	return loads, dyn


def main():
	path = sys.argv[1]
	needed_only = "--needed-only" in sys.argv
	f = open(path, "rb")
	loads, (dyn_off, dyn_size) = sections(f)

	def v2o(v):
		for va, off, size in loads:
			if va <= v < va + size:
				return off + (v - va)
		return None

	f.seek(dyn_off)
	d = f.read(dyn_size)
	tags = []
	for i in range(len(d) // 16):
		tag, val = struct.unpack_from("<QQ", d, i * 16)
		tags.append((tag, val))
		if tag == 0:
			break
	def one(t, default=None):
		v = [val for tag, val in tags if tag == t]
		return v[0] if v else default

	strtab, strsz = one(5), one(10, 0)
	symtab, syment = one(6), one(11, 24)
	f.seek(v2o(strtab))
	st = f.read(strsz)

	def s(i):
		return st[i:st.index(b"\0", i)].decode(errors="replace")

	print("soname:", *[s(v) for t, v in tags if t == 14])
	for t, v in tags:
		if t == 1:
			print("needed:", s(v))
	if needed_only:
		return

	# the symbol table has no length of its own; DT_GNU_HASH's bucket chain is
	# the usual way to find the last symbol, but the string table always starts
	# right after it in practice, so the gap between them bounds the count
	end = strtab if strtab > symtab else symtab + syment * 65536
	count = (end - symtab) // syment
	f.seek(v2o(symtab))
	sym = f.read(count * syment)
	for i in range(count):
		o = i * syment
		if o + syment > len(sym):
			break
		st_name, st_info, st_other, st_shndx = struct.unpack_from("<IBBH", sym, o)
		if st_shndx != 0 or st_name == 0:
			continue
		kind = {1: "OBJECT", 2: "FUNC", 6: "TLS"}.get(st_info & 0xF, str(st_info & 0xF))
		print("undef:", s(st_name), kind)


main()
