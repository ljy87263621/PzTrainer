@echo off
setlocal
if "%~1"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_TEST_VS=%%i"
if not defined PZ_TEST_VS exit /b 1
call "%PZ_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0.."
if not exist obj\rage-fire-jvm mkdir obj\rage-fire-jvm
set "PZ_RAGE_FIRE_JAR=%CD%\trainer\src\runtime\resources\pztrainer-player-overrides.jar"
set "PZ_RAGE_FIRE_DLL=%CD%\obj\rage-fire-jvm\rage_fire_test.dll"
set "PZ_RAGE_FIRE_CP=%CD%\obj\rage-fire-jvm;%PZ_RAGE_FIRE_JAR%;%~1\projectzomboid.jar"
cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /LD /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I trainer\src /I trainer\third_party\imgui /I trainer\third_party\openjdk\include trainer\tests\rage_fire_jvm_test.cpp trainer\src\bridge\rage_fire_bridge.cpp /Foobj\rage-fire-jvm\ /Fe"%PZ_RAGE_FIRE_DLL%"
if errorlevel 1 goto failed
"F:\java21\bin\javac.exe" --release 17 -cp "%PZ_RAGE_FIRE_JAR%" -d obj\rage-fire-jvm trainer\tests\RageFireJvmTest.java trainer\tests\RageFireTransformsTest.java
if errorlevel 1 goto failed
"%~1\jre64\bin\java.exe" -Xverify:all "-Duser.home=%CD%\obj\rage-fire-jvm\profile" --enable-native-access=ALL-UNNAMED -cp "%PZ_RAGE_FIRE_CP%" RageFireJvmTest "%PZ_RAGE_FIRE_DLL%"
if errorlevel 1 goto failed
echo Actual game JVM firing bridge tests passed.
popd
exit /b 0
:failed
popd
exit /b 1
