# Building PVM headless

- Fixed some segfault related to the mouse position
- Made some makefile to be able to run from genode
- Run strace to understand what to expect. Unfortunately, a lot of `openat()` failed.
- Turned out to be tests and class loaders.
- test/good -> probably not really good :) . Failes somewhere on class file parsing. Need to recompile .ph tests.
- ru.dz.phantom.system.boot.ph failed to parse on `pvm_load_type (h=0xffffc178, th=0xffffc148) at load_class.c:70`
- Ok, compiled plc finally. Needed to use windows mkjar.cm



- Need to compile .internal classes as well. Let's try `make` from plib
- Error: `Compile failed: ru.dz.plc.util.PlcException: can't load base class .test.hier.root in addParent (.test.hier.intermediate`
- Fixed! Needed to fix some dependencies manually in the makefile of plib/sys
- Also, `pvm_headless` works. Need to run it on Genode

- Started with creating port file
- Then `./tools/ports/prepare_port`
- And makefile. Found that `TARGET = ...` may wor

- Ok, it at least reaches Phantom's makefile. Now need to fix relative paths and resolve dependencies



- Problem is not in relative paths (partially it was with config.mk
- Didn't really understand what happened. Fixed small bug with : ans :: inside Genode's clean prg
- Small advancement. Defined `GENODE_HOME` using export and suddenly the compilation proceeded to libTinyGl
- Decided to exclude libc and crypto. Libc should be replaced anyways and let's see how crypto is used
- Back to libTinyGl. `cc1: fatal error: /home/anton/genode/contrib/phantom-7b5692dcbe87fc7e4fb528e33c5522f8f832c56d/src/app/phantom/include/ia32/arch/board-pc-config.h: No such file or directory` - quick fix is to rename existing `board-ia32_pc-config.h`, however not sure if it will break anything. Also, don't really understand where it is used. Probably need to understand first. 
- Rule is defined in `makerules`. Seems that `BOARD` variable has to be defined.
- ALSO, add genode CFLAGS. Phantom has its owns. Prbably simple appending will work
- Didn't find specific variable with these flags. Old env setup probably will not work
- HOWEVER, everything compiled till `pvm_main`!!! Need to connect libc
- Ok, firstly, need to configure includes. Seems that make used libc headers from the machine, not from the genode's version.
- Seems that need to rewrite makefile to use CC, CFALGS and e.t.c


- Turned out that Genode's GNU Make build system sets up environment, but requires the software to use automake
- There are not much files. Let's try to use Genode's built in system using `SRC_CC` e.t.c.
- Ok, seems that almost everywhere makerules were used. Main plan: either somehow replace them with Genode ones or copy all makefiles so that we can get the full list of files to compile


- Ok, it is difficult to mix both subsystems. Found a nice workaround. Substitute generated env.sh to `local_config.mk`. Seems that it works :)
- Also fixed a lot of issues by correcting makerules with cpp flags
- ld error can be fixed with ldflags, but a bunch of new errors arise
- ok, errors are not that scary except few
	- pthread: `phread_mutex_(lock|unlock|init)`, `pthread_create`
		- mostly - `unix_hal_unix.c`, probably need to implement own HAL
	- stdio: getchar, putchar, vfprintf, puts
	- memory: malloc, free
	- os: usleep, _exit (yes, with underscore)
	- strange: _udivmoddi4 (more here: `https://gcc.gnu.org/onlinedocs/gccint/Integer-library-routines.html`)


- Fixed makerules-lib, libs should compile with required flags
- Seems that everything compiles. Let's fix ld errors now
- Let's start with small errors and then move to the threads
- Seems that there is some libc files
	- Turned out that they were from `libphantom_c.a`
	- Cleaned libs directory and rerun make
	- Error: missing `libphantom_c.a`...
	- Ok, it is libc. So we can just get rid of this include
- Ok, ld errors look a bit better. Besides pthread, stdio, no strange errors and `_exit`
- Strange, `pvm_main.o` is compiled :D. Eventually, we are struggling with `libphantom_vm.a`, not `pvm_main` :D
- It doesn't require shared libs. Need to try to start it on the genode




- A bit of progress :)
- Using modified makefiles, dry run option and python notebook made a list of files containing sequence of files to be compiled
- Substituted it, needed to make some changes to target.mk such as include "config.h", e.t.c. (more in the comments)
- Need to resolve linking issues now.
	- A lot if "multiple definitions" of `clear_net_timer`. Fixed by moving implementation from header file (why it was there?)
- `unix_hal.c` references: 
	- probably missing `unix_hal_unix.c`
	- console colors

- check stdio, checkt /tests in genode to find stdio
- java from world 
- phantom x64 (ask Zavalishin)
- check linux genode 


- Forgot to add nonstandalone.c and HAL. A lot resolved, but setjmp/longjmp and many more are missing
- Probably, better to use x64. Trying to compile on host
- Disabled libc, lined bsd libc and only few errors left.
- setjmp/longjmp are missing
	- libc should have it already
	- nonstandalone.c was defining (redefining) via asm both of them. Commented out this code (nonstandalone.c:320)
- Seems that libc headers inside Phantom are not really compatible with libc on the host
	- moved them to separate folder
	- excluded from includes
	- asked to link existing ones (removed -nostdinc)
	- c'mon, way too much of errors. Probably using c++ headers...

- Moved Phantom's libc headers to separate directory and working with Genode's ones
	- Lots of errors, most of them fixable.
	- Files to take care about
		- phantom/libphantom/lzma/LzmaDec.c -> requires Bool, had to redefine it in types.h in the same dir
		- threads.h -> part of Phantom and moved them back since since some functions required it. Seem to be not conflicting with Genode headers
		- signal.h -> Probably a big problem. Seems to be not compatible with Genode header.


- Made some progress
	- `signals.h` -> seems that has to be reimplemented. Defined necessary struct in `phantom_types.h` for now. Not sure how it will work.
	- `trap.h` -> traps also seem to be not really a good thing. Fortunately, could disable them
	- `arch-cpu_state.h` -> took from amd64. Probably will work. Was required ffor `thread_private.h` (as well as traps). Seems that also is used for snapshots.
	- network seems to be a hidden problem. It is imported from newos and there are really bad defines to provide compatibility. See `include/newos/compat.h`
	- Be carefull with `gcc_replacements.h`. Can be troublesome

- DONE!!!!!
	- A lot of undefined references, but not that scary :)

- BIG TODO:
	- Check for redefines
	- Label not really good headers and check where are they used
	- Check macros	

- Tried using ubuntu's bsd libc. Didn't work out well. Had a lot of errors and not sure that they are solvable
