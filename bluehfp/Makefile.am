## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src 

phone_daemondocdir = 
phone_daemondoc_DATA =


INTLTOOL_FILES = intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = \
	autogen.sh \
	*.spec

DISTCLEANFILES = intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache

rpm:dist
	mkdir -p rpmbuild/{SOURCES,SPECS,SRPMS,RPMS,BUILD}
	rpmbuild --define="_topdir `pwd`/rpmbuild" --target=i586 -ta $(distdir).tar.gz
	mv rpmbuild/SRPMS/*rpm .
	mv rpmbuild/RPMS/i586/*rpm .
	rm -rf rpmbuild $(distdir).tar.gz
clean:
	rm -rf *.rpm

# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(phone_daemondocdir) 
