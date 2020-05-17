
:: Set COMPILER to the location of AVR-GCC bin folder
:: File path must not contain spaces!
@set COMPILER=

@set MAKE=%COMPILER%make.exe

:: https://github.com/mraardvark/pyupdi
@set PYUPDI=pyupdi.py
@set UPDIPORT=COM12

@cd /d %~dp0
::@%MAKE% clean
@echo CPUs: %NUMBER_OF_PROCESSORS%
@%MAKE% -j %NUMBER_OF_PROCESSORS% COMPILER=%COMPILER% PYUPDI=%PYUPDI% UPDIPORT=%UPDIPORT%
@if %errorlevel%==0 %MAKE% upload COMPILER=%COMPILER% PYUPDI=%PYUPDI% UPDIPORT=%UPDIPORT%
pause
