@echo off

mkdir ..\build
pushd ..\build
cl -FC -Zi -I"C:\Program Files (x86)\Microsoft GDK\231007\GRDK\GameKit\Include" ..\code\win32_copymade.cpp user32.lib gdi32.lib Ole32.lib
popd
