#!/usr/bin/env python

"""Usage: run_benchmark.py <symdivine_dir> <benchmark> [options]

Arguments:
    <benchmark>     input *.i file
    <symdivine_dir> location of symdivine

Options:
    --cheapsimplify       Use only cheap simplification methods
    --dontsimplify        Disable simplification
    --disabletimeout      Disable timeout for Z3
    -p --partialstore     Use partial SMT store (better caching)
    -c --enablecaching    Enable caching of Z3 formulas
    -s --statistics       Enable output of statistics
    -v --verbose          Enable verbose mode
    -w --vverbose         Enable extended verbose mode
    -o                    level of optimalizations [default: 2]
    --timeout             timeout in seconds [default: 900]
"""

import sys
import subprocess
import signal
import os
import atexit
import resource
import compile_to_bitcode
# import docopt

from time import sleep
from tempfile import mkdtemp

class Timeout(Exception):
    pass

def start_timeout(sec):
    def alarm_handler(signum, data):
        raise Timeout

    signal.signal(signal.SIGALRM, alarm_handler)
    signal.alarm(sec)

def stop_timeout():
    # turn of timeout
    signal.signal(signal.SIGALRM, signal.SIG_DFL)
    signal.alarm(0)

def run_symdivine(symdivine_location, benchmark, symdivine_params = None):
    cmd = [os.path.join(symdivine_location, "symdivine")]
    cmd.append('reachability')
    if symdivine_params:
        cmd = cmd + symdivine_params
    cmd.append(benchmark)

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return p

def compile_benchmark(src, opt_level, tmpdir):
    out = os.path.join(tmpdir, "model.ll")
    
    compile_to_bitcode.compile_benchmark(src, [opt_level], out, fix_inline=True)
    return out

def printTimeConsumed():
    us = resource.getrusage(resource.RUSAGE_CHILDREN)
    usr = getattr(us, 'ru_utime')
    syst = getattr(us, 'ru_stime')

    print('=== TIME CONSUMED')
    print(usr + syst)
    sys.stdout.flush()


def printMemoryUsage():
    us = resource.getrusage(resource.RUSAGE_CHILDREN)
    maxrss = getattr(us, 'ru_maxrss')

    print('=== MEMORY USAGE')
    print(maxrss / 1024.0)
    sys.stdout.flush()

def rmrf_tmp_dir(d):
    return os.system('rm -rf {0}'.format(d))

def copy_source_to_tmp(src):
    # create temporary directory and copy sources there
    tmpdir = mkdtemp(prefix='symdivine.', dir='.')
    basename = os.path.basename(src)

    if os.system('cp {0} {1}/{2}'.format(src, tmpdir, basename)) != 0:
        # cp already gave error message
        rmrf_tmp_dir(tmpdir)
        print('=== RESULT')
        print('ERROR')
        sys.exit(1)

    return (tmpdir, '{0}/{1}'.format(tmpdir, basename))

def signal_childs(sig):
    signal.signal(sig, signal.SIG_IGN)
    os.kill(0, sig)
    signal.signal(sig, signal.SIG_DFL)

def sigpipe_handler(signum, data):
    signal_childs(signal.SIGINT)
    signal_childs(signal.SIGINT)
    signal_childs(signal.SIGKILL)

def parse_args():
    args = sys.argv

    if len(args) < 3 or args[1].startswith("-") or args[2].startswith("-"):
        print(__doc__)
        sys.exit(1)
    opt = "-O2"
    loc = args[1]
    benchmark = args[2]
    timeout = 900
    res = []

    for arg in args[3:]:
        if arg.startswith("-O"):
            opt = arg
        elif arg.startswith("--timeout="):
            timeout = int(arg.split('=')[1])
        else:
            res.append(arg)
    print res        
    return (loc, benchmark, opt, timeout, res)

def parse_args_docopt():
    arguments = docopt.docopt(__doc__)

    arguments = {k: v for k, v in arguments.items() if v}
    if not "-O2" in arguments:
        arguments["-O2"] = 2

    if not "--timeout" in arguments:
        arguments["--timeout"] = 900

    benchmark = arguments["<benchmark>"]
    opt = "-o" + str(arguments["-o"])
    loc = arguments["<symdivine_dir>"]
    timeout = arguments["--timeout"]

    print timeout

    del arguments["-o"]
    del arguments["<benchmark>"]
    del arguments["<symdivine_dir>"]
    del arguments["--timeout"]

    res = []
    for key, value in arguments.iteritems():
        res.append(key)
        if isinstance(value, basestring):
            res.append(value)

    return (loc, benchmark, opt, timeout, res)

    

if __name__ == "__main__":

    location, benchmark, opt_level, timeout, symdivine_params = parse_args()

    tmpdir, src = copy_source_to_tmp(benchmark)

    signal.signal(signal.SIGPIPE, sigpipe_handler)
    start_timeout(timeout)

    print('=== RESULT')
    sys.stdout.flush()

    try:
        benchmark_comp = compile_benchmark(src, opt_level, tmpdir)
        p = run_symdivine(location, benchmark_comp, symdivine_params)
        (out, err) = p.communicate()
        stop_timeout()

        if p.returncode != 0:
            print('ERROR')

        if not out is None:
            if "Safe." in out:
                print("TRUE")
            elif "Error state" in out:
                print("FALSE")
            else:
                print("UNKNOWN")
        if not err is None:
            print(out)
            print(err)

        sys.stdout.flush()
    except Timeout:
        print('TIMEOUT')
        sys.stdout.flush()
    finally:
        stop_timeout()

        printTimeConsumed()
        printMemoryUsage()

        signal_childs(signal.SIGINT)

        signal_childs(signal.SIGTERM)
        rmrf_tmp_dir(tmpdir)
