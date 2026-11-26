#!/usr/bin/env python3
"""Copy prelude.lox into every example that embeds it.

clox has no imports, so examples that need number formatting carry their own
copy of the prelude between its first and last marker lines. Edit prelude.lox,
then run this to update every copy. A `// @prelude` line is expanded too.
"""
import pathlib

here = pathlib.Path(__file__).parent
prelude = (here / "prelude.lox").read_text().rstrip("\n")
first, last = prelude.split("\n")[0], prelude.split("\n")[-1]

for f in sorted(here.glob("[0-9]*.lox")):
    text = f.read_text()
    if "// @prelude" in text:
        new = text.replace("// @prelude", prelude)
    elif first in text:
        start = text.index(first)
        end = text.index(last, start) + len(last)
        new = text[:start] + prelude + text[end:]
    else:
        continue
    if new != text:
        f.write_text(new)
        print("updated", f.name)
