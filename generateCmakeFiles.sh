#! /usr/bin/bash

source environment/kdb_env

./vendor/premake/rpi/premake5 --scripts=./vendor/premake/cmake/ cmake

if [ $# -eq 0 ]
  then
    cmake -DCMAKE_BUILD_TYPE=Release .
else
    cmake -DCMAKE_BUILD_TYPE=Debug .
fi
