.PHONY: all clean cleanall makefiles makefiles-lib checkmakefiles


all: checkmakefiles
	+$(MAKE) -C src all

clean: checkmakefiles
	+$(MAKE) -C src clean

cleanall: checkmakefiles
	@+$(MAKE) -C src MODE=release clean
	@+$(MAKE) -C src MODE=debug clean

MAKEMAKE_OPTIONS := -f --deep -o libncs_omnet -O out \
	-I. \
	-I../../libncs_matlab/out \
	-I$$MCR_ROOT/extern/include \
	-I../../matlab-scheduler/src \
	-I../../inet/src

makefiles: $(wildcard .oppfeaturestate) .oppfeatures makefiles-lib

makefiles-lib:
	@FEATURE_OPTIONS=$$(opp_featuretool options -c -f -l | sed 's#-X/#-X#') && cd src && opp_makemake --make-so $(MAKEMAKE_OPTIONS) $$FEATURE_OPTIONS

checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '========================================================================'; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '========================================================================'; \
	echo; \
	exit 1; \
	fi
