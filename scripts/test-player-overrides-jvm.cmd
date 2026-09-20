@echo off
setlocal
if "%~2"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_TEST_VS=%%i"
if not defined PZ_TEST_VS exit /b 1
call "%PZ_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0.."
if not exist obj\extension-tests\jvm mkdir obj\extension-tests\jvm
set "PZ_PLAYER_TEST_JAR=%CD%\trainer\src\runtime\resources\pztrainer-player-overrides.jar"
set "PZ_BRIDGE_TEST_JAR=%CD%\trainer\src\runtime\resources\pztrainer-lua-bridge.jar"
set "PZ_PLAYER_TEST_DLL=%CD%\obj\extension-tests\jvm\player_overrides_test.dll"
set "PZ_PLAYER_TEST_CLASSES=%CD%\obj\extension-tests\jvm"
cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /LD /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I trainer\src /I trainer\third_party\imgui /I trainer\third_party\openjdk\include trainer\tests\player_overrides_jvm_test.cpp trainer\src\bridge\player_overrides.cpp /Foobj\extension-tests\jvm\ /Fe"%PZ_PLAYER_TEST_DLL%"
if errorlevel 1 goto failed
"%~1\bin\javac.exe" -encoding UTF-8 -cp "%~2\projectzomboid.jar" -d "%PZ_PLAYER_TEST_CLASSES%" trainer\tests\PlayerOverridesJvmTest.java
if errorlevel 1 goto failed
pushd "%~2"
"%~1\bin\java.exe" -Xverify:all --enable-native-access=ALL-UNNAMED -Djava.library.path=. -cp "%PZ_PLAYER_TEST_CLASSES%;%~2\projectzomboid.jar" PlayerOverridesJvmTest "%PZ_PLAYER_TEST_DLL%" "%PZ_BRIDGE_TEST_JAR%"
set "PZ_TEST_RESULT=%ERRORLEVEL%"
if "%PZ_TEST_RESULT%"=="0" (
    "%~1\bin\java.exe" -Xverify:all --enable-native-access=ALL-UNNAMED -Djava.library.path=. -cp "%PZ_PLAYER_TEST_CLASSES%;%~2\projectzomboid.jar" PlayerOverridesJvmTest "%PZ_PLAYER_TEST_DLL%" "%PZ_BRIDGE_TEST_JAR%" player-first
    if errorlevel 1 set "PZ_TEST_RESULT=1"
)
popd
if not "%PZ_TEST_RESULT%"=="0" goto failed
popd
exit /b 0
:failed
popd
exit /b 1
