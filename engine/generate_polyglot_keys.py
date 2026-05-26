import chess.polyglot
with open("src/polyglot_keys.h", "w") as f:
    f.write("#ifndef POLYGLOT_KEYS_H\n#define POLYGLOT_KEYS_H\n\n")
    f.write("#include \"types.h\"\n\n")
    f.write("namespace Polyglot {\n")
    f.write("    const U64 Random64[781] = {\n")
    for i in range(781):
        f.write(f"        0x{chess.polyglot.POLYGLOT_RANDOM_ARRAY[i]:016X}ULL,\n")
    f.write("    };\n")
    f.write("}\n\n#endif\n")
