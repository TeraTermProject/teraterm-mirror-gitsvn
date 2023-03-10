
if not "%VSINSTALLDIR%" == "" goto vsinstdir

rem InnoSetup からビルドする時は、標準で環境変数に設定されている
rem Visual Studioが選択される。VS2019決め打ちでビルドしたい場合は
rem 下記 goto 文を有効にすること。
rem goto check_2019

if "%VS80COMNTOOLS%" == "" goto check_2008
if not exist "%VS80COMNTOOLS%\vsvars32.bat" goto check_2008
call "%VS80COMNTOOLS%\vsvars32.bat"
goto vs2005

:check_2008
if "%VS90COMNTOOLS%" == "" goto check_2010
if not exist "%VS90COMNTOOLS%\vsvars32.bat" goto check_2010
call "%VS90COMNTOOLS%\vsvars32.bat"
goto vs2008

:check_2010
if "%VS100COMNTOOLS%" == "" goto check_2012
if not exist "%VS100COMNTOOLS%\vsvars32.bat" goto check_2012
call "%VS100COMNTOOLS%\vsvars32.bat"
goto vs2010

:check_2012
if "%VS110COMNTOOLS%" == "" goto check_2013
if not exist "%VS110COMNTOOLS%\VsDevCmd.bat" goto check_2013
call "%VS110COMNTOOLS%\VsDevCmd.bat"
goto vs2012

:check_2013
if "%VS120COMNTOOLS%" == "" goto check_2015
if not exist "%VS120COMNTOOLS%\VsDevCmd.bat" goto check_2015
call "%VS120COMNTOOLS%\VsDevCmd.bat"
goto vs2013

:check_2015
if "%VS140COMNTOOLS%" == "" goto check_2017
if not exist "%VS140COMNTOOLS%\VsDevCmd.bat" goto check_2017
call "%VS140COMNTOOLS%\VsDevCmd.bat"
goto vs2015

:check_2017
if "%VS150COMNTOOLS%" == "" goto check_2019
if not exist "%VS150COMNTOOLS%\VsDevCmd.bat" goto check_2019
call "%VS150COMNTOOLS%\VsDevCmd.bat"
goto vs2017

:check_2019
if "%VS160COMNTOOLS%" == "" goto check_2022
if not exist "%VS160COMNTOOLS%\VsDevCmd.bat" goto novs
call "%VS160COMNTOOLS%\VsDevCmd.bat"
goto vs2019

:check_2022
if "%VS170COMNTOOLS%" == "" goto novs
if not exist "%VS170COMNTOOLS%\VsDevCmd.bat" goto novs
call "%VS170COMNTOOLS%\VsDevCmd.bat"
goto vs2022

:novs
@echo off
echo "Can't find Visual Studio"
echo.
echo InnoSetupからVS2019でビルドするためには、環境変数を設定してください。
echo.
echo 例
echo VS160COMNTOOLS=c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\
@echo on
pause
goto fail

:vsinstdir
rem Visual Studioのバージョン判別
set VSCMNDIR="%VSINSTALLDIR%\Common7\Tools\"
set VSCMNDIR=%VSCMNDIR:\\=\%

if /I %VSCMNDIR% EQU "%VS80COMNTOOLS%" goto vs2005
if /I %VSCMNDIR% EQU "%VS90COMNTOOLS%" goto vs2008
if /I %VSCMNDIR% EQU "%VS100COMNTOOLS%" goto vs2010
if /I %VSCMNDIR% EQU "%VS110COMNTOOLS%" goto vs2012
if /I %VSCMNDIR% EQU "%VS120COMNTOOLS%" goto vs2013
if /I %VSCMNDIR% EQU "%VS140COMNTOOLS%" goto vs2015
if /I %VSCMNDIR% EQU "%VS150COMNTOOLS%" goto vs2017
if /I %VSCMNDIR% EQU "%VS160COMNTOOLS%" goto vs2019
if /I %VSCMNDIR% EQU "%VS170COMNTOOLS%" goto vs2022

echo Unknown Visual Studio version
goto fail

:vs2005
set TERATERMSLN=..\teraterm\ttermpro.v8.sln
set TTSSHSLN=..\ttssh2\ttssh.v8.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v8.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v8.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v8.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v8.sln
set CYGWINSLN=..\CYGWIN\cygwin.v8.sln

rem VS2005にSP1が適用されているかをチェックする
cl /? 2>&1 | findstr /C:"14.00.50727.762"
echo %errorlevel%

if %errorlevel% == 0 (
    goto vsend
)

echo "VS2005にSP1が適用されていないようです"
set /P ANS2005="続行しますか？(y/n)"
if "%ANS2005%"=="y" (
    goto vsend
) else (
    echo "build.bat を終了します"
    goto fail
)


:vs2008
set TERATERMSLN=..\teraterm\ttermpro.v9.sln
set TTSSHSLN=..\ttssh2\ttssh.v9.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v9.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v9.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v9.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v9.sln
set CYGWINSLN=..\CYGWIN\cygwin.v9.sln
goto vsend

:vs2010
set TERATERMSLN=..\teraterm\ttermpro.v10.sln
set TTSSHSLN=..\ttssh2\ttssh.v10.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v10.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v10.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v10.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v10.sln
set CYGWINSLN=..\CYGWIN\cygwin.v10.sln
goto vsend

:vs2012
set TERATERMSLN=..\teraterm\ttermpro.v11.sln
set TTSSHSLN=..\ttssh2\ttssh.v11.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v11.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v11.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v11.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v11.sln
set CYGWINSLN=..\CYGWIN\cygwin.v11.sln
goto vsend

:vs2013
set TERATERMSLN=..\teraterm\ttermpro.v12.sln
set TTSSHSLN=..\ttssh2\ttssh.v12.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v12.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v12.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v12.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v12.sln
set CYGWINSLN=..\CYGWIN\cygwin.v12.sln
goto vsend

:vs2015
set TERATERMSLN=..\teraterm\ttermpro.v14.sln
set TTSSHSLN=..\ttssh2\ttssh.v14.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v14.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v14.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v14.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v14.sln
set CYGWINSLN=..\CYGWIN\cygwin.v14.sln
goto vsend

:vs2017
set TERATERMSLN=..\teraterm\ttermpro.v15.sln
set TTSSHSLN=..\ttssh2\ttssh.v15.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v15.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v15.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v15.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v15.sln
set CYGWINSLN=..\CYGWIN\cygwin.v15.sln
goto vsend

:vs2019
set TERATERMSLN=..\teraterm\ttermpro.v16.sln
set TTSSHSLN=..\ttssh2\ttssh.v16.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v16.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v16.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v16.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v16.sln
set CYGWINSLN=..\CYGWIN\cygwin.v16.sln
goto vsend

:vs2022
set TERATERMSLN=..\teraterm\ttermpro.v17.sln
set TTSSHSLN=..\ttssh2\ttssh.v17.sln
set TTPROXYSLN=..\TTProxy\TTProxy.v17.sln
set TTXKANJISLN=..\TTXKanjiMenu\ttxkanjimenu.v17.sln
set TTPMENUSLN=..\ttpmenu\ttpmenu.v17.sln
set TTXSAMPLESLN=..\TTXSamples\TTXSamples.v17.sln
set CYGWINSLN=..\CYGWIN\cygwin.v17.sln
goto vsend

:vsend

set BUILD=build
if "%1" == "rebuild" (set BUILD=rebuild)
pushd %~dp0

rem ライブラリをコンパイル
pushd ..\libs
CALL buildall.bat
if ERRORLEVEL 1 (
    echo "build.bat を終了します"
    goto fail
)
popd


rem リビジョンが変化していれば svnversion.h を更新する。
call ..\buildtools\svnrev\svnrev.bat


devenv /%BUILD% release %TERATERMSLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %TTSSHSLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %TTPROXYSLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %TTXKANJISLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %TTPMENUSLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %TTXSAMPLESLN%
if ERRORLEVEL 1 goto fail
devenv /%BUILD% release %CYGWINSLN%
if ERRORLEVEL 1 goto fail

rem cygterm をコンパイル
pushd ..\cygwin\cygterm
if "%BUILD%" == "rebuild" (
    make clean
    make cygterm+-x86_64-clean
)
make cygterm+-x86_64 -j
make archive
popd

rem msys2term
if not exist c:\msys64\usr\bin\msys-2.0.dll goto msys2term_pass
setlocal
PATH=C:\msys64\usr\bin
pushd ..\cygwin\cygterm
if "%BUILD%" == "rebuild" (
    make clean
    make msys2term-clean
)
make msys2term -j
endlocal
popd

:msys2term_pass

rem lng ファイルを作成
call makelang.bat

popd
exit /b 0

:fail
popd
exit /b 1
