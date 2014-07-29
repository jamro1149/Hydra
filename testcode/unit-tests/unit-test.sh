#!/bin/bash

failures=0
successes=0
failues=0
red='\e[0;31m'
green='\e[0;32m'
default='\e[0m'

success() {
  echo -e "${green}test succeeded${default}:\t$1".
  successes=$(($successes + 1))
}

failure() {
  echo -e "${red}test failed${default}:\t${1}."
  failues=$(($failues + 1))
}

newline=[--------------------------------\
---------------------------------------]

echo $newline
echo [----------------------------Hydra Unit\
  Tests---------------------------]
echo $newline
echo
echo
echo

# run for each analysis pass
for a in fitness profitability joinpoints decider; do
  opt -load ~/proj-files/build/Release/lib/Tests.so \
    -test-module-${a} -o test-module-${a}.bc blank.bc
  opt -load ~/proj-files/build/Release/lib/Analyses.so -$a \
    -analyze test-module-${a}.bc > test-$a-output
  if cmp test-$a-output test-$a-expected
  then success $a
  else failure $a
  fi
  rm test-module-${a}.bc test-$a-output
done

# run for each transformation pass
for t in makespawnable parallelisecalls; do
  opt -load ~/proj-files/build/Release/lib/Tests.so \
    -test-module-${t} -o test-module-${t}.bc blank.bc
  opt -load ~/proj-files/build/Release/lib/Analyses.so \
    -load ~/proj-files/build/Release/lib/Transforms.so -$t \
    test-module-${t}.bc -o test-module-${t}-after.bc
  if cmp test-module-$t-after.bc test-module-$t-expected.bc
  then success $t
  else failure $t
  fi
  rm test-module-${t}.bc test-module-$t-after.bc
done

echo
echo
echo
echo [--------------------------------Summary\
--------------------------------]
echo
echo
echo -e "${green}${successes} successes${default}."
echo -e "${red}${failues} failures${default}."
echo
echo $newline
