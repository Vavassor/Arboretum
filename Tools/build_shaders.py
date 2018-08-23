#!/usr/bin/python

# The "xsc" in this file is https://github.com/LukasBanana/XShaderCompiler .
# It will need to be installed for this script to work correctly.

import argparse
import glob
import os.path
import subprocess

description = "This translates many HLSL shaders to GLSL."
parser = argparse.ArgumentParser(description=description)
parser.add_argument("-i", "--input ", default=".",
        help="path to input files", dest="input")
parser.add_argument("-o", "--output", default=".",
        help="path to put translated files", dest="output")
arguments = parser.parse_args()

input_path = arguments.input
output_path = arguments.output

input_pattern = os.path.join(input_path, "*.vs.hlsl")
for path in glob.glob(input_pattern):
    root, extension = os.path.splitext(path)
    filename = os.path.basename(root) + ".glsl"
    translated_path = os.path.join(output_path, filename)
    command = ["xsc", "-EB", "-E", "vertex_main", "-o", translated_path,
            "-T", "vert", path]
    subprocess.run(command)

input_pattern = os.path.join(input_path, "*.fs.hlsl")
for path in glob.glob(input_pattern):
    root, extension = os.path.splitext(path)
    filename = os.path.basename(root) + ".glsl"
    translated_path = os.path.join(output_path, filename)
    command = ["xsc", "-EB", "-E", "pixel_main", "-o", translated_path,
            "-T", "frag", path]
    subprocess.run(command)

