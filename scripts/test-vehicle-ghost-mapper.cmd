@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_TEST_VS=%%i"
if not defined PZ_TEST_VS exit /b 1
call "%PZ_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0.."
if not exist obj\extension-tests\mapper mkdir obj\extension-tests\mapper
cl /nologo /std:c++20 /EHsc /utf-8 /MD /O1 /LD /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I trainer\src /I trainer\third_party\minhook\include trainer\tests\vehicle_ghost_runtime_test.cpp /Foobj\extension-tests\mapper\ /Feobj\extension-tests\mapper\vehicle_ghost_probe.dll
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I launcher /I protection trainer\tests\vehicle_ghost_mapper_test.cpp launcher\manual_mapper.cpp /Foobj\extension-tests\mapper\ /Feobj\extension-tests\mapper\vehicle_ghost_mapper_test.exe
if errorlevel 1 goto failed
obj\extension-tests\mapper\vehicle_ghost_mapper_test.exe obj\extension-tests\mapper\vehicle_ghost_probe.dll
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
