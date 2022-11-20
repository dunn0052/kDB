#! /usr/bin/bash

./vendor/premake/rpi/premake5 cmake

if [ $# -eq 0 ]
  then
    cmake -DCMAKE_BUILD_TYPE=Release .
else
    cmake -DCMAKE_BUILD_TYPE=Debug .
fi