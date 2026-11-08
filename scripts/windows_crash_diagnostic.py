#!/usr/bin/env python3
"""Run the normal tests and diagnose the Windows access violation on failure."""

import os
from pathlib import Path
import shutil
import subprocess
import sys


def find_cdb():
    executable = shutil.which("cdb.exe")
    if executable:
        return executable
    for variable in ("ProgramFiles(x86)", "ProgramFiles"):
        root = os.environ.get(variable)
        if root:
            for kit in ("10", "11"):
                candidate = Path(root) / "Windows Kits" / kit / "Debuggers" / "x64" / "cdb.exe"
                if candidate.is_file():
                    return str(candidate)
    return None


def diagnose(binary, test_file):
    print(f"\n=== Windows crash diagnostic: {test_file} ===", flush=True)
    pdbs = sorted(Path("build/release").rglob("*.pdb"))
    print(f"Local PDB files: {len(pdbs)}", flush=True)
    for pdb in pdbs:
        print(f"  {pdb}", flush=True)

    cdb = find_cdb()
    environment = os.environ.copy()
    environment["_NT_SYMBOL_PATH"] = str(Path("build/release").resolve())
    if cdb:
        commands = (
            'sxe -c ".echo === FIRST CHANCE ACCESS VIOLATION ===; '
            '.ecxr; !analyze -v; .exr -1; r; kv; lm; q" av; g'
        )
        command = [cdb, "-lines", "-c", commands, binary, test_file]
    else:
        print("cdb.exe not found in PATH or Windows Kits; trying gdb.", flush=True)
        gdb = shutil.which("gdb.exe") or shutil.which("gdb")
        if not gdb:
            print("ERROR: Neither cdb nor gdb is available; no stack captured.", flush=True)
            return
        command = [
            gdb,
            "--batch",
            "-ex",
            "set pagination off",
            "-ex",
            "handle SIGSEGV stop print nopass",
            "-ex",
            "run",
            "-ex",
            "thread apply all bt",
            "-ex",
            "info sharedlibrary",
            "--args",
            binary,
            test_file,
        ]

    print(f"Debugger: {command[0]}", flush=True)
    try:
        result = subprocess.run(command, env=environment, timeout=300, check=False)
        print(f"Debugger exit code: {result.returncode}", flush=True)
    except subprocess.TimeoutExpired:
        print("ERROR: Debugger timed out after 300 seconds.", flush=True)
    except OSError as error:
        print(f"ERROR: Could not start debugger: {error}", flush=True)
    print("=== End Windows crash diagnostic ===", flush=True)


def main():
    if len(sys.argv) < 2:
        print("ERROR: Expected unittest binary argument.", file=sys.stderr)
        return 2
    runner = Path("duckdb/scripts/ci/run_tests.py")
    result = subprocess.run([sys.executable, str(runner), *sys.argv[1:]], check=False)
    if result.returncode and sys.platform == "win32":
        for test_file in (
            "test/sql/pdal.test",
            "test/sql/pdal_raster.test",
            "test/sql/pdal_read_where.test",
        ):
            diagnose(sys.argv[1], test_file)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
