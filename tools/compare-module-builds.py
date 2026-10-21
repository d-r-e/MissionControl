#!/usr/bin/env python3
"""Compare two MissionControl .nsp files before one of them goes on a card.

Answers three questions, none of which need a loader to answer:

- does the module ask for anything it did not ask for before? (`main.npdm` is the capability
  declaration: kernel flags, syscalls, service accesses, io/as DRAM) - compared byte for byte;
- what grew, and is the growth the size you expect for the code that was added?
- is there anything in the container that should never be in it - a keyset path, a machine path,
  an address, a credential?

This is not a signature or a reproducibility check: devkitA64 links with LTO across parallel jobs,
so two builds of one commit are not byte-identical. It is a look at what the artifact contains.
"""

import argparse
import hashlib
import re
import struct
import sys

FORBIDDEN = {
    "the console's address": "192.168.",
    "an ftp port": ":5000",
    "the sd keyset": "prod.keys",
    "any keyset": ".keys",
    "a mac user directory": "/Users/",
    "a mac temp directory": "/var/folders/",
    "a build directory outside the container": "/private/tmp",
    "a credential-shaped word": "password=",
    "an url": "http://",
    "an url": "https://",
}


def entries(data):
    """PFS0, as MissionControl ships it: 24-byte entries, offsets relative to the end of the header."""
    if data[:4] != b"PFS0":
        sys.exit("not a PFS0 container")
    count, string_size, _ = struct.unpack_from("<III", data, 4)
    names, files = 16 + 24 * count, 16 + 24 * count + string_size
    out = {}
    for i in range(count):
        offset, size, name_off, _attr = struct.unpack_from("<QQII", data, 16 + 24 * i)
        if files + offset + size > len(data):
            sys.exit(f"entry {i} runs past the end of the file")
        end = data.index(b"\0", names + name_off)
        out[data[names + name_off:end].decode()] = data[files + offset: files + offset + size]
    return out


def strings(data, minimum=6):
    return [s.decode() for s in re.findall(rb"[ -~]{%d,}" % minimum, data)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("candidate", help="the build about to be installed")
    parser.add_argument("upstream", help="the build it replaces, from the same libnx and image")
    parser.add_argument("--show-strings", action="store_true", help="print the strings the candidate adds")
    args = parser.parse_args()

    mine = open(args.candidate, "rb").read()
    theirs = open(args.upstream, "rb").read()
    mine_files, theirs_files = entries(mine), entries(theirs)

    print(f"candidate {len(mine):8d} bytes {hashlib.sha256(mine).hexdigest()}")
    print(f"upstream  {len(theirs):8d} bytes {hashlib.sha256(theirs).hexdigest()}\n")

    if set(mine_files) != set(theirs_files):
        different = sorted(set(mine_files) ^ set(theirs_files))
        sys.exit("the two builds ship different files: " + repr(different))

    for name in sorted(mine_files):
        a, b = mine_files[name], theirs_files.get(name, b"")
        verdict = "identical" if a == b else "differs"
        print(f"  {name:12s} candidate {len(a):7d} B  upstream {len(b):7d} B  ({len(a) - len(b):+d})  {verdict}")

    # The capability declaration is the part that decides what the module is allowed to do
    npdm_mine, npdm_theirs = mine_files.get("main.npdm", b""), theirs_files.get("main.npdm", b"")
    print(f"\nmain.npdm (capabilities, privileges, service ACLs): {'IDENTICAL to upstream' if npdm_mine == npdm_theirs else 'DIFFERENT - review every byte'}")

    print("\nmust not appear anywhere in either build:")
    bad = False
    for label, blob in (("candidate", mine), ("upstream", theirs)):
        for needle, why in sorted(FORBIDDEN.items()):
            if needle.encode() in blob:
                print(f"  FOUND {needle!r} ({why}) in {label}")
                bad = True
    print("  nothing found" if not bad else "")
    if bad:
        sys.exit(1)

    added = sorted(set(strings(mine_files["main"])) - set(strings(theirs_files["main"])))
    interesting = [s for s in added if "/" in s or ":" in s or "=" in s]
    print(f"\nstrings the candidate adds: {len(added)} total, {len(interesting)} that look like paths or options")
    for s in interesting:
        print(f"  {s}")
    if args.show_strings:
        for s in added:
            print(f"  . {s}")


if __name__ == "__main__":
    main()
