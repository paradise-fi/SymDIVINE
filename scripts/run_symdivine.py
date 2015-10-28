#!/usr/bin/env python

"""Usage: run_symdivine.py <symdivine_dir> [options] [<benchmark>]...

Arguments:
    <benchmark>     input *.i file
    <symdivine_dir> location of symdivine

Options:
    -O                    level of optimalizations [default: s]
    --fix_volatile        Enables LART main_volatile transformation
    --fix_inline          Fixes C89 inline function declarations
    --silent              Makes all the compilation steps silent
"""

import sys, os
import compile_to_bitcode

from tempfile import mkdtemp

def run_symdivine(symdivine_location, benchmark, symdivine_params = []):
    cmd = "LD_LIBRARY_PATH=" + symdivine_location + " "
    cmd += os.path.join(symdivine_location, "symdivine") + " reachability " + ' '.join(symdivine_params)
    cmd += " " + benchmark

    return os.system(cmd) == 0

def compile_benchmark(src, opt_level, tmpdir, bin_path):
    out = os.path.join(tmpdir, "model.ll")
    
    compile_to_bitcode.compile_benchmark(src, [opt_level], out, fix_inline=True, fix_volatile=True, lart_path = bin_path)
    return out

def rmrf_tmp_dir(d):
    return os.system('rm -rf {0}'.format(d))

def copy_source_to_tmp(src):
    tmpdir = mkdtemp(prefix='symdivine_')
    basename = os.path.basename(src)

    if os.system('cp {0} {1}/{2}'.format(src, tmpdir, basename)) != 0:
        rmrf_tmp_dir(tmpdir)
        print("Failed to create directory in $TMPDIR")
        sys.exit(2)

    return (tmpdir, '{0}/{1}'.format(tmpdir, basename))

def parse_args():
    args = sys.argv

    if len(args) < 3 or args[1].startswith("-") or args[-1].startswith("-"):
        print(__doc__)
        sys.exit(2)

    opt = "-Os"
    fix_volatile = False
    fix_inline = False
    silent = False
    loc = args[1]
    benchmark = None

    for arg in args[2:]:
        if arg.startswith("-O"):
            opt = arg
        elif arg == "--fix_volatile":
            fix_volatile = True
        elif arg == "--fix_inline":
            fix_inline = True
        elif arg == "--silent":
            silent = True
        elif arg.startswith("-"):
            print("Unknown argument " + arg)
            print(__doc__)
            sys.exit(2)
        else:
            if not benchmark:
                benchmark = arg
            else:
                print("Error: More than one benchmark specified!")
                sys.exit(2)
    if not benchmark:
        print("Error: No benchmark specified!")
        sys.exit(2)
    return (loc, benchmark, opt, fix_volatile, fix_inline, silent)

if __name__ == "__main__":

    loc, benchmark, opt, fix_volatile, fix_inline, silent = parse_args()

    tmpdir, src = copy_source_to_tmp(benchmark)

    return_code = 1
    try:
        model = os.path.join(tmpdir, "model.ll")
        print("Running symdivine")
        result = compile_to_bitcode.compile_benchmark(
            src = src, args = [opt], output = model, fix_inline = fix_inline,
            fix_volatile = fix_volatile, lart_path = loc, supress_output = silent)

        if result == "":
            print("Compilation failed!")
            sys.exit(1)

        return_code = run_symdivine(loc, model)
    except Exception as e:
        print(e.message)
    finally:
        rmrf_tmp_dir(tmpdir)
        sys.exit(return_code);