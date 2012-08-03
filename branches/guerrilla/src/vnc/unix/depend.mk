#
# C / C++ header dependency stuff
#
# Needs GNU make and vncmkdepend, a hacked version of makedepend

.SUFFIXES: .d

CMAKEDEPEND = vncmkdepend
CXXMAKEDEPEND = vncmkdepend

#
# The recommended method of doing dependency analysis in the GNU make manual
# turns out to be painfully slow.  This method is similar but it's
# substantially faster and retains the desirable property that the user doesn't
# need to manually invoke a "make depend" step.
#
# As with the method described in the manual, we generate a separate dependency
# (.d) file for each source file.  The .d file records the header files that
# each C or C++ source file includes.  Any source file recorded in SRCS or
# CXXSRCS will cause us to try and include the corresponding .d file and GNU
# make then treats each .d file as a target to be remade.
#
# Unlike the manual's method, the rule we provide for making the .d file is
# actually a fake.  All it does is record in a temporary file that the .d file
# needs to be remade.  But as well as all the .d files, we also try to include
# a file called "depend.phony".  This file never exists, but it causes GNU make
# to try and make the target "depend.phony".  The rule for depend.phony then
# looks at the temporary files generated by the .d rules and then invokes the
# "omkdepend" program on all of the source files in one go.
#

#
# We use simple assignment here to remove any of the depend.tmp files
# at the time make parses this bit.
#

dummyvariable := $(shell $(RM) cdepend.tmp cxxdepend.tmp)

#
# Now the "fake" rules for generating .d files.
#

%.d: %.c
	@echo "$<" >> cdepend.tmp

%.d: %.cxx
	@echo "$<" >> cxxdepend.tmp

#
# The depend.phony rule which actually runs omkdepend.
#

depend.phony:
	@if [ -f cdepend.tmp ]; then \
	   echo $(CMAKEDEPEND) $(ALL_CPPFLAGS) `cat cdepend.tmp`; \
	   $(CMAKEDEPEND) $(ALL_CPPFLAGS) `cat cdepend.tmp`; \
	   rm -f cdepend.tmp; \
	 fi; \
	 if [ -f cxxdepend.tmp ]; then \
	   echo $(CXXMAKEDEPEND) $(ALL_CPPFLAGS) `cat cxxdepend.tmp`; \
	   $(CXXMAKEDEPEND) $(ALL_CPPFLAGS) `cat cxxdepend.tmp`; \
	   rm -f cxxdepend.tmp; \
	 fi

#
# Now include the .d files and the "depend.phony" file which never exists.
# For some reason GNU make evaluates the targets in reverse order, so we need
# to include depend.phony first.  The "-" tells make not to complain that it
# can't find the file.
#

-include depend.phony

ifdef SRCS
-include $(patsubst %.c,%.d,$(patsubst %.cxx,%.d,$(SRCS)))
endif