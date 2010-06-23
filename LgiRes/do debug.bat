mkdir lgires-win32

del /Q lgires-win32.zip
del /Q lgires-win32\*.*
cd lgires-win32

copy ..\debug\*.exe . > nul
copy ..\debug\*.pdb . > nul
copy ..\LrStrip\debug\*.exe . > nul
copy ..\LrStrip\Code\LrStrip.lr8 . > nul
copy ..\Code\lgires.lr8 . > nul
copy ..\..\debug\LgiD.dll . > nul
copy ..\..\debug\*.pdb . > nul
copy ..\..\Gel\debug\LgiSkinD.dll . > nul
copy ..\..\Gel\debug\*.pdb . > nul

copy ..\code\*.gif . > nul

c:\PROGRA~1\WinZip\winzip32.exe -min -a -r -ex ..\lgires-debug.zip *.*

cd ..
del /q .\lgires-win32\*.*
rmdir lgires-win32

