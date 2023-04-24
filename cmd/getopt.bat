@echo off
set SNAP=
set OPTIONS=
set EXT=--with-parallel
set PHP_TS=_TS
set DEPS=pthread
set EXTNAME=parallel
:parse
if "%~1" == "" goto endparse
if "%~1" == "--php" set PHP_VER=%2
if "%~1" == "--option" set OPTIONS=%2 %OPTIONS%
if "%~1" == "--snap" set SNAP=snap
if "%~1" == "--shared" ( set EXT=--with-parallel=shared && set SHARE==shared )
if "%~1" == "--arch" set ARCH=x64

shift
goto parse
:endparse
if not defined PHP_VER (
    set PHP_VER=8.2.5
)
