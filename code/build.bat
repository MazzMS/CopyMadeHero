@echo off

mkdir ..\..\build
pushd ..\..\build
cl -FC -Zi ..\handmade\code\handmadehero.cpp user32.lib gdi32.lib
popd
