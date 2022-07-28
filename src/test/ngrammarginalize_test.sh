#!/bin/bash
# Description:
# Tests the command line binary ngrammarginalize.

bin=../bin
testdata=$srcdir/testdata
tmpdata=${TMPDIR:-/tmp}
tmpsuffix="$(mktemp -u XXXXXXXX 2>/dev/null)"
tmpprefix="${tmpdata}/ngrammarg-earnest-katz-$tmpsuffix-$RANDOM-$$"

trap "rm -f ${tmpprefix}*" 0 2 13 15

set -e
compile_test_fst() {
  if [ ! -e "${tmpprefix}-${1}.ref" ]
  then
    fstcompile \
      -isymbols="${testdata}/${1}.sym" -osymbols="${testdata}/${1}.sym" \
      -keep_isymbols -keep_osymbols -keep_state_numbering \
      "${testdata}/${1}.txt" "${tmpprefix}-${1}.ref"
  fi
}

compile_test_fst earnest-katz.mod
compile_test_fst earnest-katz.marg.mod
compile_test_fst earnest-katz.marg.iter2.mod
"${bin}/ngrammarginalize" "${tmpprefix}"-earnest-katz.mod.ref \
  "${tmpprefix}".marg.mod

fstequal \
  "${tmpprefix}"-earnest-katz.marg.mod.ref "${tmpprefix}".marg.mod

"${bin}/ngrammarginalize" \
  --steady_state_file="${tmpprefix}"-earnest-katz.marg.mod.ref \
  "${tmpprefix}"-earnest-katz.mod.ref "${tmpprefix}".marg.iter2.mod

fstequal \
  "${tmpprefix}"-earnest-katz.marg.iter2.mod.ref "${tmpprefix}".marg.iter2.mod

"${bin}/ngrammarginalize" --iterations=2 \
  "${tmpprefix}"-earnest-katz.mod.ref "${tmpprefix}".marg.iter2I.mod

fstequal \
  "${tmpprefix}"-earnest-katz.marg.iter2.mod.ref "${tmpprefix}".marg.iter2I.mod

echo PASS
