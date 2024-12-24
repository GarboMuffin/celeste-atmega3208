#include <stdint.h>

#ifndef CELESTE_H
#define	CELESTE_H

struct entity;

// Initialize celeste to default menu
void celeste_init(void);

// Call when an unrecoverable error happened
void celeste_panic(const char* message);

// Set new map
void celeste_load_map(uint8_t new_map_index);

// Reset dynamic parts of the map that is already loaded
void celeste_reset_dynamic_map_elements(void);

// Draws everything at once. Slow, use sparingly
void celeste_draw_everything(void);

// Draws a specific map tile and the entities in that tile
void celeste_draw_tile(uint8_t map_x, uint8_t map_y);

// Ran each frame while in main menu
void celeste_main_menu(void);

// Claims unused entity slot.
// The entity is invisible by default.
struct entity* celeste_entity_claim(void);

// Mark a tile on the map to be redrawn, including any entities on it
void celeste_mark_tile_for_redraw(uint8_t x, uint8_t y);

// Begins playing the spawn animation
void celeste_start_spawn_animation(void);

// Run each frame while the spawn animation is happening
void celeste_tick_spawn_animation(void);

// Activates a spring
void celeste_consume_spring(int screen_x, int screen_y);

// Check if any consumed springs should go back to normal
void celeste_update_consumed_springs(void);

// Unconsumes all springs.
void celeste_reset_consumed_springs(void);

// Activates a dash crystal
void celeste_consume_crystal(int screen_x, int screen_y);

// Check if any hidden crystals should come back.
void celeste_update_consumed_crystals(void);

// Restores all crystals.
void celeste_reset_consumed_crystals(void);

// Convert screen position to the flags of the map tile at that location
uint16_t celeste_get_sprite_flags(int screen_x, int screen_y);

// Update flag sprite, if it exists.
void celeste_update_flag(void);

// Draw text on the map
// Will be centered around given map tile
// No wrapping
void celeste_draw_text(uint8_t map_x, uint8_t map_y, const char* text);

// Win the game.
void celeste_win(void);

// Run each frame while the game is running
void celeste_tick_game(void);

// Run each frame
void celeste_tick(void);

// Map rendering test mode
void celeste_test_map(void);

#endif
