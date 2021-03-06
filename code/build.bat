@echo off

mkdir ..\build
pushd ..\build

cl -Zi ..\code\main.cpp Gdi32.lib User32.lib

popd