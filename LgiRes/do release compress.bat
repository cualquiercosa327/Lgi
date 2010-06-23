mkdir lgires-win32

del /Q lgires-win32.zip
del /Q lgires-win32\*.*
cd lgires-win32

copy ..\Release\*.exe . > nul
copy ..\LrStrip\Release\*.exe . > nul
copy ..\LrStrip\Code\LrStrip.lr8 . > nul
copy ..\Code\lgires.lr8 . > nul
copy ..\..\Release\Lgi.dll . > nul
copy ..\..\Gel\Release\LgiSkin.dll . > nul

"c:\Program Files\Upx\upx.exe" -9 lgires.exe
"c:\Program Files\Upx\upx.exe" -9 lrstrip.exe
"c:\Program Files\Upx\upx.exe" -9 Lgi.dll
"c:\Program Files\Upx\upx.exe" -9 LgiSkin.dll

copy ..\code\*.gif . > nul

c:\PROGRA~1\WinZip\winzip32.exe -min -a -r -ex ..\lgires-win32.zip *.*

cd ..
del /q .\lgires-win32\*.*
rmdir lgires-win32

