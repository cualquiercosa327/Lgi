# This script creates a source and binary release
import os
import sys
import stat

def RemoveFolder(path):
	d = os.listdir(path)
	for f in d:
		full = path + os.sep + f
		if os.path.isdir(full):
			RemoveFolder(full)
		else:
			os.chmod(full, stat.S_IWRITE | stat.S_IREAD)
			os.remove(full)
	os.rmdir(path)			

def RemoveSvnFolders(path):
	d = os.listdir(path)
	for f in d:
		full = path + os.sep + f
		if os.path.isdir(full):
			if f == ".svn":
				RemoveFolder(full)
			else:
				RemoveSvnFolders(full)
			

tmp = open("include" + os.sep + "common" + os.sep + "lgi.h").read().split("\n")
for line in tmp:
	if line.find("LGI_VER") > 0:
		p = line.split()
		version = p[2].strip("\"")
package_name = os.getcwd().split(os.sep)[-2].lower() + "-" + sys.platform + "-" + version
out = os.path.abspath(os.getcwd() + os.sep + ".." + os.sep + package_name)
print "Package name:", package_name

if not os.path.exists(out):
	os.mkdir(out)
if not os.path.exists(out):
	raise AssertionError("OutDir doesn't exist.")

print "Checking out Lgi..."
RemoveFolder(out)
r = os.system("svn co https://lgi.svn.sourceforge.net/svnroot/lgi/trunk " + out)

print "Striping out .svn folders..."
RemoveSvnFolders(out)

print "Create the documentation..."
docs_folder = os.path.join(out, "docs")
cmd = "doxygen " + os.path.join(docs_folder, "lgi.dox")
os.chdir(docs_folder)
os.system(cmd);
