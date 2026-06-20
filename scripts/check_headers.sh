#!/bin/sh
#
# check_headers.sh — verify the BESM-6 standard headers preprocess and parse.
#
# For every <header.h> in the include directory (and once for all of them
# together) build a tiny translation unit that includes it, run it through the
# system C preprocessor, then feed the result to the `parse` front end.  A
# non-zero exit from cpp or parse fails the test, catching syntax errors, bad
# macro expansions and accidental comment terminators in the headers.
#
# Usage: check_headers.sh <cpp> <parse> <include-dir> <work-dir>
#
set -u

CPP="$1"
PARSE="$2"
INCDIR="$3"
WORK="$4"

mkdir -p "$WORK"
status=0
combined="$WORK/_all.c"
: > "$combined"

for h in "$INCDIR"/*.h; do
    n=$(basename "$h")
    src="$WORK/use_${n%.h}.c"
    pre="$WORK/use_${n%.h}.i"
    ast="$WORK/use_${n%.h}.ast"

    printf '#include <%s>\nint main(void){return 0;}\n' "$n" > "$src"

    if ! "$CPP" -E -nostdinc -I"$INCDIR" "$src" > "$pre" 2>"$WORK/cpp.err"; then
        echo "PREPROCESS FAILED: $n"
        cat "$WORK/cpp.err"
        status=1
        continue
    fi
    if ! "$PARSE" "$pre" "$ast" > "$WORK/parse.err" 2>&1; then
        echo "PARSE FAILED: $n"
        cat "$WORK/parse.err"
        status=1
        continue
    fi

    printf '#include <%s>\n' "$n" >> "$combined"
done

# One TU that includes everything at once (catches cross-header clashes).
printf 'int main(void){return 0;}\n' >> "$combined"
if ! "$CPP" -E -nostdinc -I"$INCDIR" "$combined" > "$WORK/_all.i" 2>"$WORK/cpp.err"; then
    echo "PREPROCESS FAILED: <all headers>"
    cat "$WORK/cpp.err"
    status=1
elif ! "$PARSE" "$WORK/_all.i" "$WORK/_all.ast" > "$WORK/parse.err" 2>&1; then
    echo "PARSE FAILED: <all headers>"
    cat "$WORK/parse.err"
    status=1
fi

if [ "$status" -eq 0 ]; then
    echo "All BESM-6 headers preprocess and parse cleanly."
fi
exit "$status"
