import numpy as np
import matplotlib as mpl

BASENAME = "colormap"

COLORMAPS = [
    "viridis",
    "plasma",
    "inferno",
    "magma",
]

COLORMAP_SIZE = 256


def array_declaration(cmap_name):
    return f"const uint8_t {cmap_name}_rgba[{COLORMAP_SIZE}][4]"


def dump_inc(cmap_name):
    cmap = mpl.colormaps[cmap_name].resampled(COLORMAP_SIZE)
    colors_float = cmap(np.linspace(0, 1, COLORMAP_SIZE))
    colors_u8 = (colors_float * 255.0).round().astype(np.uint8)

    with open(f"{cmap_name}.inc", "w") as f:
        f.write(f"// Matplotlib {cmap_name}\n")
        f.write(f"{array_declaration(cmap_name)} = {{\n")

        for r, g, b, a in colors_u8:
            f.write(f"    {{{r:3}, {g:3}, {b:3}, {a:3}}},\n")

        f.write("};\n")


def make_header():
    with open(f"{BASENAME}.h", "w") as f:
        f.write("#pragma once\n")
        f.write("\n")
        f.write(f"#include <stdint.h>\n")
        f.write("\n")
        f.write(f"#define COLORMAP_SIZE {COLORMAP_SIZE}\n")
        f.write("\n")
        f.write("typedef const uint8_t (*const Colormap)[4];\n")
        f.write("\n")
        for cmap in COLORMAPS:
            decl = array_declaration(cmap)
            f.write(f"extern {decl};\n")


def make_implementation():
    with open(f"{BASENAME}.c", "w") as f:
        f.write(f'#include "{BASENAME}.h"\n')
        f.write("\n")
        for cmap in COLORMAPS:
            f.write(f'#include "{cmap}.inc"\n')


make_header()
make_implementation()

for cmap in COLORMAPS:
    dump_inc(cmap)
