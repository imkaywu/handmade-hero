@echo off

IF NOT EXIST build mkdir build
pushd build

clang++ -std=c++20 -g -O0 ../code/win32_handmade.cpp -o main -luser32 -lgdi32

popd
