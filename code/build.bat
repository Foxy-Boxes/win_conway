@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
mkdir ..\build
pushd ..\build

cl -Zi ..\code\main.cpp Gdi32.lib User32.lib

popd