def parse_p8(name):
    raw_lines = open("celeste.p8").readlines()
    current_section = "__header__"
    sections = {}
    sections[current_section] = []
    for line in raw_lines:
        line = line.strip()
        if line.startswith("__"):
            current_section = line
            sections[current_section] = []
        else:
            sections[current_section].append(line)
    return sections

def format_c_array(numbers, bs):
    result = "{\n"
    for start in range(0, len(numbers), bs):
        result += "    "
        for i in range(start, start + bs):
            result += (hex(numbers[i]) + ",").ljust(6, ' ')
        result += "\n"
    result += "}"
    return result

def parse_4bit_stream(d):
    return sum([[int(i, 16) for i in row.strip()] for row in d], [])

def parse_8bit_stream_msb_first(d):
    result = []
    for row in d:
        for a, b in zip(row[::2], row[1::2]):
            result.append(int(a, 16) * 16 + int(b, 16))
    return result

def parse_8bit_stream_lsb_first(d):
    result = []
    for row in d:
        for a, b in zip(row[::2], row[1::2]):
            result.append(int(a, 16) + int(b, 16) * 16)
    return result

def get_gfx_pixel(x, y):
    return gfx_data[y * 128 + x]

def get_sprite_pixels(sprite_index):
    sprite_row = sprite_index // 16
    sprite_col = sprite_index % 16
    data = []
    for y in range(sprite_row * 8, (sprite_row * 8) + 8):
        for x in range(sprite_col * 8, (sprite_col * 8) + 8):
            data.append(get_gfx_pixel(x, y))
    return data

def compress_sprite(pixels):
    compressed = []
    for a, b in zip(pixels[::2], pixels[1::2]):
        compressed.append((a << 4) | b)
    for i in compressed:
        if i > 255 or i < 0 or type(i) != int:
            raise Exception("Compression resulted in non-byte")
    return compressed

def get_all_sprites():
    sprites = [get_sprite_pixels(i) for i in range(0, 128)]
    compressed = [compress_sprite(sprite) for sprite in sprites]
    flat = sum(compressed, [])
    return "static const uint8_t progmem_sprite_data[] PROGMEM = " + format_c_array(flat, 32) + ";"



def get_map_tile(x, y):
    return map_data[y * 128 + x]

def get_map_tiles(map_index):
    map_row = map_index // 8
    map_col = map_index % 8
    data = []
    for y in range(map_row * 16, (map_row * 16) + 16):
        for x in range(map_col * 16, (map_col * 16) + 16):
            data.append(get_map_tile(x, y))
    return data

def get_all_maps():
    maps = [get_map_tiles(i) for i in [31, 0, 1, 2, 4, 5, 8, 11, 12, 18, 20, 21, 22, 23, 25, 26, 28, 30]]
    flat = sum(maps, [])
    return "static const uint8_t progmem_stored_maps[] PROGMEM = " + format_c_array(flat, 32) + ";"

def get_all_sprite_flags():
    return "static const uint8_t progmem_sprite_flags[] PROGMEM = " + format_c_array(gff_data, 32) + ";"

p8 = parse_p8("celeste.p8")

gfx_top = parse_4bit_stream(p8["__gfx__"][0:64])
gfx_data = gfx_top

map_top = parse_8bit_stream_msb_first(p8["__map__"])
gfx_bottom = parse_8bit_stream_lsb_first(p8["__gfx__"][64:])
map_data = map_top +  gfx_bottom

# gff_data = parse_8bit_stream_msb_first(p8["__gff__"])

print(get_all_sprites())
print(get_all_maps())
# print(get_all_sprite_flags())
