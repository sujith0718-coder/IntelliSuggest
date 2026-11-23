@echo off
set "PATH=C:\Users\sujit\AppData\Local\Microsoft\WinGet\Packages\Kitware.CMake_Microsoft.Winget.Source_8wekyb3d8bbwe\cmake-4.4.4-windows-x86_64\bin;C:\Users\sujit\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261117-ucrt-x86_64\bin;%PATH%"
if "%~1"=="" (
    mingw32-make -C build
) else (
    mingw32-make -C build %*
)

