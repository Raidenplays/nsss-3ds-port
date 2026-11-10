# NSSS tools

Run against the Not So Seecret Saturday jar (`a1.1.12.jar` from the MultiMC instance zip). The jar
is a full, deobfuscated game, so these load its classes directly; no mappings needed. The jar
itself is not in the repo.

    JAR=path/to/a1.1.12.jar
    javac -cp "$JAR" -d out DumpNsssBlocks.java com/mojang/minecraft/core/level/chunk/AlphaGenDump.java
    java -cp "out:$JAR" DumpNsssBlocks generated/blocks.tsv
    java -cp "out:$JAR" com.mojang.minecraft.core.level.chunk.AlphaGenDump <seed> x0 z0 x1 z1 java.bin

Terrain parity (from the repo root):

    g++ -std=c++17 -O2 -ffp-contract=off -Isource tools/host/alphagen.cpp source/world/gen/AlphaGen.cpp -o alphagen
    ./alphagen <seed> x0 z0 x1 z1 cpp.bin && cmp cpp.bin java.bin

`generated/blocks.tsv` columns: id, class, material, opacity, light, opaqueCube, normal,
renderType, pass, hardness, resistance, tickRate, tickOnLoad, container, bounds, textures
(16 metadata × 6 sides).

Populated-world parity (generation + decoration, same chunk order on both sides):

    java -cp "out:$JAR" com.mojang.minecraft.core.level.AlphaPopDump <seed> <tmpdir> cx cz loadR reportR java.bin
    g++ -std=gnu++17 -O1 -ffp-contract=off -Isource -Iandroid/shim tools/host/alphapop.cpp \
        source/world/*.cpp source/world/gen/*.cpp source/game/SurvivalData.cpp -o alphapop -lz
    ./alphapop <seed> <emptydir> cx cz loadR reportR cpp.bin && cmp java.bin cpp.bin

Block behaviour parity (scripted placements + scheduled ticks; scripts in sims/):

    java -cp "out:$JAR" com.mojang.minecraft.core.level.BlockSimDump <seed> <tmpdir> sims/X.txt java.bin
    g++ ... tools/host/blocksim.cpp ... -o blocksim && ./blocksim <seed> <emptydir> sims/X.txt cpp.bin
