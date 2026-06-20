#
# make
# make all      -- build everything
#
# make test       -- run unit tests
#
# make book_tests -- build and run the textbook chapter tests
#
# make clean    -- remove build files
#
# To reconfigure for Debug build:
#   make clean; make debug; make
#
all:    build
	$(MAKE) -Cbuild $@

test:   build
	$(MAKE) -Cbuild build_tests
	ctest --test-dir build -LE book -E _NOT_BUILT

book_tests book_test: build
	$(MAKE) -Cbuild book_tests
	cd build/backend/besm6 && ctest --test-dir $(CURDIR)/build -L book

clean:
	rm -rf build

build:
	mkdir $@
	cmake -B$@ -DCMAKE_BUILD_TYPE=RelWithDebInfo

debug:
	mkdir build
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug
