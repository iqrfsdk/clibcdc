#!/usr/bin/python3
# -*- coding: utf-8 -*-

"""
Copyright 2018 IQRF Tech s.r.o.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import argparse
import sys
import subprocess
import os

ARGS = argparse.ArgumentParser(description="Cross-platform clibcdc builder.")
ARGS.add_argument("-g", "--gen", action="store", dest="gen",
                  required=True, type=str, help="Platform generator.")
ARGS.add_argument("-d", "--debug", action="store", dest="debug",
                  default="no", type=str, help="Debug level.")


VS_GEN = "Visual Studio 14 2015"
WIN64 = "Win64"
UNIX_GEN = "Unix Makefiles"
ECLIPSE_GEN = "Eclipse CDT4 - Unix Makefiles"


def main():
    """
    Main program function
    """
    args = ARGS.parse_args()
    gen = args.gen.lower()
    debug = args.debug.lower()

    if debug == "yes":
        DEBUG_STR = "-DCMAKE_BUILD_TYPE=Debug"
    else:
        DEBUG_STR = ""

    if gen == "vs14":
        BUILD_DIR = os.path.join("build", "Visual_Studio_14_2015", "x64")
        build(VS_GEN + " " + WIN64, BUILD_DIR, DEBUG_STR)
    elif gen == "make":
        BUILD_DIR = os.path.join("build", "Unix_Makefiles")
        build(UNIX_GEN, BUILD_DIR, DEBUG_STR)
    elif gen == "eclipse":
        BUILD_DIR = os.path.join("build", "Eclipse_CDT4-Unix_Makefiles")
        build(ECLIPSE_GEN, BUILD_DIR, DEBUG_STR)


def send_command(cmd):
    """
    Execute shell command and return output
    @param cmd Command to exec
    @return string Output
    """
    print(cmd)
    return subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.read()


def build(generator, build_dir, debug):
    """
    Building clibcdc
    @param arch Platform architecture
    """
    current_dir = os.getcwd()

    send_command("rm -r " + build_dir)

    if sys.platform.startswith("win"):
        send_command("mkdir " + build_dir)
    else:
        send_command("mkdir -p " + build_dir)

    os.chdir(build_dir)

    if sys.platform.startswith("win"):
        out = send_command("cmake -G " + "\"" + generator + "\" " + current_dir)
        print(out)
    else:
        out = send_command("cmake -G " + "\"" + generator + "\" " + current_dir + " " + debug)
        print(out)

    os.chdir(current_dir)
    out = send_command("cmake --build " + build_dir)
    print(out)

if __name__ == "__main__":
    main()
