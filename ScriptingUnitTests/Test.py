import sys
import os
import subprocess

files = []
scripts_path = os.path.join(os.getcwd(), "Scripts")
exe_path = os.path.join(os.getcwd(), "x64Debug\\LgiScript.exe")

dir = os.listdir(scripts_path)
for d in dir:
	full = os.path.join(scripts_path, d)
	if not os.path.isdir(full) and full.lower().find(".script") > 0:
		files.append(full)

for f in files:
	args = [exe_path, f]
	try:
		result = subprocess.check_output(args, stderr=subprocess.STDOUT)
		# print result
		print(f, "OK")
	except:
		# print("Process error:", sys.exc_info()[0], "\n", args[0], args[1])
		print(f, "FAILED")

