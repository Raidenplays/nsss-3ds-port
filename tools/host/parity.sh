#!/bin/sh
# Parity: the same scenarios on the NSSS jar (Java, headless) and on the port's world code (host
# build), outputs compared byte for byte. Needs the jar (NSSS_JAR) and the Java tools compiled to
# NSSS_TOOLS (javac -cp $NSSS_JAR -d $NSSS_TOOLS tools/nsss/...).
#   tools/host/parity.sh            terrain, populate and the block scripts
set -e
cd "$(dirname "$0")/../.."
JAR=${NSSS_JAR:-"/home/claude/nsss_src/extracted/NSSS a1.1.12/jarmods/a1.1.12.jar"}
TOOLS=${NSSS_TOOLS:-/home/claude/nsss_tools/out}
OUT=${PARITY_OUT:-/tmp/parity}
mkdir -p "$OUT"
CXX="g++ -std=gnu++17 -O1 -ffp-contract=off -w -Isource -Itools/host/shim"
SRC="source/world/*.cpp source/world/gen/*.cpp source/game/SurvivalData.cpp"
for t in alphagen alphapop blocksim; do
    $CXX tools/host/$t.cpp $SRC -o "$OUT/$t" -lz
done
J() { java -cp "$JAR:$TOOLS" "$@"; }
fail=0
same() { if cmp -s "$1" "$2"; then echo "  same: $3"; else echo "  DIFFERENT: $3"; fail=1; fi; }

echo "terrain (ChunkProviderGenerate.provideChunk)"
for seed in 12345 -42 1 987654321 31337 -7777777; do
    J com.mojang.minecraft.core.level.chunk.AlphaGenDump $seed -3 -3 3 3 "$OUT/gen_j_$seed" 2>/dev/null
    "$OUT/alphagen" $seed -3 -3 3 3 "$OUT/gen_c_$seed"
    same "$OUT/gen_j_$seed" "$OUT/gen_c_$seed" "seed $seed"
done

echo "populate (ChunkProviderGenerate.populate)"
for seed in 12345 -42 1 987654321 31337 -7777777; do
    rm -rf "$OUT/pj" "$OUT/pc"
    J com.mojang.minecraft.core.level.AlphaPopDump $seed "$OUT/pj" 0 0 4 2 "$OUT/pop_j_$seed" 2>/dev/null
    "$OUT/alphapop" $seed "$OUT/pc" 0 0 4 2 "$OUT/pop_c_$seed"
    same "$OUT/pop_j_$seed" "$OUT/pop_c_$seed" "seed $seed"
done

echo "block scripts (tools/nsss/sims)"
for s in tools/nsss/sims/*.txt; do
    n=$(basename "$s" .txt)
    rm -rf "$OUT/sj" "$OUT/sc"
    J com.mojang.minecraft.core.level.BlockSimDump 12345 "$OUT/sj" "$s" "$OUT/sim_j_$n" 2>/dev/null
    "$OUT/blocksim" 12345 "$OUT/sc" "$s" "$OUT/sim_c_$n"
    same "$OUT/sim_j_$n" "$OUT/sim_c_$n" "$n"
done
exit $fail
