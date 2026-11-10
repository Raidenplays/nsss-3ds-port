#!/bin/bash
# run.sh seedtemplate cx cz R R2 [dcx dcz]
cd /home/claude/gentest
rm -rf saves/W cw; cp -r template$1 saves/W; cp -r template$1 cw
if [ -n "$6" ]; then D="$6 $7"; fi
java -cp ".:$(cat cp.txt)" GenPop saves W $2 $3 $4 $5 ${D:+$D jdump.bin} 2>/dev/null > j.txt
/home/claude/hosttest/genpop cw $2 $3 $4 $5 ${D:+$D cdump.bin} > c.txt
echo "$*: $(wc -l < j.txt) chunks, $(diff j.txt c.txt | grep -c '^<') differ"
