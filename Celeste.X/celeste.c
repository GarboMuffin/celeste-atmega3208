#include "celeste.h"
#include "cpufreq.h"
#include "debug.h"
#include "display.h"
#include "joystick.h"
#include "buttons.h"
#include "bluetooth.h"
#include "generated-game-data.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Map ID for the startup menu
#define MENU_LEVEL 0

// Map to enter after leaving the main menu
#define INITIAL_LEVEL 1

// Special sprite IDs
#define SPRITE_EMPTY  0
#define SPRITE_PLAYER_NEUTRAL 1
#define SPRITE_PLAYER_SLIDING 5
#define SPRITE_PLAYER_DOWN    6
#define SPRITE_PLAYER_UP      7
#define SPRITE_SPRING_UNUSED  18
#define SPRITE_SPRING_USED    19
#define SPRITE_CRYSTAL        22
#define SPRITE_FLAG_1         118
#define SPRITE_FLAG_2         119
#define SPRITE_FLAG_3         120
#define SPRITE_0              8
#define SPRITE_1              9
#define SPRITE_2              10
#define SPRITE_3              11
#define SPRITE_4              12
#define SPRITE_5              13
#define SPRITE_6              14
#define SPRITE_7              15
#define SPRITE_8              26
#define SPRITE_9              28
#define SPRITE_PERIOD         29
#define SPRITE_COLON          30
#define SPRITE_DEATHS         31
#define SPRITE_LILY           62

// Sprite ID to flags
#define FLAG_SOLID_bm       (((uint16_t) 1) << 0)
#define FLAG_DEATH_bm       (((uint16_t) 1) << 1)
#define FLAG_BOUNCE_bm      (((uint16_t) 1) << 2)
#define FLAG_BOTTOM_SLAB_bm (((uint16_t) 1) << 3)
#define FLAG_LEFT_SLAB_bm   (((uint16_t) 1) << 4)
#define FLAG_RIGHT_SLAB_bm  (((uint16_t) 1) << 5)
#define FLAG_TOP_SLAB_bm    (((uint16_t) 1) << 6)
#define FLAG_CRYSTAL_bm     (((uint16_t) 1) << 7)
#define FLAG_CHEST_bm       (((uint16_t) 1) << 8)
#define FLAG_FLAG_bm        (((uint16_t) 1) << 9)
static const uint16_t sprite_flags[] = {
    // Rows correspond to PICO-8 editor
    // Updated manually since the tags in the original game don't really make sense
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, FLAG_DEATH_bm | FLAG_BOTTOM_SLAB_bm, FLAG_BOUNCE_bm, 0, 0, 0, FLAG_CRYSTAL_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, FLAG_DEATH_bm | FLAG_TOP_SLAB_bm, 0, 0, 0, 0,
    FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, 0, 0, FLAG_DEATH_bm | FLAG_LEFT_SLAB_bm, 0, 0, 0, 0,
    FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, 0, 0, FLAG_DEATH_bm | FLAG_RIGHT_SLAB_bm, 0, 0, 0, 0,
    FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, 0, FLAG_SOLID_bm, 0, 0, 0, 0, 0, 0, 0,
    FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    FLAG_CHEST_bm, FLAG_CHEST_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    FLAG_CHEST_bm, FLAG_CHEST_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_SOLID_bm, FLAG_FLAG_bm, FLAG_FLAG_bm, FLAG_FLAG_bm, FLAG_FLAG_bm, 0, 0, 0, 0, 0, 0,
};

// The current map. Can be modified with care.
static int current_map_index = MENU_LEVEL;
static uint8_t current_map_data[256];

// Buffer where one tile's pixel data is stored temporarily during rendering
static uint8_t dynamic_tile_buffer[64];

#define ENTITY_CLAIMED (1 << 0)
#define ENTITY_VISIBLE (1 << 1)
#define ENTITY_FLIP_X  (1 << 2)
#define MAX_ENTITIES 10
struct entity {
    uint8_t flags;
    uint8_t sprite;
    uint8_t palette[16];
    int x;
    int y;
};
static struct entity entities[MAX_ENTITIES];
static struct entity* player_entity;
static struct entity* remote_player_entity;

#define MAX_REDRAW_POSITIONS 100
struct redraw_position {
    uint8_t x;
    uint8_t y;
};
static struct redraw_position redraw_positions[MAX_REDRAW_POSITIONS];
static uint8_t redraw_positions_length;

// Player position
static float player_x;
static float player_y;

// Player velocity
static float player_vx;
static float player_vy;

#define DIR_HORIZONTAL_RIGHT 0
#define DIR_HORIZONTAL_LEFT  1
static uint8_t player_horizontal_direction;

#define DIR_VERTICAL_UP      0
#define DIR_VERTICAL_NEUTRAL 1
#define DIR_VERTICAL_DOWN    2
static uint8_t player_vertical_direction;

// Dashing state
static uint8_t player_dashing_for_ticks;
static uint8_t player_available_dashes;
static uint8_t player_max_dashes = 1;

#define BASE_HAIR_COLOR PALETTE_RED
static uint8_t hair_colors[] = {
    PALETTE_BLUE,
    PALETTE_RED,
    PALETTE_LIGHT_GREEN
};

// Wall jump state
static uint8_t player_wall_jump_for_ticks;

// If someone presses jump just before they hit the ground, we'll store that
// here so we can count it anyways.
// Amount measured in frames.
#define JUMP_BUFFER_AMOUNT 3
static uint8_t player_jump_buffer;

// 1 if player was on the ground at end of last frame which means they can jump
static uint8_t player_is_on_ground;

// 1 if player is currently sliding against a wall
static uint8_t player_is_wall_sliding;

// Spawning animation
// 0: not spawning
// 1: going up
// 2: going down
static uint8_t player_is_spawning;
static uint8_t player_spawn_x;
static uint8_t player_spawn_y;

// 1 if player was by a wall at the end of last frame
#define NO_WALL       0
#define WALL_TO_RIGHT 1
#define WALL_TO_LEFT  2
static uint8_t player_nearby_wall;

// Physics constants.
#define GRAVITY 0.24
#define TERMINAL_VELOCITY 3
#define SLIDING_TERMINAL_VELOCITY 1
#define JUMP -3.0
#define WALL_JUMP_VERTICAL -2.0
#define WALL_JUMP_HORIZONTAL 2.0
#define WALL_JUMP_VERTICAL_AFTER -0.5
#define WALL_JUMP_TIME 8
#define SPRING_BOUNCE -4.0
#define MAX_RUN 2.0
#define ACCEL 0.8
#define DECEL 0.5
#define DEADZONE 30
#define DASH_VELOCITY_STRAIGHT 3.0
#define INV_SQRT_2 0.707
#define DASH_TIME 8
#define DASH_END_VELOCITY_MULTIPLIER 0.4

#define MAX_NUM_CONSUMED_SPRINGS 10
#define SPRING_ANIMATION_TIME 15
struct consumed_spring {
    uint8_t map_x;
    uint8_t map_y;
    uint8_t frames;
};
static struct consumed_spring consumed_springs[MAX_NUM_CONSUMED_SPRINGS];

#define MAX_NUM_CONSUMED_CRYSTALS 10
#define CRYSTAL_REFRESH_TIME 60
struct consumed_crystal {
    uint8_t map_x;
    uint8_t map_y;
    uint8_t frames;
};
static struct consumed_crystal consumed_crystals[MAX_NUM_CONSUMED_CRYSTALS];

// 1 if flag sprite exists
static uint8_t flag_exists;
static uint8_t flag_x; // map x
static uint8_t flag_y; // map y
static uint8_t flag_sprites[] = {
    SPRITE_FLAG_1,
    SPRITE_FLAG_2,
    SPRITE_FLAG_3
};

// 1 if the player won :)
static uint8_t player_won;

// # of ticks the player has been playing for
static uint32_t timer;

// # of deaths
static uint32_t player_deaths;

// Bluetooth multiplayer protocol
struct peer_status {
    // 12345678
    //       ~~ # of dashes available
    //    ~~~   sprite ID
    //   ~      horizontal direction
    uint8_t appearance;

    // map ID
    uint8_t map;

    // screen x
    uint8_t x;

    // screen y
    uint8_t y;
};
static struct peer_status my_peer_status;
static struct peer_status remote_peer_status;

void celeste_init(void) {
    player_entity = celeste_entity_claim();
    remote_player_entity = celeste_entity_claim();
    celeste_load_map(current_map_index);
    bluetooth_set_message_size(sizeof(struct peer_status));
}

void celeste_panic(const char* message) {
    while (1) {
        debug_write_string("\r\nPANIC: ");
        debug_write_string(message);
    }
}

void celeste_load_map(uint8_t new_map_index) {
    celeste_reset_dynamic_map_elements();
    current_map_index = new_map_index;
    flag_exists = 0;

    for (int i = 0; i < 256; i++) {
        uint8_t sprite_id = pgm_read_byte(progmem_stored_maps + new_map_index * 256 + i);

        // Madeline sprite is where the player goes. Gets replaced with empty.
        if (sprite_id == SPRITE_PLAYER_NEUTRAL) {
            player_spawn_x = (i % 16) * 8;
            player_spawn_y = (i / 16) * 8;
            sprite_id = SPRITE_EMPTY;
        }

        if (sprite_id == SPRITE_FLAG_1) {
            flag_exists = 1;
            flag_x = i % 16;
            flag_y = i / 16;
        }

        current_map_data[i] = sprite_id;
    }

    celeste_start_spawn_animation();
    celeste_draw_everything();
}

void celeste_reset_dynamic_map_elements() {
    celeste_reset_consumed_springs();
    celeste_reset_consumed_crystals();
}

void celeste_draw_everything(void) {
    for (uint8_t map_x = 0; map_x < 16; map_x++) {
        for (uint8_t map_y = 0; map_y < 16; map_y++) {
            uint8_t sprite_id = current_map_data[map_y * 16 + map_x];
            const uint8_t* progmem_sprite = progmem_sprite_data + sprite_id * 32;
            display_draw_packed_sprite(map_x * 8, map_y * 8, progmem_sprite);
        }
    }
}

void celeste_draw_tile(uint8_t map_x, uint8_t map_y) {
    uint8_t* buffer_ptr = dynamic_tile_buffer;
    uint8_t sprite_id = current_map_data[map_y * 16 + map_x];
    const uint8_t* progmem_tile = progmem_sprite_data + sprite_id * 32;

    for (uint8_t pixel_y = 0; pixel_y < 8; pixel_y++) {
        for (uint8_t pixel_x = 0; pixel_x < 8; pixel_x += 2) {
            uint8_t packed = pgm_read_byte(progmem_tile);
            progmem_tile++;

            *buffer_ptr = packed >> 4;
            buffer_ptr++;

            *buffer_ptr = packed & 0xF;
            buffer_ptr++;
        }
    }

    for (int i = 0; i < MAX_ENTITIES; i++) {
        if ((entities[i].flags & (ENTITY_CLAIMED | ENTITY_VISIBLE)) != (ENTITY_CLAIMED | ENTITY_VISIBLE)) {
            continue;
        }

        int my_screen_left = map_x * 8;
        int my_screen_right = my_screen_left + 8;
        int my_screen_top = map_y * 8;
        int my_screen_bottom = my_screen_top + 8;

        int sprite_screen_left = entities[i].x;
        int sprite_screen_right = sprite_screen_left + 8;
        int sprite_screen_top = entities[i].y;
        int sprite_screen_bottom = sprite_screen_top + 8;

        int draw_screen_left = MAX(sprite_screen_left, my_screen_left);
        int draw_screen_right = MIN(sprite_screen_right, my_screen_right);
        int draw_screen_top = MAX(sprite_screen_top, my_screen_top);
        int draw_screen_bottom = MIN(sprite_screen_bottom, my_screen_bottom);

        const uint8_t* progmem_sprite = progmem_sprite_data + entities[i].sprite * 32;

        for (int screen_y = draw_screen_top; screen_y < draw_screen_bottom; screen_y++) {
            int tile_y = screen_y - my_screen_top;
            int sprite_y = screen_y - sprite_screen_top;

            for (int screen_x = draw_screen_left; screen_x < draw_screen_right; screen_x++) {
                int tile_x = screen_x - my_screen_left;
                int sprite_x = screen_x - sprite_screen_left;

                if (entities[i].flags & ENTITY_FLIP_X) {
                    sprite_x = 7 - sprite_x;
                }

                uint8_t color = pgm_read_byte(progmem_sprite + sprite_y * 4 + (sprite_x / 2));
                if (sprite_x % 2 == 0) {
                    // Even, take most significant 4 bits
                    color = color >> 4;
                } else {
                    color = color & 0xF;
                }

                if (color != PALETTE_TRANSPARENT) {
                    dynamic_tile_buffer[tile_y * 8 + tile_x] = entities[i].palette[color];
                }
            }
        }
    }

    display_draw_pixels(map_x * 8, map_y * 8, 8, 8, dynamic_tile_buffer);
}

struct entity* celeste_entity_claim() {
    for (uint8_t i = MAX_ENTITIES - 1; i >= 0; i--) {
        if ((entities[i].flags & ENTITY_CLAIMED) == 0) {
            // Mark as claimed, clear whatever leftover data was in there
            entities[i].flags = ENTITY_CLAIMED;
            entities[i].sprite = 0;
            entities[i].x = 0;
            entities[i].y = 0;
            for (uint8_t j = 0; j < 16; j++) {
                entities[i].palette[j] = j;
            }
            return &entities[i];
        }
    }

    // This should never happen
    celeste_panic("Out of entity slots");
    return 0; // NULL
}

void celeste_mark_tile_for_redraw(uint8_t x, uint8_t y) {
    // Having a naive O(n^2) duplicate tile check saves CPU cycles in the end
    // from drawing fewer tiles unnecessarily
    for (uint8_t i = 0; i < redraw_positions_length; i++) {
        if (redraw_positions[i].x == x && redraw_positions[i].y == y) {
            return;
        }
    }

    if (redraw_positions_length >= MAX_REDRAW_POSITIONS) {
        celeste_panic("Out of redraw positions");
    }

    redraw_positions[redraw_positions_length].x = x;
    redraw_positions[redraw_positions_length].y = y;
    redraw_positions_length++;
}

void celeste_start_spawn_animation(void) {
    celeste_reset_dynamic_map_elements();
    player_is_spawning = 1;
    player_x = player_spawn_x;
    player_y = 8 * 16;
    player_horizontal_direction = DIR_HORIZONTAL_RIGHT;
    player_vertical_direction = DIR_VERTICAL_NEUTRAL;
    player_vx = 0;
    player_vy = JUMP * 0.5;
    player_dashing_for_ticks = 0;
    player_available_dashes = player_max_dashes;
    player_jump_buffer = 0;
    player_is_on_ground = 1;
    player_nearby_wall = NO_WALL;
}

void celeste_tick_spawn_animation(void) {
    player_y += player_vy;
    if (player_is_spawning == 1) {
        if (player_y <= player_spawn_y) {
            player_is_spawning = 2;
        }
    } else {
        player_vy += GRAVITY;
        if (player_y >= player_spawn_y) {
            player_y = player_spawn_y;
            player_vy = 0;
            player_is_spawning = 0;
        }
    }
}

uint16_t celeste_get_sprite_flags(int screen_x, int screen_y) {
    if (screen_x >= 128 || screen_x < 0 || screen_y >= 128 || screen_y < 0) {
        return 0;
    }
    uint8_t map_x = screen_x / 8;
    uint8_t map_y = screen_y / 8;
    uint8_t sprite_id = current_map_data[map_y * 16 + map_x];
    uint16_t flags = sprite_flags[sprite_id];

    // Slab shaped tiles: pretend to be empty if we not colliding
    if (flags & FLAG_BOTTOM_SLAB_bm) {
        if (screen_y % 8 < 5) {
            return 0;
        }
    } else if (flags & FLAG_TOP_SLAB_bm) {
        if (screen_y % 8 > 3) {
            return 0;
        }
    } else if (flags & FLAG_LEFT_SLAB_bm) {
        if (screen_x % 8 > 3) {
            return 0;
        }
    } else if (flags & FLAG_RIGHT_SLAB_bm) {
        if (screen_x % 8 < 5) {
            return 0;
        }
    }

    return flags;
}

void celeste_consume_spring(int screen_x, int screen_y) {
    player_vy = SPRING_BOUNCE;
    player_available_dashes = player_max_dashes;

    uint8_t map_x = screen_x / 8;
    uint8_t map_y = screen_y / 8;
    current_map_data[map_y * 16 + map_x] = SPRITE_SPRING_USED;
    celeste_mark_tile_for_redraw(map_x, map_y);

    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_SPRINGS; i++) {
        if (consumed_springs[i].frames == 0) {
            // Unused slot, let's take it.
            consumed_springs[i].frames = SPRING_ANIMATION_TIME;
            consumed_springs[i].map_x = map_x;
            consumed_springs[i].map_y = map_y;
            break;
        }
    }
}

void celeste_update_consumed_springs(void) {
    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_SPRINGS; i++) {
        if (consumed_springs[i].frames > 0) {
            consumed_springs[i].frames--;
            if (consumed_springs[i].frames == 0) {
                uint8_t map_x = consumed_springs[i].map_x;
                uint8_t map_y = consumed_springs[i].map_y;
                current_map_data[map_y * 16 + map_x] = SPRITE_SPRING_UNUSED;
                celeste_mark_tile_for_redraw(map_x, map_y);
            }
        }
    }
}

void celeste_reset_consumed_springs(void) {
    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_SPRINGS; i++) {
        if (consumed_springs[i].frames > 0) {
            consumed_springs[i].frames = 0;
            uint8_t map_x = consumed_springs[i].map_x;
            uint8_t map_y = consumed_springs[i].map_y;
            current_map_data[map_y * 16 + map_x] = SPRITE_SPRING_UNUSED;
            celeste_mark_tile_for_redraw(map_x, map_y);
        }
    }
}

void celeste_consume_crystal(int screen_x, int screen_y) {
    player_available_dashes = player_max_dashes;

    uint8_t map_x = screen_x / 8;
    uint8_t map_y = screen_y / 8;
    current_map_data[map_y * 16 + map_x] = SPRITE_EMPTY;
    celeste_mark_tile_for_redraw(map_x, map_y);

    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_CRYSTALS; i++) {
        if (consumed_crystals[i].frames == 0) {
            // Unused slot, let's take it.
            consumed_crystals[i].frames = CRYSTAL_REFRESH_TIME;
            consumed_crystals[i].map_x = map_x;
            consumed_crystals[i].map_y = map_y;
            break;
        }
    }
}

void celeste_update_consumed_crystals(void) {
    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_CRYSTALS; i++) {
        if (consumed_crystals[i].frames > 0) {
            consumed_crystals[i].frames--;
            if (consumed_crystals[i].frames == 0) {
                uint8_t map_x = consumed_crystals[i].map_x;
                uint8_t map_y = consumed_crystals[i].map_y;
                current_map_data[map_y * 16 + map_x] = SPRITE_CRYSTAL;
                celeste_mark_tile_for_redraw(map_x, map_y);
            }
        }
    }
}

void celeste_reset_consumed_crystals(void) {
    for (uint8_t i = 0; i < MAX_NUM_CONSUMED_CRYSTALS; i++) {
        if (consumed_crystals[i].frames > 0) {
            consumed_crystals[i].frames = 0;
            uint8_t map_x = consumed_crystals[i].map_x;
            uint8_t map_y = consumed_crystals[i].map_y;
            current_map_data[map_y * 16 + map_x] = SPRITE_CRYSTAL;
            celeste_mark_tile_for_redraw(map_x, map_y);
        }
    }
}

void celeste_update_flag(void) {
    if (flag_exists) {
        current_map_data[flag_y * 16 + flag_x] = flag_sprites[(timer / 5) % sizeof(flag_sprites)];
        celeste_mark_tile_for_redraw(flag_x, flag_y);
    }
}

void celeste_draw_text(uint8_t map_x, uint8_t map_y, const char* text) {
    // Center text
    map_x -= strlen(text) / 2;

    while (*text != '\0') {
        uint8_t tile;
        switch (*text) {
            case '0':
                tile = SPRITE_0;
                break;
            case '1':
                tile = SPRITE_1;
                break;
            case '2':
                tile = SPRITE_2;
                break;
            case '3':
                tile = SPRITE_3;
                break;
            case '4':
                tile = SPRITE_4;
                break;
            case '5':
                tile = SPRITE_5;
                break;
            case '6':
                tile = SPRITE_6;
                break;
            case '7':
                tile = SPRITE_7;
                break;
            case '8':
                tile = SPRITE_8;
                break;
            case '9':
                tile = SPRITE_9;
                break;
            case ':':
                tile = SPRITE_COLON;
                break;
            case '.':
                tile = SPRITE_PERIOD;
                break;
            case 'D':
                tile = SPRITE_DEATHS;
                break;
            default:
                // Fallback to something that will be obviously wrong
                tile = SPRITE_LILY;
                break;
        }
        current_map_data[map_y * 16 + map_x] = tile;
        celeste_mark_tile_for_redraw(map_x, map_y);

        map_x++;
        text++;
    }
}

void celeste_win(void) {
    player_won = 1;

    float total_seconds = timer / 30.0;
    uint8_t minutes = total_seconds / 60;
    uint8_t seconds = ((uint32_t) total_seconds) % 60;
    uint8_t centiseconds = (total_seconds - ((uint32_t) total_seconds)) * 100;

    // a reasonable limit
    // ensures that the timer is always centered
    if (minutes > 99) {
        minutes = 99;
        seconds = 59;
        centiseconds = 99;
    }

    // buf is big enough to fit any text that would even fit on one map line
    // and the following null terminator
    char buf[17];

    sprintf(buf, "%02d:%02d.%02d", minutes, seconds, centiseconds);
    celeste_draw_text(8, 0, buf);

    // This looks way better if it is always centered so we'll prepend another death
    // symbol if needed
    sprintf(buf, "%luD", player_deaths);
    if (strlen(buf) % 2 != 0) {
        sprintf(buf, "D%luD", player_deaths);
    }
    celeste_draw_text(8, 2, buf);
}

void celeste_tick_menu(void) {
    if (buttons_dash() || buttons_jump()) {
        celeste_load_map(INITIAL_LEVEL);
    }
}

void celeste_tick_game(void) {
    // ADC reading involves waiting so we try to do a few other operations
    // in the meantime.
    joystick_start_horizontal();
    int press_dash = buttons_dash();
    int horizontal = ((int) joystick_finish_reading()) - 127;
    joystick_start_vertical();
    int press_jump = buttons_jump();
    int vertical = ((int) joystick_finish_reading()) - 127;

    if (player_jump_buffer > 0) {
        player_jump_buffer--;
    }
    if (press_jump) {
        // Store the jump for later
        player_jump_buffer = JUMP_BUFFER_AMOUNT;
    }

    if (player_dashing_for_ticks > 0) {
        // Wall jumping: constant motion, ignore friction, ignore gravity
        player_dashing_for_ticks--;

        if (player_dashing_for_ticks == 0) {
            player_vy *= DASH_END_VELOCITY_MULTIPLIER;
            player_vx *= DASH_END_VELOCITY_MULTIPLIER;
        }
    } else if (player_wall_jump_for_ticks > 0) {
        // Wall jumping: constant motion, ignore friction, ignore gravity
        player_wall_jump_for_ticks--;

        if (player_wall_jump_for_ticks == 0) {
            // Just ended, stop going up
            player_vy = WALL_JUMP_VERTICAL_AFTER;
        }

        // But can still dash to cancel early
        if (press_dash && player_available_dashes > 0) {
            player_wall_jump_for_ticks = 0;
            player_available_dashes--;
            player_dashing_for_ticks = DASH_TIME;

            if (horizontal > DEADZONE && vertical > DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal > DEADZONE && vertical < -DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal < -DEADZONE && vertical < -DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal < -DEADZONE && vertical > DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal > DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else if (horizontal < -DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else if (vertical > DEADZONE) {
                player_vx = 0;
                player_vy = -DASH_VELOCITY_STRAIGHT;
            } else if (vertical < -DEADZONE) {
                player_vx = 0;
                player_vy = DASH_VELOCITY_STRAIGHT;
            } else if (player_horizontal_direction == DIR_HORIZONTAL_RIGHT) {
                player_vx = DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else {
                // player_Direction is left
                player_vx = -DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            }
        }
    } else {
        // Normal movement

        // Facing direction
        if (horizontal > DEADZONE) {
            player_horizontal_direction = DIR_HORIZONTAL_LEFT;
        } else if (horizontal < -DEADZONE) {
            player_horizontal_direction = DIR_HORIZONTAL_RIGHT;
        }
        if (vertical > DEADZONE) {
            player_vertical_direction = DIR_VERTICAL_UP;
        } else if (vertical < -DEADZONE) {
            player_vertical_direction = DIR_VERTICAL_DOWN;
        } else {
            player_vertical_direction = DIR_VERTICAL_NEUTRAL;
        }

        // Dashing
        if (press_dash && player_available_dashes > 0) {
            player_available_dashes--;
            player_dashing_for_ticks = DASH_TIME;

            if (horizontal > DEADZONE && vertical > DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal > DEADZONE && vertical < -DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal < -DEADZONE && vertical < -DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal < -DEADZONE && vertical > DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
                player_vy = -DASH_VELOCITY_STRAIGHT * INV_SQRT_2;
            } else if (horizontal > DEADZONE) {
                player_vx = -DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else if (horizontal < -DEADZONE) {
                player_vx = DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else if (vertical > DEADZONE) {
                player_vx = 0;
                player_vy = -DASH_VELOCITY_STRAIGHT;
            } else if (vertical < -DEADZONE) {
                player_vx = 0;
                player_vy = DASH_VELOCITY_STRAIGHT;
            } else if (player_horizontal_direction == DIR_HORIZONTAL_RIGHT) {
                player_vx = DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            } else {
                // player_Direction is left
                player_vx = -DASH_VELOCITY_STRAIGHT;
                player_vy = 0;
            }
        } else {
            // Horizontal joystick movement
            if (horizontal > DEADZONE) {
                player_vx -= ACCEL;
                if (player_vx < -MAX_RUN) {
                    player_vx = -MAX_RUN;
                }
            } else if (horizontal < -DEADZONE) {
                player_vx += ACCEL;
                if (player_vx > MAX_RUN) {
                    player_vx = MAX_RUN;
                }
            } else if (player_vx > 0) {
                player_vx -= DECEL;
                if (player_vx < 0.0) {
                    player_vx = 0;
                }
            } else {
                player_vx += DECEL;
                if (player_vx > 0.0) {
                    player_vx = 0;
                }
            }

            // Jumping
            if ((player_is_on_ground || (player_nearby_wall != NO_WALL)) && player_jump_buffer) {
                if (player_is_on_ground) {
                    // on ground: always a regular jump
                    player_vy = JUMP;
                } else if (player_nearby_wall == WALL_TO_LEFT) {
                    player_vy = WALL_JUMP_VERTICAL;
                    player_vx = WALL_JUMP_HORIZONTAL;
                    player_horizontal_direction = DIR_HORIZONTAL_RIGHT;
                    player_wall_jump_for_ticks = WALL_JUMP_TIME;
                } else if (player_nearby_wall == WALL_TO_RIGHT) {
                    player_vy = WALL_JUMP_VERTICAL;
                    player_vx = -WALL_JUMP_HORIZONTAL;
                    player_horizontal_direction = DIR_HORIZONTAL_LEFT;
                    player_wall_jump_for_ticks = WALL_JUMP_TIME;
                }

                // Clear buffer so we don't try to jump again right after
                player_jump_buffer = 0;
            } else {
                player_vy += GRAVITY;
            }
        }
    }

    player_is_on_ground = 0;
    player_is_wall_sliding = 0;

    uint16_t combined_flags = 0;
    uint16_t combined_combined_flags = 0;
    uint16_t flags = 0;

    player_x += player_vx;
    if (player_x < 0.0) {
        player_x = 0.0;
    } else if (player_x > 120.0) {
        player_x = 120.0;
    }

    if (player_vx > 0.0) {
        player_nearby_wall = NO_WALL;
        flags = celeste_get_sprite_flags(player_x + 8.0, player_y + 1.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_x = (((uint8_t) player_x) / 8) * 8.0;
            player_vx = 0.0;
            player_nearby_wall = WALL_TO_RIGHT;
            player_is_wall_sliding = 1;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x + 8.0, player_y + 1.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x + 8.0, player_y + 1.0);
        }

        flags = celeste_get_sprite_flags(player_x + 8.0, player_y + 7.9);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_x = (((uint8_t) player_x) / 8) * 8.0;
            player_vx = 0.0;
            player_nearby_wall = WALL_TO_RIGHT;
            player_is_wall_sliding = 1;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x + 8.0, player_y + 7.9);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x + 8.0, player_y + 7.9);
        }
    } else if (player_vx < 0.0) {
        player_nearby_wall = NO_WALL;

        flags = celeste_get_sprite_flags(player_x, player_y + 1.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_x = (((uint8_t) player_x) / 8 + 1) * 8.0;
            player_vx = 0.0;
            player_nearby_wall = WALL_TO_LEFT;
            player_is_wall_sliding = 1;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x, player_y + 1.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x, player_y + 1.0);
        }

        flags = celeste_get_sprite_flags(player_x, player_y + 7.9);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_x = (((uint8_t) player_x) / 8 + 1) * 8.0;
            player_vx = 0.0;
            player_nearby_wall = WALL_TO_LEFT;
            player_is_wall_sliding = 1;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x, player_y + 7.9);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x, player_y + 7.9);
        }
    }

    combined_combined_flags |= combined_flags;

    float terminal_velocity = player_is_wall_sliding ? SLIDING_TERMINAL_VELOCITY : TERMINAL_VELOCITY;
    if (player_vy > terminal_velocity) {
        player_vy = terminal_velocity;
    }

    if ((combined_flags & FLAG_DEATH_bm) && ((combined_flags & FLAG_SOLID_bm) == 0)) {
        player_deaths++;
        celeste_start_spawn_animation();
        return;
    }

    combined_flags = 0;
    player_y += player_vy;

    if (player_vy > 0.0) {
        flags = celeste_get_sprite_flags(player_x, player_y + 8.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_y = (((uint8_t) player_y) / 8) * 8.0;
            player_vy = 0.0;
            player_is_on_ground = 1;
            player_available_dashes = player_max_dashes;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x, player_y + 8.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x, player_y + 8.0);
        }

        flags = celeste_get_sprite_flags(player_x + 7.9, player_y + 8.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_y = (((uint8_t) player_y) / 8) * 8.0;
            player_vy = 0.0;
            player_is_on_ground = 1;
            player_available_dashes = player_max_dashes;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x + 7.9, player_y + 8.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x + 7.9, player_y + 8.0);
        }
    } else {
        flags = celeste_get_sprite_flags(player_x, player_y + 1.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_y = (((uint8_t) player_y) / 8 + 1) * 8.0 - 1.0;
            player_vy = 0.0;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x, player_y + 1.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x, player_y + 1.0);
        }

        flags = celeste_get_sprite_flags(player_x + 7.9, player_y + 1.0);
        combined_flags |= flags;
        if (flags & FLAG_SOLID_bm) {
            player_y = (((uint8_t) player_y) / 8 + 1) * 8.0 - 1.0;
            player_vy = 0.0;
        }
        if (flags & FLAG_BOUNCE_bm) {
            celeste_consume_spring(player_x + 7.9, player_y + 1.0);
        }
        if (flags & FLAG_CRYSTAL_bm)  {
            celeste_consume_crystal(player_x + 7.9, player_y + 1.0);
        }
    }

    combined_combined_flags |= combined_flags;

    if (player_y <= 0) {
        if (flag_exists) {
            // No more levels to go to, just stop moving offscreen.
            player_y = 0;
        } else {
            celeste_load_map(current_map_index + 1);
        }
    } else if (player_y >= 120) {
        player_deaths++;
        celeste_start_spawn_animation();
        return;
    }

    if ((combined_flags & FLAG_DEATH_bm) && ((combined_flags & FLAG_SOLID_bm) == 0)) {
        player_deaths++;
        celeste_start_spawn_animation();
        return;
    }

    if (player_max_dashes != 2 && (combined_combined_flags & FLAG_CHEST_bm)) {
        player_max_dashes = 2;
        if (player_is_on_ground) {
            player_available_dashes = player_max_dashes;
        }
    }

    if (!player_won && (combined_combined_flags & FLAG_FLAG_bm)) {
        // !!!!
        celeste_win();
    }
}

void celeste_tick(void) {
    redraw_positions_length = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if ((entities[i].flags & (ENTITY_CLAIMED | ENTITY_VISIBLE)) == (ENTITY_CLAIMED | ENTITY_VISIBLE)) {
            uint8_t map_x = entities[i].x / 8;
            uint8_t map_y = entities[i].y / 8;
            celeste_mark_tile_for_redraw(map_x, map_y);
            celeste_mark_tile_for_redraw(map_x + 1, map_y);
            celeste_mark_tile_for_redraw(map_x, map_y + 1);
            celeste_mark_tile_for_redraw(map_x + 1, map_y + 1);
        }
    }

    if (current_map_index == 0) {
        celeste_tick_menu();
    } else {
        timer++;

        celeste_update_consumed_springs();
        celeste_update_consumed_crystals();
        celeste_update_flag();

        if (player_is_spawning) {
            celeste_tick_spawn_animation();
        } else {
            celeste_tick_game();
        }

        player_entity->x = player_x;
        player_entity->y = player_y;
        player_entity->flags |= ENTITY_VISIBLE;
        if (player_horizontal_direction == DIR_HORIZONTAL_LEFT) {
            player_entity->flags |= ENTITY_FLIP_X;
        } else {
            player_entity->flags &= ~ENTITY_FLIP_X;
        }
        if (player_is_on_ground) {
            if (player_vertical_direction == DIR_VERTICAL_UP) {
                player_entity->sprite = SPRITE_PLAYER_UP;
            } else if (player_vertical_direction == DIR_VERTICAL_NEUTRAL) {
                player_entity->sprite = SPRITE_PLAYER_NEUTRAL;
            } else if (player_vertical_direction == DIR_VERTICAL_DOWN) {
                player_entity->sprite = SPRITE_PLAYER_DOWN;
            }
        } else if (player_is_wall_sliding) {
            player_entity->sprite = SPRITE_PLAYER_SLIDING;
        } else {
            player_entity->sprite = SPRITE_PLAYER_NEUTRAL;
        }
        player_entity->palette[BASE_HAIR_COLOR] = hair_colors[player_available_dashes];

        if (bluetooth_has_pending_message()) {
            bluetooth_copy_pending_message(&remote_peer_status);
        }

        if (bluetooth_is_exchanging_ok() && remote_peer_status.map == current_map_index) {
            remote_player_entity->x = remote_peer_status.x;
            remote_player_entity->y = remote_peer_status.y;
            remote_player_entity->sprite = (remote_peer_status.appearance >> 2) & 0b111;
            if (((remote_peer_status.appearance >> 5) & 0b1) == DIR_HORIZONTAL_LEFT) {
                remote_player_entity->flags |= ENTITY_FLIP_X;
            } else {
                remote_player_entity->flags &= ~ENTITY_FLIP_X;
            }
            remote_player_entity->flags |= ENTITY_VISIBLE;
            uint8_t remote_dashes = remote_peer_status.appearance & 0b11;
            remote_player_entity->palette[BASE_HAIR_COLOR] = hair_colors[remote_dashes];
        } else {
            remote_player_entity->flags &= ~ENTITY_VISIBLE;
        }

        if (bluetooth_can_send_message()) {
            my_peer_status.appearance = (
                    ((player_available_dashes << 0) & 0b00000011) |
                    ((player_entity->sprite << 2) & 0b00011100) |
                    ((player_horizontal_direction << 5) & 0b00100000)
            );
            my_peer_status.map = current_map_index;
            my_peer_status.x = player_x;
            my_peer_status.y = player_y;
            bluetooth_send_message((void*) &my_peer_status);
        }
    }

    for (int i = 0; i < MAX_ENTITIES; i++) {
        if ((entities[i].flags & (ENTITY_CLAIMED | ENTITY_VISIBLE)) == (ENTITY_CLAIMED | ENTITY_VISIBLE)) {
            uint8_t map_x = entities[i].x / 8;
            uint8_t map_y = entities[i].y / 8;
            celeste_mark_tile_for_redraw(map_x, map_y);
            celeste_mark_tile_for_redraw(map_x + 1, map_y);
            celeste_mark_tile_for_redraw(map_x, map_y + 1);
            celeste_mark_tile_for_redraw(map_x + 1, map_y + 1);
        }
    }

    for (int i = 0; i < redraw_positions_length; i++) {
        celeste_draw_tile(redraw_positions[i].x, redraw_positions[i].y);
    }
}

void celeste_test_map(void) {
    while (1) {
        for (int i = 0; i < sizeof(progmem_stored_maps) / 256; i++) {
            celeste_load_map(i);
            _delay_ms(1000);
        }
    }
}
