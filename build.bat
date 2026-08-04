@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

IF NOT EXIST build mkdir build
pushd build

cl -MT -nologo -Gm- -GR- -EHa -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7 -Fmwin32_handmade.map ../code/win32_handmade.cpp /link -OPT:REF -SUBSYSTEM:WINDOWS user32.lib gdi32.lib
popd
