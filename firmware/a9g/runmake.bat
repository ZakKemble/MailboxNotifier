@echo off
@cd /d %~dp0

:: Hacky way of forcing fwbuild.c to recompile everytime
echo // >> src/fwbuild.c

@cd ../../
./build.bat mailbox debug

pause
