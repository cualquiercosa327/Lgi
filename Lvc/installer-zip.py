#!/usr/bin/env python3
import os
import sys
import shutil
import zipfile

ver = None
lines = open(os.path.join("Src", "Lvc.h")).read().split("\n")
for l in lines:
	if l.find("APP_VERSION") >= 0:
		p = l.split()
		ver = l.split()[-1].strip("\"")
		print("Ver:", ver)
		break

if ver:
	filename = "lvc-" + sys.platform + "-v" + str(ver) + ".zip"
	print("Output:", filename)
	z = zipfile.ZipFile(filename, "w")
	if sys.platform.find("win") >= 0:
		z.write("x64Release\\Lvc.exe", "Lvc.exe")
		z.write("..\\lib\\Lgi12x64.dll", "Lgi12x64.dll")
		z.write("..\\lib\\libpng12x64.dll", "libpng12x64.dll")
	elif sys.platform.find("linux") >= 0:
		z.write("./lvc")
		z.write("../Release/liblgi.so")
	else:
		print("Unexpected platform:", sys.platform)
	z.write("./Resources/Lvc.lr8");
	z.write("./Resources/icon64.png");
	z.write("./Resources/image-list.png")
else:
	print("Error: no version found.")
	