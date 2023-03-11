#! /usr/bin/bash

source config/.kdb_env

if [[ "$*" == *"-d"* ]]
  then
    cmake -DCMAKE_BUILD_TYPE=Debug .
else
    cmake -DCMAKE_BUILD_TYPE=Release .
fi
