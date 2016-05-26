import os
import sys

Key = "#include \"Instructions.h\""
GScriptVmCpp = open("GScriptVM.cpp", "r").read()
InstructionsH = open("Instructions.h", "r").read()
Insert = None

parts = GScriptVmCpp.split(Key);
print "Parts:", len(parts)
if len(parts) == 3:
	print "Converting #include to inline..."
	Insert = InstructionsH
elif len(parts) == 1:
	print "Converting inline to #include..."
	parts = GScriptVmCpp.split(InstructionsH)
	Insert = Key
else:
	print "Unknown number of parts."
	parts = None

if len(parts) > 0:
	out = Insert.join(parts)
	f = open("GScriptVM.cpp", "w")
	f.truncate(0)
	f.write(out)
else:
	print "Error: Parts=", len(parts)
