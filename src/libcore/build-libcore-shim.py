#!/usr/bin/env python3
"""Build libcore-shim.jar, the java.* classes ATL puts in front of ART's boot
class path.

Neither tool will touch java.* by default: javac needs -bootclasspath (and an
old -source) before it lets a source file declare one, and dx needs
--core-library.  --min-sdk-version=24 keeps the default methods as real default
methods instead of desugaring them away.
"""
import os
import subprocess
import sys
import tempfile

out, core_all = sys.argv[1], sys.argv[2]
srcs = sys.argv[3:]

with tempfile.TemporaryDirectory() as classes:
	subprocess.run(["javac", "-source", "8", "-target", "8", "-nowarn",
	                "-encoding", "UTF-8", "-bootclasspath", core_all,
	                "-d", classes] + srcs, check=True)
	subprocess.run(["dx", "--dex", "--core-library", "--min-sdk-version=24",
	                "--output=" + os.path.abspath(out), classes], check=True)
