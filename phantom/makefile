include $(realpath $(PHANTOM_HOME))/config.mk

default::
	-@mkdir -p lib
	@$(MAKE) all

ifeq ($(ARCH),arm)
EXCLUDED_SUBDIRS=apps
endif

# subdir offsets - turned off for seems to be bad idea :(

# SUBDIRS =  $(filter-out $(EXCLUDED_SUBDIRS),  libc crypt gl dev libphantom threads newos libtuned libwin libkern libfreetype vm tmalloc user modules apps)
# SUBDIRS = $(addprefix $(PHANTOM_HOME)/, $(filter-out $(EXCLUDED_SUBDIRS),  libc crypt gl dev libphantom threads newos libtuned libwin libkern libfreetype vm tmalloc user modules apps))
# SUBDIRS =  $(addprefix phantom/, $(filter-out $(EXCLUDED_SUBDIRS),  libc crypt gl dev libphantom threads newos libtuned libwin libkern libfreetype vm tmalloc user modules apps))
SUBDIRS =  $(addprefix phantom/, $(filter-out $(EXCLUDED_SUBDIRS),  gl dev libphantom threads newos libtuned libwin libkern libfreetype vm tmalloc user modules apps))


%.MAKE:
	@$(MAKE) -C $(dir $@) $(basename $(notdir $@))
%-subdirs: $(addsuffix /%.MAKE,$(SUBDIRS)) ;

# By default, make any target by descending into subdirectories.
%: %-subdirs ;

all: all-subdirs
	cd modules ; make install ; cd ..

install: install-subdirs


clean: clean-subdirs
	rm -f lib/*.a lib/crt0.o 

analyse:: analyse-subdirs

# Genode porting related

vmonly:
	# building
	$(MAKE) -C gl
	$(MAKE) -C libtuned
	$(MAKE) -C libphantom
	$(MAKE) -C libwin
	$(MAKE) -C libfreetype
	# $(MAKE) -C libc
	$(MAKE) -C vm pvm_genode
	
vmonly-clean:
	# cleaning
	$(MAKE) -C gl clean
	$(MAKE) -C libtuned clean
	$(MAKE) -C libphantom clean
	$(MAKE) -C libwin clean
	$(MAKE) -C libfreetype clean
	$(MAKE) -C libc clean
	$(MAKE) -C vm clean

