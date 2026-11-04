#!/usr/bin/env python3
"""Turn core/app/remote_page.html, the phone remote's page, into C.

The page is served from flash as one string, so run this after changing it:

    python3 tools/gen_remote_page.py

It writes core/app/remote_page.c, leaving out the indentation and blank
lines to make it a little smaller.
"""
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, "core", "app", "remote_page.html")
OUT = os.path.join(REPO, "core", "app", "remote_page.c")


def c_string(line):
    return '"' + line.replace("\\", "\\\\").replace('"', '\\"') + '\\n"'


def main():
    with open(SOURCE, encoding="utf-8") as f:
        lines = [line.strip() for line in f]
    lines = [line for line in lines if line]
    data = "\n".join(lines) + "\n"

    body = "\n".join("    " + c_string(line) for line in lines)
    text = (
        "/*\n"
        " * The phone remote's page, from remote_page.html by tools/gen_remote_page.py.\n"
        " * Edit the page and run the script rather than editing this file.\n"
        " */\n"
        "#include <stddef.h>\n\n"
        "#if RCAT_HAVE_NETWORK\n\n"
        "const char remote_page[] =\n"
        f"{body};\n\n"
        "const size_t remote_page_len = sizeof(remote_page) - 1;\n\n"
        "#endif\n"
    )
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"{OUT}: {len(data.encode('utf-8'))} bytes")


if __name__ == "__main__":
    main()
