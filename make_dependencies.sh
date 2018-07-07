#!/bin/bash
TOP=$(pwd)
cd $TOP/
bash make_dirs.sh
bash make_glslang.sh
bash make_spirv_cross.sh