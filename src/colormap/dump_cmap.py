import numpy as np
import matplotlib as mpl

def dump(cmap_name, n_colors=256):
    cmap = mpl.colormaps[cmap_name].resampled(n_colors)
    colors_float = cmap(np.linspace(0, 1, n_colors))
    colors_u8 = (colors_float * 255.0).round().astype(np.uint8)

    with open(f"{cmap_name}.inc", "w") as f:
        f.write(f"// Matplotlib {cmap_name}\n")
        f.write(f"static const unsigned char {cmap_name}_rgba[{n_colors}][4] = {{\n")

        for r, g, b, a in colors_u8:
            f.write(f"    {{{r:3}, {g:3}, {b:3}, {a:3}}},\n")

        f.write("};\n")


dump("viridis")
dump("plasma")
dump("inferno")
dump("magma")
