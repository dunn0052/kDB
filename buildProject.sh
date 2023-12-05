if [[ "$*" == *"-d"* ]]
  then
    ./generateCmakeFiles.sh -d
    cmake --build . -j 12
else
    ./generateCmakeFiles.sh
    cmake --build . -j 12
fi
