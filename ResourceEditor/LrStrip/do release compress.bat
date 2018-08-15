del /Q .\lrstrip-win32.zip
cd lrstrip-win32
del /Q *.*

copy d:\code\lgi\release\*.dll . > nul
copy ..\release\*.exe . > nul

"D:\Program Files\Upx\upx.exe" -9 lgi.dll
"D:\Program Files\Upx\upx.exe" -9 lrstrip.exe

D:\PROGRA~1\WinZip\winzip32.exe -min -a -r -ex ..\lrstrip-win32.zip *.*
