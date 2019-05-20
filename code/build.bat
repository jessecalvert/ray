@REM @Author: Jesse
@REM @Date:   2016-08-26 15:46:32
@REM @Last Modified by:   Jesse
@REM Modified time: 2017-07-16 22:50:29

@echo off

set CommonCompilerFlags=-O2 -MTd -nologo -Gm- -GR- -EHa- -Oi -WX -W4 -wd4127 -wd4201 -wd4100 -wd4189 -wd4505 -FC -Z7 -F0x1000000
set CommonCompilerFlags=-D_CRT_SECURE_NO_WARNINGS -DRAY_DEBUG=1 %CommonCompilerFlags%
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

IF NOT EXIST W:\ray\build mkdir W:\ray\build
pushd W:\ray\build
W:\ctime\ctime -begin ray.ctm

cl %CommonCompilerFlags% W:\ray\code\ray.cpp -Fmray.map /link %CommonLinkerFlags%
set seterror=%ERRORLEVEL%

W:\ctime\ctime -end ray.ctm

popd

IF %seterror%==0 GOTO compiled
GOTO end

:compiled
pushd W:\ray\data
w:\ray\build\ray.exe
start test.bmp
popd

:end
