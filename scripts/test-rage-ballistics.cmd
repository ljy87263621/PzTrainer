@echo off
setlocal
if "%~1"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PZ_TEST_VS=%%i"
if not defined PZ_TEST_VS exit /b 1
call "%PZ_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
pushd "%~dp0.."
if not exist obj\rage-tests mkdir obj\rage-tests
set "PZ_RAGE_JDK=%~2"
if not defined PZ_RAGE_JDK set "PZ_RAGE_JDK=F:\java21"
"%PZ_RAGE_JDK%\bin\javac.exe" --release 17 -d obj\rage-tests trainer\tests\fixtures\aim_delay\zombie\characters\IsoPlayer.java trainer\tests\fixtures\aim_delay\zombie\MainThread.java trainer\tests\fixtures\aim_delay\zombie\MainThreadQueueItem.java trainer\tests\fixtures\aim_delay\zombie\network\GameClient.java
if errorlevel 1 goto failed
cl /nologo /std:c++17 /EHsc /utf-8 /MD /O1 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I protection /I trainer\src /I trainer\third_party\openjdk\include /I trainer\third_party\minhook\include /I trainer\third_party\minhook\src trainer\tests\rage_ballistics_test.cpp trainer\src\bridge\player_aiming_delay_bridge.cpp trainer\src\bridge\main_thread_invoker.cpp trainer\third_party\minhook\src\buffer.c trainer\third_party\minhook\src\hook.c trainer\third_party\minhook\src\trampoline.c trainer\third_party\minhook\src\hde\hde64.c /Foobj\rage-tests\ /Feobj\rage-tests\rage_ballistics_test.exe
if errorlevel 1 goto failed
obj\rage-tests\rage_ballistics_test.exe "%~1" "%CD%\obj\rage-tests"
if errorlevel 1 goto failed
echo Native Rage hooks and main-thread local-player aiming-delay tests passed.
popd
exit /b 0
:failed
popd
exit /b 1
