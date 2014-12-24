import os
import sys

Key = "#include \"Instructions.h\""
GScriptVmCpp = open("GScriptVM.cpp", "r").read()
InstructionsH = open("Instructions.h", "r").read()
Insert = None

parts = GScriptVmCpp.split(Key);
if len(parts) < 4:
	print "Already converted... converting back..."
	parts = GScriptVmCpp.split(InstructionsH)
	Insert = Key
else:
	print "Converting..."
	Insert = InstructionsH

if len(parts) == 4:
	out = Insert.join(parts)
	f = open("GScriptVM.cpp", "w")
	f.truncate(0)
	f.write(out)
else:
	print "Error: Parts=", len(parts)
