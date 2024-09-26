#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Surface *getPixel_Surface;
Uint8 getPixel_bpp;
TTF_Font *font = NULL;
SDL_Event event;
int quit;

SDL_Color color_array[16] = {
	{0, 0, 0, 255},
	{0, 0, 0xAA, 255},
	{0, 0xAA, 0, 255},
	{0, 0xAA, 0xAA, 255},
	{0xAA, 0, 0, 255},
	{0xAA, 0, 0xAA, 255},
	{0xAA, 0x55, 0, 255},
	{0xCC, 0xCC, 0xCC, 255},
	{0x55, 0x55, 0x55, 255},
	{0, 0, 0xFF, 255},
	{0, 0xFF, 0, 255},
	{0, 0xFF, 0xFF, 255},
	{0xFF, 0, 0, 255},
	{0xFF, 0, 0xFF, 255},
	{0xFF, 0xFF, 0, 255},
	{0xFF, 0xFF, 0xFF, 255}};

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
} bool; // define bool as enum to increase readability

#define TEXT_PIXEL_WIDTH 8
#define TEXT_PIXEL_HEIGHT 7

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
	int x;
	int y;
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

void FillRect(int x, int y, int w, int h, int color);
void FillCircle(int x, int y, int r, int color);
void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);

void init_space_ship() // Function that places space_ship in the bottom middle
{
	space_ship.x = (WINDOW_WIDTH - 71) / 2; // pos = middle bottom
	space_ship.y = WINDOW_HEIGHT - 30;
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
	SDL_Surface *surface = TTF_RenderText_Solid(font, str, color_array[color]);
	if (surface == NULL)
		return;
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	if (texture == NULL)
	{
		SDL_FreeSurface(surface);
		return;
	}
	SDL_Rect dstRect = {x, y, surface->w, surface->h}; // Position and size
	SDL_RenderCopy(renderer, texture, NULL, &dstRect);

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
}

void clear_screen()
{
	SDL_SetRenderDrawColor(renderer, color_array[BLACK].r, color_array[BLACK].g, color_array[BLACK].b, color_array[BLACK].a); // black
	SDL_RenderClear(renderer);
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

void print_score(bool draw) // Function that writes the score in the upper right corner of the screen
{
	if (draw) // Draw if true
	{
		int temp = score;
		for (int i = 2; i > -1; i--)
		{
			Score[i] = (char)temp % 10 + '0';
			temp /= 10;
		}
		kprint_at(WINDOW_WIDTH - 11 * TEXT_PIXEL_WIDTH, 8, "Score:", LIGHT_CYAN);
		kprint_at(WINDOW_WIDTH - 4 * TEXT_PIXEL_WIDTH, 8, Score, WHITE);
	}
	else // If false delete
	{
		FillRect(WINDOW_WIDTH - 11 * TEXT_PIXEL_WIDTH, 8, 88, 15, BLACK);
	}
}

void print_life(bool draw) // Function that writes the life in the upper left corner of the screen
{
	if (draw) // Draw if true
	{
		char life_char[2] = {(char)life + '0', '\0'};
		kprint_at(8, 8, "Life:", LIGHT_GREEN);
		kprint_at(8 + 6 * TEXT_PIXEL_WIDTH, 8, life_char, WHITE);
	}
	else // If false delete
	{
		FillRect(8, 8, 60, 15, BLACK);
	}
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
			if (space_ship.x < WINDOW_WIDTH - 75)
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

void draw_space_ship(bool draw) // Function that draws and deletes the space_ship
{
	if (draw) // Draw space_ship if true
	{
		FillRect(space_ship.x + 20, space_ship.y + 15, 30, 5, DARK_GRAY);
		FillTriangle(space_ship.x, space_ship.y + 30, space_ship.x + 20, space_ship.y + 25, space_ship.x + 30, space_ship.y + 2, WHITE);
		FillTriangle(space_ship.x + 71, space_ship.y + 30, space_ship.x + 51, space_ship.y + 25, space_ship.x + 41, space_ship.y + 2, WHITE);
		FillTriangle(space_ship.x + 30, space_ship.y + 15, space_ship.x + 36, space_ship.y, space_ship.x + 41, space_ship.y + 15, LIGHT_RED);
		FillCircle(space_ship.x + 35, space_ship.y + 2, 2, YELLOW);
	}
	else // If false delete
	{
		FillRect(space_ship.x, space_ship.y, 71, 30, BLACK);
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

void draw_bullets(bool draw) //
{
	if (draw && bullet.active) // Draw  bullet if the bullet is active and true
	{
		FillRect(bullet.x, bullet.y, 3, 10, YELLOW);
	}
	else // If false delete
	{
		FillRect(bullet.x, bullet.y, 3, 10, BLACK);
	}
}

void spawn_asteroids() // Function that spawns passive asteroids from a random x location
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		if (!asteroids[i].active)
		{
			asteroids[i].x = rand() % (WINDOW_WIDTH - 130 - 100) + 100; // 100 - (WINDOW_WIDTH - 130) range
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
			asteroids[i].y += 10;
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
			if ((asteroids[i].x > space_ship.x && asteroids[i].x < space_ship.x + 71 && asteroids[i].y > space_ship.y - 55) || (asteroids[i].y > WINDOW_HEIGHT - 55)) // If it reaches the bottom of the screen (below the spaceship), lose life and deactivate.
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
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 55, LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 55, RED);
			}
			else
			{
				FillRect(asteroids[id].x - 55, asteroids[id].y - 55, 112, 112, BLACK);
			}
			break;

		case 1:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 40, LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 40, RED);
			}
			else
			{
				FillRect(asteroids[id].x - 40, asteroids[id].y - 40, 82, 82, BLACK);
			}
			break;

		case 2:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 25, LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 25, RED);
			}
			else
			{
				FillRect(asteroids[id].x - 25, asteroids[id].y - 25, 52, 52, BLACK);
			}
			break;

		case 3:
			// Determine size according to phase
			if (draw && !asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 15, LIGHT_GRAY);
			}
			else if (draw && asteroids[id].explode)
			{
				FillCircle(asteroids[id].x, asteroids[id].y, 15, RED);
			}
			else
			{
				FillRect(asteroids[id].x - 15, asteroids[id].y - 15, 32, 32, BLACK);
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

SDL_Color GetPixelColor(int pixel_X, int pixel_Y)
{
	SDL_Color pixelColor;
	Uint8 *pPixel = (Uint8 *)getPixel_Surface->pixels + pixel_Y * getPixel_Surface->pitch + pixel_X * getPixel_bpp;

	Uint32 pixelData;

	switch (getPixel_bpp)
	{
	case 1:
		pixelData = *pPixel;
		break;
	case 2:
		pixelData = *(Uint16 *)pPixel;
		break;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			pixelData = pPixel[0] << 16 | pPixel[1] << 8 | pPixel[2];
		else
			pixelData = pPixel[0] | pPixel[1] << 8 | pPixel[2] << 16;
		break;
	case 4:
		pixelData = *(Uint32 *)pPixel;
		break;
	}

	SDL_GetRGBA(pixelData, getPixel_Surface->format, &pixelColor.r, &pixelColor.g, &pixelColor.b, &pixelColor.a);
	return pixelColor;
}

bool check_collision(int x, int y) // Function that controls simple color-check collision
{
	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, getPixel_Surface->pixels, getPixel_Surface->pitch);
	for (int i = x; i < x + 3; i++)
	{
		SDL_Color color = GetPixelColor(x, y);
		if ((color.r != 0 && 0xAA) || color.g != 0 || color.b != 0)
		{
			return true;
		}
	}
	return false;
}
void collision_event() // Function that handles collisions and life checking
{
	if (check_collision(bullet.x, bullet.y) && bullet.y > 16) // If collision find out which asteroid it hit
	{

		float closest_dist = 1000.0f;
		int closest_id = 0;
		for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++) // To calculate all distances and get the closest one
		{
			// Formula for distance between two points sqrt2(pow(x1-x2)+pow(y1-y2))
			int asteroid_dist = sqrt((asteroids[i].x - bullet.x) * (asteroids[i].x - bullet.x) + (asteroids[i].y - bullet.y) * (asteroids[i].y - bullet.y));
			if (closest_dist > asteroid_dist)
			{
				closest_dist = asteroid_dist;
				closest_id = i;
			}
		}
		// After find we reset bullet and
		bullet.x = 0;
		bullet.y = 0;
		bullet.active = false;
		asteroids[closest_id].explode = true; // Set the explode flag (of the asteroid we found) to true
		score++;
	}
	if (life <= 0)
		game_over = true;
}

int main()
{

	srand(time(NULL));
	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer);
	TTF_Init();
	font = TTF_OpenFont("VGA.ttf", 16);
	SDL_RenderPresent(renderer);
	getPixel_Surface = SDL_CreateRGBSurface(0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, getPixel_Surface->pixels, getPixel_Surface->pitch);
	getPixel_bpp = getPixel_Surface->format->BytesPerPixel;
Restart_Game:
	game_init();

	if (!game_start)
		kprint_at((WINDOW_WIDTH - 11 * TEXT_PIXEL_WIDTH) / 2, WINDOW_HEIGHT / 2, "Start Game", LIGHT_GRAY); // Appears only on first startup
	game_start = true;

	if (game_restart)
	{ // Appears on every restart
		kprint_at((WINDOW_WIDTH - 10 * TEXT_PIXEL_WIDTH) / 2, (WINDOW_HEIGHT / 2) - 16, "Score:", LIGHT_CYAN);
		kprint_at(((WINDOW_WIDTH - 10 * TEXT_PIXEL_WIDTH) / 2) + 7 * TEXT_PIXEL_WIDTH, (WINDOW_HEIGHT / 2) - 16, Score, WHITE);
		kprint_at((WINDOW_WIDTH - 13 * TEXT_PIXEL_WIDTH) / 2, WINDOW_HEIGHT / 2, "Restart Game", LIGHT_GRAY);
	}
	game_restart = false;

	if (game_over)
	{ // Appears on every gameover
		kprint_at((WINDOW_WIDTH - 10 * TEXT_PIXEL_WIDTH) / 2, (WINDOW_HEIGHT / 2) - 16, "Score:", LIGHT_CYAN);
		kprint_at(((WINDOW_WIDTH - 10 * TEXT_PIXEL_WIDTH) / 2) + 7 * TEXT_PIXEL_WIDTH, (WINDOW_HEIGHT / 2) - 16, Score, WHITE);
		kprint_at((WINDOW_WIDTH - 10 * TEXT_PIXEL_WIDTH) / 2, WINDOW_HEIGHT / 2, "Game Over", LIGHT_GRAY);
	}
	game_over = false;

	for (int i = 0; i < 3; i++)
		Score[i] = '0'; // Clear Score Array
	int loop_step = 0;	// We create(reset) the loop counter to create delay.

	kprint_at((WINDOW_WIDTH - 33 * TEXT_PIXEL_WIDTH) / 2, 16, "180101012 - Ibrahim Yusuf Cosgun", DARK_GRAY);

Pause_Game:
	if (game_pause)
		kprint_at((WINDOW_WIDTH - 14 * TEXT_PIXEL_WIDTH) / 2, WINDOW_HEIGHT / 2, "Continue Game", LIGHT_GRAY);
	game_pause = false;

	kprint_at((WINDOW_WIDTH - 14 * TEXT_PIXEL_WIDTH) / 2, (WINDOW_HEIGHT / 2) + 16, "Press [Enter]", DARK_GRAY); // Appears always
	SDL_RenderPresent(renderer);

	while (current_key != '\r')
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				quit = 1;
			else if (event.type == SDL_KEYDOWN)
			{
				current_key = event.key.keysym.sym;
			}
		}				   // Read char using event pool
		usleep(50 * 1000); // Sleep 50ms
	}

	clear_screen();
	quit = 0;
	while (!quit)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				quit = 1;
			else if (event.type == SDL_KEYDOWN)
			{
				current_key = event.key.keysym.sym;
				key_pressed = true;
			}
		}// Read char using event pool
		if (game_restart || game_over) // Proper reset with goto in case of gameover or restart of the game
			goto Restart_Game;

		if (game_pause) // Pause by just freezing the screen without resetting game objects (Buffer is not a game object, it makes the printing process faster and in one piece.)
			goto Pause_Game;

		usleep(50 * 1000); // Sleep 50ms

		// Clear
		draw_space_ship(false);
		draw_bullets(false);
		print_score(false);
		print_life(false);

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
		print_score(true);
		print_life(true);
		SDL_RenderPresent(renderer);
	}
	TTF_Quit();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

void FillRect(int x, int y, int w, int h, int color)
{
	SDL_SetRenderDrawColor(renderer, color_array[color].r, color_array[color].g, color_array[color].b, color_array[color].a);
	SDL_Rect rect_area = {x, y, w, h};
	SDL_RenderFillRect(renderer, &rect_area);
	SDL_SetRenderDrawColor(renderer, color_array[BLACK].r, color_array[BLACK].g, color_array[BLACK].b, color_array[BLACK].a);
}
void FillCircle(int x, int y, int r, int color)
{
	SDL_SetRenderDrawColor(renderer, color_array[color].r, color_array[color].g, color_array[color].b, color_array[color].a);
	int i, j;
	float sinus = 0.70710678118;
	// This is the distance on the axis from sin(90) to sin(45).
	int range = r / (2 * sinus);
	for (int i = r; i >= range; --i)
	{
		int j = sqrt(r * r - i * i);
		for (int k = -j; k <= j; k++)
		{
			// We draw all the 4 sides at the same time.
			SDL_RenderDrawPoint(renderer, x - k, y + i);
			SDL_RenderDrawPoint(renderer, x - k, y - i);
			SDL_RenderDrawPoint(renderer, x + i, y + k);
			SDL_RenderDrawPoint(renderer, x - i, y - k);
		}
	}
	// To fill the circle we draw the circumscribed square.
	range = r * sinus;
	for (int i = x - range + 1; i < x + range; i++)
	{
		for (int j = y - range + 1; j < y + range; j++)
		{
			SDL_RenderDrawPoint(renderer, i, j);
		}
	}
	SDL_SetRenderDrawColor(renderer, color_array[BLACK].r, color_array[BLACK].g, color_array[BLACK].b, color_array[BLACK].a);
}

#define SWAP(x, y)       \
	do                   \
	{                    \
		(x) = (x) ^ (y); \
		(y) = (x) ^ (y); \
		(x) = (x) ^ (y); \
	} while (0)
void HLine(int x1, int x2, int y)
{
	if (x1 >= x2)
		SWAP(x1, x2);
	for (; x1 <= x2; x1++)
		SDL_RenderDrawPoint(renderer, x1, y);
}
// Fill a triangle - slope method
void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
	SDL_SetRenderDrawColor(renderer, color_array[color].r, color_array[color].g, color_array[color].b, color_array[color].a);
	int a, b, y, last;
	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if (y0 > y1)
	{
		SWAP(y0, y1);
		SWAP(x0, x1);
	}
	if (y1 > y2)
	{
		SWAP(y2, y1);
		SWAP(x2, x1);
	}
	if (y0 > y1)
	{
		SWAP(y0, y1);
		SWAP(x0, x1);
	}

	if (y0 == y2)
	{ // All on same line case
		a = b = x0;
		if (x1 < a)
			a = x1;
		else if (x1 > b)
			b = x1;
		if (x2 < a)
			a = x2;
		else if (x2 > b)
			b = x2;
		HLine(a, b, y0);
		return;
	}

	double
		dx01 = x1 - x0,
		dy01 = y1 - y0,
		dx02 = x2 - x0,
		dy02 = y2 - y0,
		dx12 = x2 - x1,
		dy12 = y2 - y1;
	double sa = 0, sb = 0;

	// For upper part of triangle, find scanline crossings for segment
	// 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y
	// is included here (and second loop will be skipped, avoiding a /
	// error there), otherwise scanline y1 is skipped here and handle
	// in the second loop...which also avoids a /0 error here if y0=y
	// (flat-topped triangle)
	if (y1 == y2)
		last = y1; // Include y1 scanline
	else
		last = y1 - 1; // Skip it

	for (y = y0; y <= last; y++)
	{
		a = x0 + sa / dy01;
		b = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;
		// longhand a = x0 + (x1 - x0) * (y - y0) / (y1 - y0)
		//          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
		HLine(a, b, y);
	}

	// For lower part of triangle, find scanline crossings for segment
	// 0-2 and 1-2.  This loop is skipped if y1=y2
	sa = dx12 * (y - y1);
	sb = dx02 * (y - y0);
	for (; y <= y2; y++)
	{
		a = x1 + sa / dy12;
		b = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;
		// longhand a = x1 + (x2 - x1) * (y - y1) / (y2 - y1)
		//          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
		HLine(a, b, y);
	}
	SDL_SetRenderDrawColor(renderer, color_array[BLACK].r, color_array[BLACK].g, color_array[BLACK].b, color_array[BLACK].a);
}