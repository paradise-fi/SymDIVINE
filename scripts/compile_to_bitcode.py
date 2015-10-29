#!/usr/bin/env python

"""Usage: compile_to_bitcode.py <source>  [options]
"""

import sys
import subprocess
import os

def change_suffix(src, suffix):
    out, _ = os.path.splitext(src)
    out += "." + suffix
    return out

def compile_benchmark(src, args, output = None, fix_inline=False, fix_volatile=False, lart_path=None, supress_output=False):
    if not output:
        output = change_suffix(src, "ll")

    if fix_inline:
        args.append('-fgnu89-inline')

    suff = ""
    if supress_output:
        suff = " 2> /dev/null"

    if fix_volatile:
        lart_path = os.path.join(lart_path, "lart")
        args_no_opt = [x for x in args if not x.startswith("-O")]
        cmd = "clang -c -m32 -fno-inline -emit-llvm {0} -o {1} {2}".format(' '.join(args_no_opt), output, src) + suff
        if not supress_output:
            print cmd

        if os.system(cmd) != 0:
            print("ERROR")
            print("First phase compilation failed")
            return ""

        cmd = lart_path + " {0} {1} main-volatilize".format(output, output) + suff
        if not supress_output:
            print cmd

        if os.system(cmd) != 0:
            print("ERROR")
            print("Transformation failed")
            return ""

        cmd = "clang -S -m32 -fno-inline -emit-llvm {0} -o {1} {2}".format(' '.join(args_no_opt), output, output) + suff
        if not supress_output:
            print cmd

        if os.system(cmd) != 0:
            print("ERROR")
            print("Second phase compilation failed")
            return ""
    else:
        cmd = "clang -S -m32 -fno-inline -emit-llvm {0} -o {1} {2}".format(' '.join(args), output, src) + suff
        if not supress_output:
            print cmd

        if os.system(cmd) != 0:
            print("ERROR")
            print("Compilation failed")
            return ""
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
