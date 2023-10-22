if [[ "$*" == *"-d"* ]]
  then
    ./generateCmakeFiles.sh -d
    cmake --build . -j 12 --verbose
else
    ./generateCmakeFiles.sh
    cmake --build . -j 12
fi
