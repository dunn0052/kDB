#! /usr/bin/bash

./vendor/premake/rpi/premake5 cmake
cmake -DCMAKE_BUILD_TYPE=Debug .