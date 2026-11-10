#!/bin/bash
# vehicles_compare.sh: minecart and boat physics, tick by tick, against the Java game
# (build carttest/boattest with the genpop sources plus game/{Vehicles,Player,Inventory,ItemTable}.cpp
#  and -I a stub dir for citro3d.h / citro2d.h / 3ds.h)
cd /home/claude/gentest
for v in 1.2 0.3 2.5 -0.8; do
  rm -rf saves/C; cp -r template12345 saves/C; rm -rf /tmp/cw; cp -r template12345 /tmp/cw
  java -cp ".:$(cat cp.txt)" CartTest saves C $v 120 2>/dev/null > j.txt
  /home/claude/hosttest/carttest /tmp/cw $v 120 > c.txt
  echo "cart $v: $(diff j.txt c.txt | grep -c '^<') differ"
done
for a in "0.3 0.1 80" "0.39 0 60"; do
  rm -rf saves/B; cp -r template12345 saves/B; rm -rf /tmp/bw; cp -r template12345 /tmp/bw
  java -cp ".:$(cat cp.txt)" BoatTest saves B $a 2>/dev/null > j.txt
  /home/claude/hosttest/boattest /tmp/bw $a > c.txt
  echo "boat $a: $(diff j.txt c.txt | grep -c '^<') differ"
done
