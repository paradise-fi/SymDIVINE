#!/usr/bin/env python

"""Usage: compile_to_bitcode.py <source>  [options]
"""

import sys
import subprocess
import os

LART_PATH = "./bin/lart"

def change_suffix(src, suffix):
    out, _ = os.path.splitext(src)
    out += "." + suffix
    return out

def compile_benchmark(src, args, output = None, fix_inline=False):
    if not output:
        output = change_suffix(src, "ll")

    if fix_inline:
        args.append('-fgnu89-inline')
    cmd = "clang -c -O0 -emit-llvm -o {0} {1}".format(output, src)
    print(cmd)

    if os.system(cmd) != 0:
        print("ERROR")
        print("Compilation failed")
        return ""

    cmd = LART_PATH + " {0} {1} main-volatilize".format(output, output)
    print(cmd)
    if os.system(cmd) != 0:
        print("ERROR")
        print("Compilation failed")
        return ""
    """
    cmd = "clang -S -emit-llvm {0} -o {1} {2}".format(' '.join(args), output, output)
    print(cmd)
    if os.system(cmd) != 0:
        print("ERROR")
        print("Compilation failed")
        return ""
    """
    return output

if __name__ == "__main__":
    if len(sys.argv) == 1:
        print(__def__)
        sys.exit(1)
    it = iter(sys.argv)
    it.next()
    src = it.next()
    args = list(it)
    compile_benchmark(src, args, fix_inline=True)
