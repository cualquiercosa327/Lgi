#!/usr/bin/make
# Makes the distribution file

.SILENT :

Name = Lgi
File = $(Name)-src

source :
	# setup
	-rm -rf $(Name)
	-mkdir $(Name)
	# docs
	#cd docs ; doxygen ./lgi.dox
	# copy in files
	cp Distro.make $(Name)
	cp Makefile.* $(Name)
	cp Lgi.ds* $(Name)
	cp LgiStatic.ds* $(Name)
	cp Info.plist $(Name)
	cp Lgi.proj $(Name)
	cp Lgi.sln $(Name)
	cp *.xml $(Name)
	cp Lgi_Prefix.pch $(Name)
	cp license.txt $(Name)
	cp readme.txt $(Name)
	cp release.bat $(Name)
	cp -r src $(Name)
	cp -r docs $(Name)
	cp -r include $(Name)
	mkdir $(Name)/LgiIde
	cp -r LgiIde/* $(Name)/LgiIde
	-rm -rf $(Name)/LgiIde/lgiide
	-rm -rf $(Name)/LgiIde/lgiide.txt
	-rm -rf $(Name)/LgiIde/lgiide.r
	-rm -rf $(Name)/LgiIde/DebugX
	-rm -rf $(Name)/LgiIde/ReleaseX
	mkdir $(Name)/LgiRes
	cp -r LgiRes/* $(Name)/LgiRes
	-rm -rf $(Name)/LgiRes/lgires
	-rm -rf $(Name)/LgiRes/DebugX
	-rm -rf $(Name)/LgiRes/ReleaseX
	-rm -rf $(Name)/src/common/Mfs
	-find $(Name) -iname ".svn" -exec rm -rf '{}' \;  2> /dev/null
	# archive
	-rm $(File).* 2> /dev/null
	tar -cf $(File).tar $(Name)/*
	bzip2 -z $(File).tar 
	ls -l $(File).tar.bz2
	# finish
	-rm -rf $(Name)
	