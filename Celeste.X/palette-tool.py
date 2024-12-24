colors = [
    # copied from https://lospec.com/palette-list/pico-8
    (0, 0, 0),
    (29, 43, 83),
    (126, 37, 83),
    (0, 135, 81),
    (171, 82, 54),
    (95, 87, 79),
    (194, 195, 199),
    (255, 241, 232),
    (255, 0, 77),
    (255, 163, 0),
    (255, 236, 39),
    (0, 228, 54),
    (41, 173, 255),
    (131, 118, 156),
    (255, 119, 168),
    (255, 204, 170),
]

def truncate_color(c):
    # 16 bits, RGB 5-6-5
    # r goes first, in the least significant bits
    r, g, b = c
    r >>= 3
    g >>= 2
    b >>= 3
    compressed = r | (g << 5) | (b << 11)
    print(r, g, b, bin(compressed))
    return compressed

truncated = [truncate_color(c) for c in colors]
print(",\n".join([hex(i) for i in truncated]))
