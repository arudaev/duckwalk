"""Check a release tag against the app's version.

A tag is `v` + `fap_version` from app/application.fam (a release, like v0.1),
optionally followed by `-rcN` (a pre-release, like v0.1-rc1). Anything else
is refused, so a typo can't publish a release.

Usage:
    python scripts/release_tag.py TAG   # prints prerelease=true or false
    python -m doctest scripts/release_tag.py

>>> kind("v0.1-rc1", "0.1")
'true'
>>> kind("v0.1", "0.1")
'false'
>>> kind("v0.1-rc12", "0.1")
'true'
>>> kind("v0.2-rc1", "0.1")
Traceback (most recent call last):
ValueError: Tag v0.2-rc1 is for version 0.2, but fap_version is 0.1
>>> kind("v0.1-beta", "0.1")
Traceback (most recent call last):
ValueError: Tag v0.1-beta isn't vX.Y or vX.Y-rcN
>>> kind("v0.1-rc", "0.1")
Traceback (most recent call last):
ValueError: Tag v0.1-rc isn't vX.Y or vX.Y-rcN
>>> kind("0.1", "0.1")
Traceback (most recent call last):
ValueError: Tag 0.1 isn't vX.Y or vX.Y-rcN
"""

import pathlib
import re
import sys

TAG = re.compile(r"v(\d+\.\d+)(-rc[1-9]\d*)?")


def kind(tag: str, fap_version: str) -> str:
    """Return "true" for a pre-release tag, "false" for a release tag."""
    match = TAG.fullmatch(tag)
    if not match:
        raise ValueError(f"Tag {tag} isn't vX.Y or vX.Y-rcN")
    if match.group(1) != fap_version:
        raise ValueError(f"Tag {tag} is for version {match.group(1)}, but fap_version is {fap_version}")
    return "true" if match.group(2) else "false"


def fap_version() -> str:
    fam = pathlib.Path(__file__).resolve().parent.parent / "app" / "application.fam"
    match = re.search(r'fap_version="([^"]*)"', fam.read_text(encoding="utf-8"))
    if not match:
        raise ValueError("No fap_version in app/application.fam")
    return match.group(1)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: python scripts/release_tag.py TAG")
    try:
        print(f"prerelease={kind(sys.argv[1], fap_version())}")
    except ValueError as error:
        sys.exit(str(error))
