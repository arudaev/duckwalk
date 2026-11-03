"""Text hygiene for everything people read in this repository.

Fails on:
  - invisible or format characters (zero-width spaces and joiners, bidi
    controls, soft hyphens, tag characters, byte-order marks) and unusual
    space characters, in any tracked text file;
  - em dashes (U+2014) in prose: this project uses en dashes (U+2013);
  - stock phrases that make writing read as generated, in Markdown files.

Usage:
    python scripts/check_text.py            # every tracked text file
    python scripts/check_text.py FILE ...   # just these files

No dependencies beyond the Python standard library.
"""

import re
import subprocess
import sys
import unicodedata
from pathlib import Path

TEXT_SUFFIXES = {".md", ".txt", ".yml", ".yaml", ".c", ".h", ".py", ".fam", ".cjs", ".js", ".json", ".svg"}
PROSE_SUFFIXES = {".md", ".txt"}

# Characters a human almost never types on purpose.
ALLOWED_FORMAT = set()
ODD_SPACES = {chr(c) for c in (0x00A0, *range(0x2000, 0x200B), 0x202F, 0x205F, 0x3000)}
EM_DASH = chr(0x2014)

STOCK_PHRASES = [
    "delve", "tapestry", "testament to", "seamless", "seamlessly", "leverage", "game-changer",
    "game changer", "in today's", "it is important to note", "it's important to note",
    "unlock the power", "harness the power", "elevate your", "thrilled to", "passionate about",
    "embark on", "journey", "navigate the complexities", "a myriad of", "plays a crucial role",
    "cutting-edge", "revolutionize", "supercharge", "in conclusion", "furthermore,",
    "moreover,", "rest assured", "look no further",
]
PHRASE_RE = re.compile(r"\b(" + "|".join(re.escape(p) for p in STOCK_PHRASES) + r")", re.IGNORECASE)
CODE_FENCE = re.compile(r"```.*?```", re.DOTALL)
INLINE_CODE = re.compile(r"`[^`\n]*`")


def tracked_files():
    out = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True).stdout
    return [Path(p) for p in out.splitlines()]


def bad_char(ch):
    if ch in ODD_SPACES:
        return "unusual space"
    cat = unicodedata.category(ch)
    if cat == "Cf" and ch not in ALLOWED_FORMAT:
        return "invisible format character"
    if cat == "Co":
        return "private-use character"
    return None


def blank_code(text):
    """Replace code spans with spaces so positions keep their line numbers."""
    keep = lambda m: re.sub(r"[^\n]", " ", m.group(0))
    return INLINE_CODE.sub(keep, CODE_FENCE.sub(keep, text))


def check(path):
    problems = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return [f"{path}: not valid UTF-8"]
    for lineno, line in enumerate(text.splitlines(), 1):
        for col, ch in enumerate(line, 1):
            kind = bad_char(ch)
            if kind:
                problems.append(f"{path}:{lineno}:{col}: {kind} U+{ord(ch):04X} {unicodedata.name(ch, '')}")
    if path.suffix in PROSE_SUFFIXES or path.suffix in {".c", ".h", ".py", ".yml"}:
        for lineno, line in enumerate(text.splitlines(), 1):
            col = line.find(EM_DASH)
            if col >= 0:
                problems.append(f"{path}:{lineno}:{col + 1}: em dash; use an en dash (U+2013)")
    if path.suffix in PROSE_SUFFIXES:
        prose = blank_code(text)
        for lineno, line in enumerate(prose.splitlines(), 1):
            for m in PHRASE_RE.finditer(line):
                problems.append(f"{path}:{lineno}:{m.start() + 1}: stock phrase \"{m.group(0)}\"")
    return problems


def main(argv):
    paths = [Path(p) for p in argv] if argv else tracked_files()
    paths = [p for p in paths if p.suffix in TEXT_SUFFIXES and p.name != "LICENSE" and p.is_file()]
    problems = [msg for p in paths for msg in check(p)]
    for msg in problems:
        print(msg)
    print(f"checked {len(paths)} files, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
