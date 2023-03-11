#! /usr/bin/bash

source config/.kdb_env

./vendor/premake/Linux/premake5 --scripts=./vendor/premake/cmake/ cmake

if [ $# -eq 0 ]
  then
    cmake -DCMAKE_BUILD_TYPE=Release .
else
    cmake -DCMAKE_BUILD_TYPE=Debug .
fi
