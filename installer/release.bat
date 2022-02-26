@echo off
setlocal
set CUR=%~dp0
cd /d %CUR%

set CURL=C:\Windows\System32\curl.exe
set CMAKE_PATH=C:\Program Files\CMake\bin
set VS_BASE=C:\Program Files (x86)\Microsoft Visual Studio\2019
set CYGWIN_PATH=C:\cygwin64\bin
set INNO_SETUP=C:\Program Files (x86)\Inno Setup 6\iscc.exe

echo 1. download libs, rebuild libs and Tera Term, installer, archive
echo 2. build libs
echo 3. build libs and rebuild Tera Term, installer, archive (for Release build)
echo 4. build libs and Tera Term (for Normal build, snapshot)
echo 7. exec cmd.exe
echo 8. check tools
echo 9. exit

if "%1" == "" (
    set /p no="select no "
) else (
    set no=%1
)
echo %no%

if "%no%" == "1" (
    call :update_libs
    call :build_teraterm freeze_state
)

if "%no%" == "2" (
    call :build_libs
)

if "%no%" == "3" (
    call :build_teraterm freeze_state
)

if "%no%" == "4" (
    call :build_teraterm
)

if "%no%" == "7" (
    call :exec_cmd
)

if "%no%" == "8" (
    call :check_tools
)

pause
exit 0


rem ####################
:build_teraterm

setlocal
set PATH=
call :set_path
cd /d %CUR%

if "%1" == "freeze_state" (
    call build.bat rebuild
    call makearchive.bat release
) else (
    call makearchive.bat
)
call ..\buildtools\svnrev\sourcetree_info.bat
if "%1" == "freeze_state" (
    pushd Output
    %CMAKE% -E tar cf TERATERM_r%SVNVERSION%_%DATE%_%TIME%.zip --format=zip teraterm-5.0/
    popd
) else (
    %CMAKE% -E tar cf TERATERM_r%SVNVERSION%_%DATE%_%TIME%.zip --format=zip snapshot-%DATE%_%TIME%
)
"%INNO_SETUP%" teraterm.iss

endlocal
exit /b 0

rem ####################
:update_libs

setlocal
set PATH=
set PATH=%PATH%;%CMAKE_PATH%
set PATH=%PATH%;%SystemRoot%
set PATH=%PATH%;%SystemRoot%\system32
cd /d %CUR%..\libs
set CMAKE="%CMAKE_PATH%\cmake.exe"

:oniguruma
%CURL% -L https://github.com/kkos/oniguruma/releases/download/v6.9.7.1/onig-6.9.7.1.tar.gz -o oniguruma.tar.gz
%CMAKE% -E tar xf oniguruma.tar.gz
%CMAKE% -E rm -rf oniguruma
%CMAKE% -E rename onig-6.9.7 oniguruma

:zlib
%CURL% -L https://zlib.net/zlib-1.2.11.tar.xz -o zlib.tar.xz
%CMAKE% -E tar xf zlib.tar.xz
%CMAKE% -E rm -rf zlib
%CMAKE% -E rename zlib-1.2.11 zlib

:putty
%CURL% -L https://the.earth.li/~sgtatham/putty/0.76/putty-0.76.tar.gz -o putty.tar.gz
%CMAKE% -E tar xf putty.tar.gz
%CMAKE% -E rm -rf putty
%CMAKE% -E rename putty-0.76 putty

:SFMT
%CURL% -L http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/SFMT-src-1.5.1.zip -o sfmt.zip
%CMAKE% -E tar xf sfmt.zip
%CMAKE% -E rm -rf SFMT
%CMAKE% -E rename SFMT-src-1.5.1 SFMT
echo #define SFMT_VERSION "1.5.1" > SFMT\SFMT_version_for_teraterm.h

:cJSON
%CURL% -L https://github.com/DaveGamble/cJSON/archive/v1.7.14.zip -o cJSON.zip
%CMAKE% -E tar xf cJSON.zip
%CMAKE% -E rm -rf cJSON
%CMAKE% -E rename cJSON-1.7.14 cJSON

:argon2
%CURL% -L https://github.com/P-H-C/phc-winner-argon2/archive/refs/tags/20190702.tar.gz -o argon2.tar.gz
%CMAKE% -E tar xf argon2.tar.gz
%CMAKE% -E rm -rf argon2
%CMAKE% -E rename phc-winner-argon2-20190702 argon2

:libressl
%CURL% -L https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.4.2.tar.gz -o libressl.tar.gz
%CMAKE% -E tar xf libressl.tar.gz
%CMAKE% -E rm -rf libressl
%CMAKE% -E rename libressl-3.4.2 libressl

endlocal
exit /b 0

rem ####################
:build_libs

setlocal
cd /d %CUR%..\libs
set PATH=
set PATH=%PATH%;C:\Program Files (x86)\Subversion\bin
set PATH=%PATH%;C:\Program Files\TortoiseSVN\bin
set PATH=%PATH%;%CMAKE_PATH%
set PATH=%PATH%;C:\Strawberry\perl\bin
set PATH=%PATH%;%SystemRoot%
set PATH=%PATH%;%SystemRoot%\system32
call :set_vs_env
call buildall.bat
endlocal
exit /b 0

rem ####################
:set_path
set PATH=%PATH%;C:\Program Files (x86)\Subversion\bin
set PATH=%PATH%;C:\Program Files\TortoiseSVN\bin
set PATH=%PATH%;%CMAKE_PATH%
set PATH=%PATH%;C:\Strawberry\perl\bin
set PATH=%PATH%;%SystemRoot%
set PATH=%PATH%;%SystemRoot%\system32
set PATH=%PATH%;%CYGWIN_PATH%
set CMAKE="%CMAKE_PATH%\cmake.exe"
call :set_vs_env
exit /b 0

rem ####################
:set_vs_env

if exist "%VS_BASE%\Community" (
  call "%VS_BASE%\Community\VC\Auxiliary\Build\vcvars32.bat"
)
if exist "%VS_BASE%\Professional" (
  call "%VS_BASE%\Profssional\VC\Auxiliary\Build\vcvars32.bat"
)
if exist "%VS_BASE%\Enterprise" (
  call "%VS_BASE%\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
)
exit /b 0

rem ####################
:exec_cmd
set PATH=
call :set_path
cmd
exit /b 0

rem ####################
:check_tools
set PATH=
call :set_path

echo cmd(windows)
ver

echo Visual Studio
echo VS_BASE=%VS_BASE%
cl

echo curl
where curl
echo CURL=%CURL%
%CURL% --version
curl

echo svn
where svn
svn --version

echo perl
where perl
perl --version

echo cmake
where cmake
echo CMAKE=%CMAKE%
%CMAKE% --version

echo cygwin
echo CYGWIN_PATH=%CYGWIN_PATH%
cygcheck -c base-cygwin
cygcheck -c gcc-core
cygcheck -c w32api-headers
cygcheck -c make

echo inno setup
"%INNO_SETUP%" /?

exit /b 0
