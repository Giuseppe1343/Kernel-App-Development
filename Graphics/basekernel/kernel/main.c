/*
Copyright (C) 2015-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.
*/

#include "console.h"
#include "page.h"
#include "process.h"
#include "keyboard.h"
#include "mouse.h"
#include "interrupt.h"
#include "clock.h"
#include "ata.h"
#include "device.h"
#include "cdromfs.h"
#include "string.h"
#include "graphics.h"
#include "kernel/ascii.h"
#include "kernel/syscall.h"
#include "rtc.h"
#include "kernelcore.h"
#include "kmalloc.h"
#include "memorylayout.h"
#include "kshell.h"
#include "cdromfs.h"
#include "diskfs.h"
#include "serial.h"
/*
This is the C initialization point of the kernel.
By the time we reach this point, we are in protected mode,
with interrupts disabled, a valid C stack, but no malloc heap.
Now we initialize each subsystem in the proper order:
*/

const struct graphics_color color_array[] = { // The part of the Ansi escape sequence that changes for color is \033[.;..m
	{0, 0, 0},
	{0, 0, 0xAA},
	{0, 0xAA, 0},
	{0, 0xAA, 0xAA},
	{0xAA, 0, 0},
	{0xAA, 0, 0xAA},
	{0xAA, 0x55, 0},
	{0xCC, 0xCC, 0xCC},
	{0x55, 0x55, 0x55},
	{0, 0, 0xFF},
	{0, 0xFF, 0},
	{0, 0xFF, 0xFF},
	{0xFF, 0, 0},
	{0xFF, 0, 0xFF},
	{0xFF, 0xFF, 0},
	{0xFF, 0xFF, 0xFF}};

#define BLACK 0
#define BLUE 1
#define GREEN 2
#define CYAN 3
#define RED 4
#define MAGENTA 5
#define BROWN 6
#define LIGHT_GRAY 7
#define DARK_GRAY 8
#define LIGHT_BLUE 9
#define LIGHT_GREEN 10
#define LIGHT_CYAN 11
#define LIGHT_RED 12
#define LIGHT_MAGENTA 13
#define YELLOW 14
#define WHITE 15

typedef enum
{
	false,
	true
} mbool; // define bool as enum to increase readability

#define TEXT_PIXEL_WIDTH 8
#define TEXT_PIXEL_HEIGHT 7

struct Space_Ship
{
	int x;
	int y;
};

struct Bullet
{
	mbool active;
	int x;
	int y;
};

#define NUMBER_OF_ASTEROIDS 3
struct Asteroid
{
	mbool active;
	int x;
	int y;
	mbool explode;
	int phase;
};
struct Space_Ship space_ship;
struct Bullet bullet;
struct Asteroid asteroids[NUMBER_OF_ASTEROIDS];

char current_key = 0;
mbool key_pressed = false;
mbool game_start = false;
mbool game_over = false;
mbool game_restart = false;
mbool game_pause = false;
int life = 3;
int score = 0;
char Score[3];

void init_space_ship() // Function that places space_ship in the bottom middle
{
	space_ship.x = (video_xres - 71) / 2; // pos = middle bottom
	space_ship.y = video_yres - 30;
}

void init_asteroids() // Function that clears and initializes the asteroids[]
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		asteroids[i].active = false; // Disable all asteroids so they can be (re)spawn
	}
}

void kprint_at(int x, int y, const char *str, int color)
{
	graphics_fgcolor(&graphics_root, color_array[color]);

	while (*str != '\0') // For each character
	{
		graphics_char(&graphics_root, x, y, *str++);
		x += TEXT_PIXEL_WIDTH;
	}
	// graphics_char(&graphics_root,video_xres / 2, video_yres / 2,'');
}

void clear_screen()
{
	graphics_clear(&graphics_root, 0, 0, graphics_width(&graphics_root), graphics_height(&graphics_root));
}

void game_init() // Function that initializes and resets all game variables
{
	init_space_ship();
	init_asteroids();
	bullet.active = false;
	clear_screen();
	score = 0;
	life = 3;
}

void print_score() // Function that writes the score in the upper right corner of the screen
{

	int temp = score;
	for (int i = 2; i > -1; i--)
	{
		Score[i] = (char)temp % 10 + '0';
		temp /= 10;
	}
	kprint_at(video_xres - 10 * TEXT_PIXEL_WIDTH, 8, "Score:", LIGHT_CYAN);
	kprint_at(video_xres - 4 * TEXT_PIXEL_WIDTH, 8, Score, WHITE);
}

void print_life() // Function that writes the life in the upper left corner of the screen
{
	char life_char[2] = {(char)life + '0', '\0'};
	kprint_at(8, 8, "Life:", LIGHT_GREEN);
	kprint_at(8 + 5 * TEXT_PIXEL_WIDTH, 8, life_char, WHITE);
}

void move_and_fire_space_ship() // Function that takes keyboard inputs and determines the movements of the spaceship and the starting point of the bullet according to these inputs
{
	if (key_pressed)
	{
		switch (current_key) // If the same or a different key is pressed again
		{
		// case 'w':
		// 	space_ship.y--;
		// 	break;
		// case 's':
		// 	space_ship.y++;
		// 	break;
		case 'a':
			if (space_ship.x > 5)
				space_ship.x -= 5;
			break;
		case 'd':
			if (space_ship.x < video_xres - 75)
				space_ship.x += 5;
			break;
		case ' ':
			if (!bullet.active) // If a bullet has been fired, a new bullet cannot be fired until that bullet hits something.
			{
				bullet.y = space_ship.y + 10;
				bullet.x = space_ship.x + 34;
				bullet.active = true;
			}
			break;
		case 'r':
			game_restart = true;
			break;
		case 'p':
			game_pause = true;
			break;
		}
		key_pressed = false; // After the key is processed, the flag will be set to false so that it does not enter the same place again.
	}
}

void draw_space_ship(mbool draw) // Function that draws and deletes the space_ship
{
	if (draw) // Draw space_ship if true
	{
		graphics_rect(&graphics_root, space_ship.x + 20, space_ship.y + 15, 30, 5, color_array[DARK_GRAY]);
		graphics_tri(&graphics_root, space_ship.x, space_ship.y + 30, space_ship.x + 20, space_ship.y + 25, space_ship.x + 30, space_ship.y + 2, color_array[WHITE]);
		graphics_tri(&graphics_root, space_ship.x + 71, space_ship.y + 30, space_ship.x + 51, space_ship.y + 25, space_ship.x + 41, space_ship.y + 2, color_array[WHITE]);
		graphics_tri(&graphics_root, space_ship.x + 30, space_ship.y + 15, space_ship.x + 36, space_ship.y, space_ship.x + 41, space_ship.y + 15, color_array[LIGHT_RED]);
		graphics_circ(&graphics_root, space_ship.x + 35, space_ship.y + 2, 2, color_array[YELLOW]);
	}
	else // If false delete
	{
		graphics_clear(&graphics_root, space_ship.x, space_ship.y, 71, 30);
		// graphics_rect(&graphics_root, space_ship.x, space_ship.y, 71, 30, color_array[WHITE]);
	}
}

void move_bullets() // Function that moves the bullet
{
	if (bullet.y > 10 && bullet.active) // If the bullet doesn't reach the top, move the bullet up
	{
		bullet.y -= 20;
	}
	else if (bullet.active) // If the bullet has reached the top and is active, deactivate the bullet so it can be fired again
	{
		bullet.active = false;
	}
}

void draw_bullets(mbool draw) //
{
	if (draw && bullet.active) // Draw  bullet if the bullet is active and true
	{
		// kprint_at(bullet.x, bullet.y, "|", YELLOW);
		graphics_rect(&graphics_root, bullet.x, bullet.y, 3, 10, color_array[YELLOW]);
	}
	else // If false delete
	{
		graphics_clear(&graphics_root, bullet.x, bullet.y, 3, 10);
	}
}

void spawn_asteroids() // Function that spawns passive asteroids from a random x location
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		if (!asteroids[i].active)
		{
			asteroids[i].x = rand(100, video_xres - 130); // 100 - (video_xres - 130) range
			asteroids[i].y = 70;
			asteroids[i].explode = false;
			asteroids[i].phase = 0;
			asteroids[i].active = true;
			return;
		}
	}
}

void move_asteroids()
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		if (asteroids[i].active) // Active ones move
		{
			asteroids[i].y+=10;
			if (asteroids[i].explode)
			{
				asteroids[i].explode = false; // Reset explosion status when you move
				if (asteroids[i].phase < 3)	  // If the size of asteroid is not the smalles, reduce the size
				{
					asteroids[i].phase++; // increasing phase means decreasing size
				}
				else // If it becomes the smallest, deactivate it
					asteroids[i].active = false;
			}
			if ((asteroids[i].x > space_ship.x && asteroids[i].x < space_ship.x + 71 && asteroids[i].y > space_ship.y - 55) || (asteroids[i].y > video_yres - 55)) // If it reaches the bottom of the screen (below the spaceship), lose life and deactivate.
			{
				asteroids[i].active = false;
				life--;
			}
		}
	}
}

void draw_asteroid(int id, mbool draw) // Function to draw a single Asteroid and determine its hitboxes
{
	if (asteroids[id].active)
	{
		switch (asteroids[id].phase) // Enter one of the cases depending on the phase of the active asteroids
		{
		case 0:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 55, color_array[LIGHT_GRAY]);
			}
			else if (draw && asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 55, color_array[RED]);
			}
			else
			{
				graphics_clear(&graphics_root, asteroids[id].x - 55, asteroids[id].y - 55, 112, 112);
			}
			break;

		case 1:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 40, color_array[LIGHT_GRAY]);
			}
			else if (draw && asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 40, color_array[RED]);
			}
			else
			{
				graphics_clear(&graphics_root, asteroids[id].x - 40, asteroids[id].y - 40, 82, 82);
			}
			break;

		case 2:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 25, color_array[LIGHT_GRAY]);
			}
			else if (draw && asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 25, color_array[RED]);
			}
			else
			{
				graphics_clear(&graphics_root, asteroids[id].x - 25, asteroids[id].y - 25, 52, 52);
			}
			break;

		case 3:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 15, color_array[LIGHT_GRAY]);
			}
			else if (draw && asteroids[id].explode)
			{
				graphics_circ(&graphics_root, asteroids[id].x, asteroids[id].y, 15, color_array[RED]);
			}
			else
			{
				graphics_clear(&graphics_root, asteroids[id].x - 15, asteroids[id].y - 15, 32, 32);
			}
			break;
		}
	}
}

void draw_asteroids(mbool draw) // Function to draw all asteroids
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		draw_asteroid(i, draw); // We draw them all one by one using the method
	}
}

mbool check_collision(int x, int y) // Function that controls simple color-check collision
{
	for (int i = x; i < x + 3; i++)
	{
		struct graphics_color c = get_pixel_color(&graphics_root, i, y);
		if ( (c.r != 0 && 0xAA) || c.g != 0 || c.b != 0)
			return true;
	}
	return false;
}
void collision_event() // Function that handles collisions and life checking
{
	if (check_collision(bullet.x, bullet.y) && bullet.y > 16) //If collision find out which asteroid it hit
	{

		float closest_dist = 1000.0f;
		int closest_id = 0;
		for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++) // To calculate all distances and get the closest one
		{
			//Formula for distance between two points sqrt2(pow(x1-x2)+pow(y1-y2))
			int asteroid_dist = sqrt2((asteroids[i].x - bullet.x)*(asteroids[i].x - bullet.x) + (asteroids[i].y - bullet.y)*(asteroids[i].y - bullet.y));
			if(closest_dist > asteroid_dist)
			{
				closest_dist = asteroid_dist;
				closest_id = i;
			}
		}
		//After find we reset bullet and 
		bullet.x = 0;
		bullet.y = 0;
		bullet.active = false;
		asteroids[closest_id].explode = true; // Set the explode flag (of the asteroid we found) to true  
		score++;
	}
	if (life <= 0)
		game_over = true;
}

static unsigned long next; // Random Seeder

int rand(int start, int end) // Function that generate a pseudo-random integer
{
	next = next * 1103515245 + 12345;
	return (unsigned int)(next / 65536) % (end - start) + start;
}

/*
All changes we made to files other than main.c are as follows. 
(A means Added, C means Changed)
event_queue.c, A: 41-49
graphics.c, A: 225-366, C: 368
graphics.h, A: 34-37, C: 38
console.c, A: 155, C: 148
console.h, C: 39
*/

int kernel_main()
{
	struct console *console = console_create_root();
	console_addref(console);
	page_init();
	kmalloc_init((char *)KMALLOC_START, KMALLOC_LENGTH);
	interrupt_init();
	keyboard_init();
	rtc_init();
	clock_init();
	process_init();
	current->ktable[KNO_STDIN] = kobject_create_console(console);
	current->ktable[KNO_STDOUT] = kobject_copy(current->ktable[0]);
	current->ktable[KNO_STDERR] = kobject_copy(current->ktable[1]);
	current->ktable[KNO_STDWIN] = kobject_create_window(&window_root);
	current->ktable[KNO_STDDIR] = 0; // No current dir until something is mounted.

	next = 	boottime; // seeding next value using rtc_boottime_value

Restart_Game:
	game_init();

	if (!game_start)
		kprint_at((video_xres - 11 * TEXT_PIXEL_WIDTH) / 2, video_yres / 2, "Start Game", LIGHT_GRAY); // Appears only on first startup
	game_start = true;

	if (game_restart)
	{ // Appears on every restart
		kprint_at((video_xres - 10 * TEXT_PIXEL_WIDTH) / 2, (video_yres / 2) - 10, "Score:", LIGHT_CYAN);
		kprint_at(((video_xres - 10 * TEXT_PIXEL_WIDTH) / 2) + 6 * TEXT_PIXEL_WIDTH, (video_yres / 2) - 10, Score, WHITE);
		kprint_at((video_xres - 13 * TEXT_PIXEL_WIDTH) / 2, video_yres / 2, "Restart Game", LIGHT_GRAY);
	}
	game_restart = false;

	if (game_over)
	{ // Appears on every gameover
		kprint_at((video_xres - 10 * TEXT_PIXEL_WIDTH) / 2, (video_yres / 2) - 10, "Score:", LIGHT_CYAN);
		kprint_at(((video_xres - 10 * TEXT_PIXEL_WIDTH) / 2) + 6 * TEXT_PIXEL_WIDTH, (video_yres / 2) - 10, Score, WHITE);
		kprint_at((video_xres - 10 * TEXT_PIXEL_WIDTH) / 2, video_yres / 2, "Game Over", LIGHT_GRAY);
	}
	game_over = false;

	for (int i = 0; i < 3; i++)
		Score[i] = '0'; // Clear Score Array
	int loop_step = 0;	// We create(reset) the loop counter to create delay.

	kprint_at((video_xres - 33 * TEXT_PIXEL_WIDTH) / 2, 10, "180101012 - Ibrahim Yusuf Cosgun", DARK_GRAY);

Pause_Game:
	if (game_pause)
		kprint_at((video_xres - 14 * TEXT_PIXEL_WIDTH) / 2, video_yres / 2, "Continue Game", LIGHT_GRAY);
	game_pause = false;

	kprint_at((video_xres - 14 * TEXT_PIXEL_WIDTH) / 2, (video_yres / 2) + 10, "Press [Enter]", DARK_GRAY); // Appears always

	while (current_key != ASCII_CR)
	{
		console_read(console, &current_key, 1);
	}

	clear_screen();
	while (1)
	{
		console_read_nonblock(console, &current_key, 1, &key_pressed);
		if (game_restart || game_over) // Proper reset with goto in case of gameover or restart of the game
			goto Restart_Game;

		if (game_pause) // Pause by just freezing the screen without resetting game objects (Buffer is not a game object, it makes the printing process faster and in one piece.)
			goto Pause_Game;

		clock_wait(50); // Sleep 50ms

		// Clear
		draw_space_ship(false);
		draw_bullets(false);

		// Move and do Events
		move_and_fire_space_ship();
		move_bullets();
		if (loop_step++ % 120 == 0)
		{
			spawn_asteroids();
		}
		if (loop_step % 10 == 0)
		{
			// Before Movement Clear Asteroids
			draw_asteroids(false);
			move_asteroids();
		}
		collision_event();

		// Print
		draw_space_ship(true);
		draw_bullets(true);
		draw_asteroids(true);
		print_score();
		print_life();
	}

	return 0;
}
