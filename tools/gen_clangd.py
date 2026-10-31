#!/usr/bin/env python3
"""Write a .clangd so editors resolve libsaturn's includes.

The SDK cross-compiles with the SH-2 toolchain, so there is no compile
database an editor's clang can use. This writes the include paths the build
passes ("include/", the repository root for "src/..." includes, the generated
headers and the shared example headers) as absolute paths into the
git-ignored .clangd at the repository root.

    python3 tools/gen_clangd.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    includes = [os.path.join(ROOT, d) for d in ("include", "build/generated", "examples/common")]
    flags = [f"-I{d}" for d in includes]
    # -iquote like the build: on -I the root's VERSION file shadows libc++'s <version>.
    flags += ["-iquote", ROOT]
    lines = ["CompileFlags:", "  Add:"] + [f"    - {f}" for f in flags]
    # The runtime is C++20 and the examples are C; pick the standard by extension.
    # A lone .h would be guessed as Objective-C++; the public headers are C.
    lines += ["---", "If:", "  PathMatch: .*\\.(cpp|hpp|cc)$", "CompileFlags:", "  Add: [-std=c++20]",
              "---", "If:", "  PathMatch: .*\\.c$", "CompileFlags:", "  Add: [-std=gnu11]",
              "---", "If:", "  PathMatch: .*\\.h$", "CompileFlags:", "  Add: [-xc, -std=gnu11]",
              "---", "Diagnostics:", "  UnusedIncludes: None", "  MissingIncludes: None"]
    path = os.path.join(ROOT, ".clangd")
    with open(path, "w", encoding="utf-8") as out:
        out.write("\n".join(lines) + "\n")
    print(f"wrote {path} ({len(includes) + 1} include directories)")


if __name__ == "__main__":
    main()
