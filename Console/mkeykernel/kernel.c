/*
 * Copyright (C) 2014  Arjun Sreedharan
 * License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html
 */
#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT *COLUMNS_IN_LINE *LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

typedef enum
{
	false,
	true
} bool; // define bool as enum to increase readability

// These definitions are used to call colors by name using bios color codes. It increases readability.
#define BLACK 0x00
#define BLUE 0x01
#define GREEN 0x02
#define CYAN 0x03
#define RED 0x04
#define MAGENTA 0x05
#define BROWN 0x06
#define LIGHT_GRAY 0x07
#define DARK_GRAY 0x08
#define LIGHT_BLUE 0x09
#define LIGHT_GREEN 0x0A
#define LIGHT_CYAN 0x0B
#define LIGHT_RED 0x0C
#define LIGHT_MAGENTA 0x0D
#define YELLOW 0x0E
#define WHITE 0x0F

// #define ENTER_KEY_CODE 0x1C

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* video memory begins at address 0xb8000 */
char *vidptr = (char *)0xb8000;

struct IDT_entry
{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};
struct IDT_entry IDT[IDT_SIZE];

struct Space_Ship
{
	int x;
	int y;
};

struct Bullet
{
	bool active;
	int x;
	int y;
};

#define NUMBER_OF_ASTEROIDS 3
struct Asteroid
{
	bool active;
	int x_start;
	int x_end;
	int y_start;
	int y_end;
	bool explode;
	int phase;
};
struct Space_Ship space_ship;
struct Bullet bullet;
struct Asteroid asteroids[NUMBER_OF_ASTEROIDS];

char current_key = 0;
bool key_pressed = false;
bool game_start = false;
bool game_over = false;
bool game_restart = false;
bool game_pause = false;
int life = 3;
int score = 0;
char Score[3];

void init_space_ship() // Function that places space_ship in the bottom middle
{
	space_ship.x = (COLUMNS_IN_LINE - 5) / 2; // pos = middle bottom
	space_ship.y = LINES - 2;
}

void init_asteroids() // Function that clears and initializes the asteroids[]
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		asteroids[i].active = false; // Disable all asteroids so they can be (re)spawn
	}
}

void kprint_at(int x, int y, const char *str, int color) // Function that places specific string and color at specific x and y coordinates into video memory buffer
{
	// Calculate absolute index in video memory buffer
	int index = BYTES_FOR_EACH_ELEMENT * (y * COLUMNS_IN_LINE + x);
	while (*str != '\0') // For each character
	{
		//[' '] [color]
		vidptr[index++] = *str++;
		vidptr[index++] = color;
	}
}

void clear_screen() // Function that clears the screen
{
	unsigned int i = 0;
	while (i < SCREENSIZE) // Reset all elements of video buffer
	{
		//[' '] [color] is struct of each character in video buffer
		vidptr[i++] = ' ';
		vidptr[i++] = BLACK;
	}
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
	kprint_at(COLUMNS_IN_LINE - 9, 0, "Score:", LIGHT_CYAN);
	kprint_at(COLUMNS_IN_LINE - 3, 0, Score, WHITE);
}

void print_life() // Function that writes the life in the upper left corner of the screen
{
	char life_char[2] = {(char)life + '0', '\0'};
	kprint_at(0, 0, "Life:", LIGHT_GREEN);
	kprint_at(5, 0, life_char, WHITE);
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
			if (space_ship.x > 1)
				space_ship.x--;
			break;
		case 'd':
			if (space_ship.x < COLUMNS_IN_LINE - 6)
				space_ship.x++;
			break;
		case ' ':
			if (!bullet.active) // If a bullet has been fired, a new bullet cannot be fired until that bullet hits something.
			{
				bullet.y = space_ship.y;
				bullet.x = space_ship.x + 2;
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

void draw_space_ship(bool draw) // Function that draws and deletes the space_ship
{
	if (draw) // Draw space_ship if true
	{
		kprint_at(space_ship.x, space_ship.y, "/-^-\\", WHITE);
	}
	else // If false delete
	{
		kprint_at(space_ship.x - 1, space_ship.y, "       ", BLACK);
	}
}

void move_bullets() // Function that moves the bullet
{
	if (bullet.y > 1) // If the bullet doesn't reach the top, move the bullet up
	{
		bullet.y--;
	}
	else if (bullet.active) // If the bullet has reached the top and is active, deactivate the bullet so it can be fired again
	{
		bullet.active = false;
	}
}

void draw_bullets(bool draw) //
{
	if (draw && bullet.active) // Draw  bullet if the bullet is active and true
	{
		kprint_at(bullet.x, bullet.y, "|", YELLOW);
	}
	else // If false delete
	{
		kprint_at(bullet.x, bullet.y, " ", BLACK);
	}
}

void spawn_asteroids() // Function that spawns passive asteroids from a random x location
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		if (!asteroids[i].active)
		{
			asteroids[i].x_start = rand(1, COLUMNS_IN_LINE - 9); // 1 - (COLUMNS_IN_LINE-9) range
			asteroids[i].y_start = 1;
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
			asteroids[i].y_start++;
			if (asteroids[i].explode)
			{
				asteroids[i].explode = false; // Reset explosion status when you move
				if (asteroids[i].phase < 3)	  // If the size of asteroid is not the smalles, reduce the size
				{
					asteroids[i].phase++; // increasing phase means decreasing size
					asteroids[i].x_start++;
				}
				else // If it becomes the smallest, deactivate it
					asteroids[i].active = false;
			}
			if (asteroids[i].y_end > LINES - 3) // If it reaches the bottom of the screen (below the spaceship), lose life and deactivate.
			{
				asteroids[i].active = false;
				life--;
			}
		}
	}
}

void draw_asteroid(int id, bool draw) // Function to draw a single Asteroid and determine its hitboxes
{
	if (asteroids[id].active)
	{
		switch (asteroids[id].phase) // Enter one of the cases depending on the phase of the active asteroids
		{
		case 0:
			asteroids[id].x_end = asteroids[id].x_start + 7;
			asteroids[id].y_end = asteroids[id].y_start + 3;
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " ######", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "########", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, "########", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 3, " ######", LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " \\\\|//", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "\\\\\\\\////", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, "////\\\\\\\\", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 3, " //|\\\\", RED);
			}
			else
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "       ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "        ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, "        ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 3, "       ", BLACK);
			}
			break;

		case 1:
			asteroids[id].x_end = asteroids[id].x_start + 5;
			asteroids[id].y_end = asteroids[id].y_start + 2;
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " ####", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "######", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, " ####", LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " \\\\//", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, ">>><<<", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, " //\\\\", RED);
			}
			else
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "     ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "      ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, "     ", BLACK);
			}
			break;

		case 2:
			asteroids[id].x_end = asteroids[id].x_start + 2;
			asteroids[id].y_end = asteroids[id].y_start + 1;
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "###", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "###", LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "\\|/", RED);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "/|\\", RED);
			}
			else
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "   ", BLACK);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "   ", BLACK);
			}
			break;

		case 3:
			asteroids[id].x_end = asteroids[id].x_start + 1;
			asteroids[id].y_end = asteroids[id].y_start;
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "##", LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "><", RED);
			}
			else
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "  ", BLACK);
			}
			break;
		}
	}
}

void draw_asteroids(bool draw) // Function to draw all asteroids
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		draw_asteroid(i, draw); // We draw them all one by one using the method
	}
}

bool check_collision(int x, int y, int x_start, int x_end, int y_start, int y_end) // Function that controls simple hitbox and point collision
{
	if ((x >= x_start && x <= x_end) && (y >= y_start && y <= y_end))
		return true;
	return false;
}

void collision_event() // Function that handles collisions and life checking
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++) // Perform collision checks for all asteroids
	{
		if (check_collision(bullet.x, bullet.y, asteroids[i].x_start, asteroids[i].x_end, asteroids[i].y_start, asteroids[i].y_end) && bullet.active && asteroids[i].active)
		{
			bullet.active = false;
			asteroids[i].explode = true;
			score++;
		}
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

unsigned int get_cpu_timer_value() // Function that get the current value of the system timer
{
	unsigned int val;
	__asm__ volatile("rdtsc" : "=a"(val)); // Get Read Time-Stamp Counter value using inline assembly
	return val;
}

void sleep(int milliseconds) // Sleep Method 1 milisecond ~ 0.8-1.0ms
{
	unsigned int loop_count = (unsigned int)milliseconds * 150000;
	for (unsigned int i = 0; i < loop_count; i++)
		;
}

void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     Ports
	 *	 PIC1	PIC2
	 *Command 0x20	0xA0
	 *Data	 0x21	0xA1
	 */

	/* ICW1 - begin initialization */
	write_port(0x20, 0x11);
	write_port(0xA0, 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	 * In x86 protected mode, we have to remap the PICs beyond 0x20 because
	 * Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	 */
	write_port(0x21, 0x20);
	write_port(0xA1, 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21, 0x00);
	write_port(0xA1, 0x00);

	/* ICW4 - environment info */
	write_port(0x21, 0x01);
	write_port(0xA1, 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21, 0xff);
	write_port(0xA1, 0xff);

	/* disable cursor blinking */
	write_port(0x3D4, 0x0A); // Select start register
	write_port(0x3D5, 0x20); // Disable cursor blinking by setting bit 5

	write_port(0x3D4, 0x0B); // Select end register
	write_port(0x3D5, 0x00); // Setting end register to 0 disables cursor

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT;
	idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21, 0xFD);
}

void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01)
	{
		keycode = read_port(KEYBOARD_DATA_PORT);
		if (keycode < 0)
			return;

		current_key = (char)keyboard_map[(unsigned char)keycode];
		key_pressed = true; // It activates this flag instead of interrupt, thus interaction is made.
	}
}

void kmain(void)
{
	idt_init();
	kb_init();
	next = get_cpu_timer_value(); // seeding next value using cpu_timer_value

Restart_Game:
	game_init(); // Init and Reset game variables

	if (!game_start)
		kprint_at((COLUMNS_IN_LINE - 11) / 2, LINES / 2, "Start Game", LIGHT_GRAY); // Appears only on first startup
	game_start = true;

	if (game_restart)
	{ // Appears on every restart
		kprint_at((COLUMNS_IN_LINE - 9) / 2, (LINES / 2) - 1, "Score:", LIGHT_CYAN);
		kprint_at(((COLUMNS_IN_LINE - 9) / 2) + 6, (LINES / 2) - 1, Score, WHITE);
		kprint_at((COLUMNS_IN_LINE - 13) / 2, LINES / 2, "Restart Game", LIGHT_GRAY);
	}
	game_restart = false;

	if (game_over)
	{ // Appears on every gameover
		kprint_at((COLUMNS_IN_LINE - 9) / 2, (LINES / 2) - 1, "Score:", LIGHT_CYAN);
		kprint_at(((COLUMNS_IN_LINE - 9) / 2) + 6, (LINES / 2) - 1, Score, WHITE);
		kprint_at((COLUMNS_IN_LINE - 10) / 2, LINES / 2, "Game Over", LIGHT_GRAY);
	}
	game_over = false;

	for (int i = 0; i < 3; i++)
		Score[i] = '0'; // Clear Score Array
	int loop_step = 0;	// We create(reset) the loop counter to create delay.

	kprint_at((COLUMNS_IN_LINE - 33) / 2, 1, "180101012 - Ibrahim Yusuf Cosgun", DARK_GRAY);

Pause_Game:
	if (game_pause)
		kprint_at((COLUMNS_IN_LINE - 14) / 2, LINES / 2, "Continue Game", LIGHT_GRAY); // Appears every pause
	game_pause = false;

	kprint_at((COLUMNS_IN_LINE - 14) / 2, (LINES / 2) + 1, "Press [Enter]", DARK_GRAY); // Appears always

	while (current_key != '\n' || !key_pressed) // Wait for [Enter] Key
		;
	clear_screen(); // Reset and Inıt video buffer
	while (true)
	{
		if (game_restart || game_over) // Proper reset with goto in case of gameover or restart of the game
			goto Restart_Game;

		if (game_pause) // Pause by just freezing the screen without resetting game objects (Buffer is not a game object, it makes the printing process faster and in one piece.)
			goto Pause_Game;

		sleep(35); // Sleep 35ms

		// Clear
		draw_space_ship(false);
		draw_bullets(false);
		draw_asteroids(false);

		// Move and do Events
		move_and_fire_space_ship();
		move_bullets();
		if (loop_step++ % 90 == 0)
		{
			spawn_asteroids();
		}
		if (loop_step % 15 == 0)
		{
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
	while (1)
		;
}
