@echo off

mkdir build
pushd build

g++ -std=c++20 -g -O0 ../code/win32_handmade.cpp -o main -luser32 -lgdi32

popd
