@echo off
setlocal
if "%~2"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_TEST_VS=%%i"
if not defined PZ_TEST_VS exit /b 1
call "%PZ_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0.."
if not exist obj\viewpoint-tests mkdir obj\viewpoint-tests
cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I trainer\src /I trainer\third_party\openjdk\include trainer\tests\viewpoint_bridge_test.cpp trainer\src\bridge\viewpoint_bridge.cpp /Foobj\viewpoint-tests\ /Feobj\viewpoint-tests\viewpoint_bridge_test.exe
if errorlevel 1 goto failed
obj\viewpoint-tests\viewpoint_bridge_test.exe "%~1"
if errorlevel 1 goto failed
obj\viewpoint-tests\viewpoint_bridge_test.exe "%~1" "%~2"
if errorlevel 1 goto failed
echo Viewpoint projection and actual-mod JNI tests passed.
popd
exit /b 0
:failed
popd
exit /b 1
