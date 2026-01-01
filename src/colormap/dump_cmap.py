import numpy as np
import matplotlib as mpl


def dump(cmap_name, n_colors=256):
    cmap = mpl.colormaps[cmap_name].resampled(n_colors)
    # Returns an (N, 4) array of RGBA floats [0.0, 1.0]
    colors = cmap(np.linspace(0, 1, n_colors))

    with open(f"{cmap_name}.inc", "w") as f:
        f.write(f"// Matplotlib {cmap_name} colormap\n")
        f.write(f"float {cmap_name}_rgba[{n_colors}][4] = {{\n")
        for r, g, b, a in colors:
            f.write(f"    {{{r:.6f}f, {g:.6f}f, {b:.6f}f, {a:.6f}f}},\n")
        f.write("};\n")


dump("viridis")
dump("plasma")
dump("inferno")
dump("magma")
