#! python3
#!/usr/bin/env python
import os
import sys

def process(s, vars):
	i = 0
	l = len(s)
	out = ""
	while i < l:
		pos1 = s.find("%", i)
		if pos1 < 0:
			# no more instances
			out = out + s[i:]
			return out
		else:
			# look for the end var marker
			pos2 = s.find("%", pos1 + 1)
			if pos2 < 0:
				out = out + s[i:]
				break;
			
			# add any content since the last var
			out = out + s[i:pos1]

			# process var			
			var = s[pos1+1:pos2]
			if var in vars:
				out = out + vars[var]
			i = pos2 + 1

	return out + s[i:]

def process_folder(out_folder, in_folder, vars):

	if not os.path.exists(out_folder):
		os.mkdir(out_folder)

	files = os.listdir(in_folder)
	for idx, file in enumerate(files):
		if file.find(".py") > 0:
			del files[idx]

	for f in files:
		in_path = os.path.join(in_folder, f)
		if os.path.isdir(in_path):
			child_out = os.path.join(out_folder, process(f, vars))
			process_folder(child_out, in_path, vars)
		else:
			# print("in_path:", in_path)
			out_path = os.path.abspath(os.path.join(out_folder, process(f, vars)))
			# print("out_path:", out_path)

			in_data = open(in_path, "r").read()
			out_data = process(in_data, vars)
			open(out_path, "w").write(out_data)
			print("Wrote:", out_path)
	

if len(sys.argv) < 2:
	print("Usage:", os.path.basename(sys.argv[0]), " <template_path> [<project_name>]")
	sys.exit(-1)

template_path = sys.argv[1]
if not os.path.exists(template_path):
	print("Error: The path '"+template_path+"' doesn't exist.")
	sys.exit(-1)

if 1:
	dest_path = os.path.abspath(os.path.join(sys.argv[0], ".."))
	print("dest_path:", dest_path)
	print("template_path:", template_path)

	# set up the variables
	vars = dict()

	# find the Lgi folder based on the template path
	parts = template_path.split(os.sep)
	for i in range(len(parts)):
		r = (".." + os.sep) * i
		t = os.path.abspath(os.path.join(template_path, r, "Lgi", "trunk", "lgi_vc9.sln"))
		# print(i, t)
		if os.path.exists(t):
			lgi = os.path.abspath(os.path.join(template_path, r, "Lgi", "trunk"))
			rel = os.path.relpath(lgi, dest_path)
			if rel is not None:
				vars["lgi.folder"] = rel
			else:
				vars["lgi.folder"] = lgi
			print("Lgi:", vars["lgi.folder"])
			break

	if "lgi.folder" not in vars:
		print("Error: The LGI path wasn't found.")
		raw_input("Wait")
		sys.exit(-1)

	if len(sys.argv) > 2:
		proj_name = sys.argv[2]
		print("Project name:", proj_name)
	else:
		proj_name = input("Project name: ")
	vars["name"] = proj_name

	process_folder(dest_path, template_path, vars)


#except:
#	print("Unexpected error:", sys.exc_info()[0])
# input("Wait")
