#
# make
# make all   -- build everything
#
# make test  -- build all unit tests, do not run
#
# make run   -- run all unit tests (including the textbook chapter tests)
#
# make clean -- remove build files
#
# To reconfigure for Debug build:
#   make clean; make debug; make
#
all:    build
	$(MAKE) -Cbuild $@

test:   build
	$(MAKE) -Cbuild build_tests

run:    test
	ctest --test-dir build

clean:
	rm -rf build

build:
	mkdir $@
	cmake -B$@ -DCMAKE_BUILD_TYPE=RelWithDebInfo

debug:
	mkdir build
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug
