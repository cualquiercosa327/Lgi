#!/usr/bin/env python3
import os
import sys
import shutil
import zipfile
import subprocess
import glob

ver = None
lines = open(os.path.join("Code","LgiIde.h")).read().split("\n")
for l in lines:
	if l.find("APP_VER") >= 0:
		p = l.split()
		print(p)
		ver = l.split()[2].strip("\"")
		break

def Path(s):
	return os.sep.join(s.split("/"))

if ver:
	filename = "lgiide-" + sys.platform.replace("32","64") + "-v" + str(ver) + ".zip"
	print("Output:", filename)
	z = zipfile.ZipFile(filename, "w")
	if sys.platform.find("win") >= 0:
		z.write("x64Release12\\LgiIde.exe", "LgiIde.exe")
		z.write("..\\lib\\Lgi12x64.dll", "Lgi12x64.dll")
	elif sys.platform.find("linux") >= 0:
		subprocess.call(["strip", "clipshare", "../Lgi/trunk/Release/liblgi.so"])
		z.write("./lgiide")
		z.write("../Lgi/trunk/Release/liblgi.so")
	else:
		print("Unexpected platform:", sys.platform)
	
	for file in glob.glob(Path("./Resources/*.png")):
		z.write(file);
	z.write(Path("./Resources/LgiIde.lr8"));
	