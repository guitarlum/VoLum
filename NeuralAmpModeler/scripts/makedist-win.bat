@echo off

REM - batch file to build Visual Studio project and zip the resulting binaries (or make installer)
REM - updating version numbers requires python and python path added to %PATH% env variable 
REM - zipping requires 7zip in %ProgramFiles%\7-Zip\7z.exe
REM - building installer requires innosetup 6 in "%ProgramFiles(x86)%\Inno Setup 6\iscc"
REM - AAX codesigning requires wraptool tool added to %PATH% env variable and aax.key/.crt in .\..\..\iPlug2\Certificates\

REM - two arguments are demo/full and zip/installer

set DEMO_ARG="%1"
set ZIP_ARG="%2"

if [%DEMO_ARG%]==[] goto USAGE
if [%ZIP_ARG%]==[] goto USAGE

echo SCRIPT VARIABLES -----------------------------------------------------
echo DEMO_ARG %DEMO_ARG% 
echo ZIP_ARG %ZIP_ARG% 
echo END SCRIPT VARIABLES -----------------------------------------------------

if %DEMO_ARG% == "demo" (
  echo Making NeuralAmpModeler Windows DEMO VERSION distribution ...
  set DEMO=1
) else (
  echo Making NeuralAmpModeler Windows FULL VERSION distribution ...
  set DEMO=0
)

if %ZIP_ARG% == "zip" (
  set ZIP=1
) else (
  set ZIP=0
)

echo ------------------------------------------------------------------
echo Updating version numbers ...

call python prepare_resources-win.py %DEMO%
call python update_installer-win.py %DEMO%

cd ..\

echo "touching source"

copy /b *.cpp+,,

echo ------------------------------------------------------------------
echo Building ...

REM Remove previous build logs
if exist "build-win.log" (del build-win.log)

REM GitHub Actions: use microsoft/setup-msbuild — PATH already has MSBuild. Skip vcvarsall/vswhere
REM (nested for /f + paths under "Program Files" can break and yield 'C:\Program' is not recognized).
if /i "%GITHUB_ACTIONS%"=="true" (
  echo Using MSBuild from PATH ^(GITHUB_ACTIONS^)
  for /f "delims=" %%G in ('where msbuild 2^>nul') do (
    set "MSBUILD_EXE=%%G"
    goto :msbuild_ready
  )
  echo ERROR: msbuild not on PATH. Add microsoft/setup-msbuild@v2 before makedist-win.bat.
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

REM Visual Studio 2019 path is obsolete on many machines; use vswhere (VS 2022 Build Tools, Community, etc.)
if not defined DevEnvDir (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if not "%%i"=="" call "%%i\VC\Auxiliary\Build\vcvarsall.bat" x86_x64
  )
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`) do set "MSBUILD_EXE=%%i"
if not defined MSBUILD_EXE (
  echo ERROR: MSBuild not found. Install "Visual Studio Build Tools" with workload "Desktop development with C++" ^(Microsoft.VisualStudio.Workload.VCTools^).
  exit /b 1
)

:msbuild_ready


REM - set preprocessor macros like this, for instance to set demo preprocessor macro:
if %DEMO% == 1 (
  set CMDLINE_DEFINES="DEMO_VERSION=1"
  REM -copy ".\resources\img\AboutBox_Demo.png" ".\resources\img\AboutBox.png"
) else (
  set CMDLINE_DEFINES="DEMO_VERSION=0"
  REM -copy ".\resources\img\AboutBox_Registered.png" ".\resources\img\AboutBox.png"
)

REM - Could build individual targets like this:
REM - msbuild NeuralAmpModeler-app.vcxproj /p:configuration=release /p:platform=win32

REM echo Building 32 bit binaries...
REM msbuild NeuralAmpModeler.sln /p:configuration=release /p:platform=win32 /nologo /verbosity:minimal /fileLogger /m /flp:logfile=build-win.log;errorsonly 

REM echo Building 64 bit binaries...
REM add projects with /t to build VST2 and AAX
REM NOTE: In cmd.exe, semicolons split commands unless quoted — targets and file logger must be quoted.
"%MSBUILD_EXE%" NeuralAmpModeler.sln /t:"NeuralAmpModeler-app;NeuralAmpModeler-vst3" /p:configuration=release /p:platform=x64 /nologo /verbosity:minimal /fileLogger /m /flp:"logfile=build-win.log;errorsonly;append"
if errorlevel 1 (
  echo ERROR: MSBuild failed
  exit /b 1
)

REM --echo Copying AAX Presets

REM --echo ------------------------------------------------------------------
REM --echo Code sign AAX binary...
REM --info at pace central, login via iLok license manager https://www.paceap.com/pace-central.html
REM --wraptool sign --verbose --account XXXXX --wcguid XXXXX --keyfile XXXXX.p12 --keypassword XXXXX --in .\build-win\aax\bin\NeuralAmpModeler.aaxplugin\Contents\Win32\NeuralAmpModeler.aaxplugin --out .\build-win\aax\bin\NeuralAmpModeler.aaxplugin\Contents\Win32\NeuralAmpModeler.aaxplugin
REM --wraptool sign --verbose --account XXXXX --wcguid XXXXX --keyfile XXXXX.p12 --keypassword XXXXX --in .\build-win\aax\bin\NeuralAmpModeler.aaxplugin\Contents\x64\NeuralAmpModeler.aaxplugin --out .\build-win\aax\bin\NeuralAmpModeler.aaxplugin\Contents\x64\NeuralAmpModeler.aaxplugin

if %ZIP% == 0 (
REM - Make Installer (InnoSetup)

echo ------------------------------------------------------------------
echo Making Installer ...

  REM if exist "%ProgramFiles(x86)%" (goto 64-Bit-is) else (goto 32-Bit-is)

  REM :32-Bit-is
  REM REM "%ProgramFiles%\Inno Setup 6\iscc" /Q ".\installer\VoLum.iss"
  REM goto END-is

  REM :64-Bit-is
  "%ProgramFiles(x86)%\Inno Setup 6\iscc" /Q ".\installer\VoLum.iss"
  REM goto END-is

  REM :END-is

  REM - Codesign Installer for Windows 8+
  REM -"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin\signtool.exe" sign /f "XXXXX.p12" /p XXXXX /d "NeuralAmpModeler Installer" ".\installer\NeuralAmpModeler Installer.exe"

  REM -if %1 == 1 (
  REM -copy ".\installer\NeuralAmpModeler Installer.exe" ".\installer\NeuralAmpModeler Demo Installer.exe"
  REM -del ".\installer\NeuralAmpModeler Installer.exe"
  REM -)

  echo Making Zip File of Installer ...
) else (
  echo Making Zip File ...
)

FOR /F "tokens=* USEBACKQ" %%F IN (`call python scripts\makezip-win.py %DEMO% %ZIP%`) DO (
SET ZIP_NAME=%%F
)

echo ------------------------------------------------------------------
echo Printing log file to console...

type build-win.log
goto SUCCESS

:USAGE
echo Usage: %0 [demo/full] [zip/installer]
exit /B 1

:SUCCESS
echo %ZIP_NAME%

exit /B 0