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

if len(sys.argv) < 2:
	print "Usage:", os.path.basename(sys.argv[0]), " <template_path> [<project_name>]"
	sys.exit(-1)

template_path = sys.argv[1]
if not os.path.exists(template_path):
	print "Error: The path '"+template_path+"' doesn't exist."
	sys.exit(-1)

try:
	dest_path = os.path.abspath(os.path.join(sys.argv[0], ".."))
	print "dest_path:", dest_path
	print "template_path:", template_path

	# set up the variables
	vars = dict()

	# find the Lgi folder based on the template path
	parts = template_path.split(os.sep)
	for i in range(len(parts)-1):
		r = (".." + os.sep) * i
		t = os.path.join(template_path, r, "Lgi", "trunk", "lgi_vc9.sln")
		if os.path.exists(t):
			lgi = os.path.abspath(os.path.join(template_path, r, "Lgi", "trunk"))
			rel = os.path.relpath(lgi, dest_path)
			if rel is not None:
				vars["lgi.folder"] = rel
			else:
				vars["lgi.folder"] = lgi
			print "Lgi:", vars["lgi.folder"]
			break

	if "lgi.folder" not in vars:
		print "Error: The LGI path wasn't found."
		raw_input("Wait")
		sys.exit(-1)

	if len(sys.argv) > 2:
		proj_name = sys.argv[2]
		print "Project name:", proj_name
	else:
		proj_name = raw_input("Project name: ")
	vars["name"] = proj_name

	files = os.listdir(template_path)
	for idx, file in enumerate(files):
		if file.find(".py") > 0:
			del files[idx]

	print "Input files:"
	for f in files:
		in_path = os.path.join(template_path, f)
		# print "in_path:", in_path
		out_path = os.path.abspath(os.path.join(dest_path, process(f, vars)))
		# print "out_path:", out_path

		in_data = open(in_path, "r").read()
		out_data = process(in_data, vars)
		open(out_path, "w").write(out_data)
		print "Wrote:", out_path

except:
	print "Unexpected error:", sys.exc_info()[0]
	# raw_input("Wait")
