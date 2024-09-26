#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

#define HEIGHT 25
#define WIDTH 80
#define SCREENSIZE WIDTH *HEIGHT
typedef enum
{
	false,
	true
} bool;

const char *color_array[] = {
	"0;30",
	"0;34",
	"0;32",
	"0;36",
	"0;31",
	"0;35",
	"0;33",
	"0;37",
	"1;30",
	"1;34",
	"1;32",
	"1;36",
	"1;31",
	"1;35",
	"1;33",
	"1;37"};

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

char screen_buffer[SCREENSIZE];
int color_buffer[SCREENSIZE];

#define NUMBER_OF_ASTEROIDS 4

struct Space_Ship
{
	int x;
	int y;
};
struct Space_Ship space_ship = {(WIDTH - 5) / 2, HEIGHT - 2};

struct Bullet
{
	bool active;
	int x;
	int y;
};
struct Bullet bullet = {false};

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
struct Asteroid asteroids[NUMBER_OF_ASTEROIDS];

char current_key;
bool key_pressed = false;
bool game_start = false;
bool game_over = false;
bool game_restart = false;
bool game_pause = false;
int life = 9;
int score = 0;
char Score[3];

void init_space_ship()
{
	space_ship.x = (WIDTH - 5) / 2;
	space_ship.y = HEIGHT - 2;
}

void init_asteroids()
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		asteroids[i].active = false;
	}
}

// void kprint_at(int x, int y, const char *str, const char *color) {
//     // Move cursor to position (x, y)
//     printf("\033[%d;%dH", y, x);
//     // Set text color
//     printf("%s", color);

//     // Print each character in the string
//     while (*str != '\0') {
//         printf("%c", *str);
//         str++;
//     }

//     // Reset text color
//     printf("%s", RESET);
// }

void kprint_at(int x, int y, const char *str, int color)
{
	int index = (y * WIDTH) + x;

	while (*str != '\0')
	{
		color_buffer[index] = color;
		screen_buffer[index++] = *str++;
	}
}

void clear_buffers()
{
	for (int i = 0; i < SCREENSIZE; i++)
	{
		screen_buffer[i] = ' ';
		color_buffer[i] = BLACK;
	}
}

void clear_screen()
{
	printf("\033[2J\033[H");
}

void game_init()
{
	init_space_ship();
	init_asteroids();
	bullet.active = false;
	clear_screen();
	clear_buffers();
	score = 0;
	life = 3;
}

void print_score()
{
	int temp = score;
	for (int i = 2; i > -1; i--)
	{
		Score[i] = (char)temp % 10 + '0';
		temp /= 10;
	}
	kprint_at(WIDTH - 9, 0, "Score:", LIGHT_CYAN);
	kprint_at(WIDTH - 3, 0, Score, WHITE);
}

void print_life()
{
	char life_char[1];
	life_char[0] = (char)life + '0';
	kprint_at(0, 0, "Life:", LIGHT_GREEN);
	kprint_at(6, 0, life_char, WHITE);
}

void move_and_fire_space_ship()
{
	if (key_pressed)
	{
		switch (current_key)
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
			if (space_ship.x < WIDTH - 6)
				space_ship.x++;
			break;
		case ' ':
			if (!bullet.active)
			{
				bullet.y = space_ship.y;
				bullet.x = space_ship.x + 2;
				bullet.active = true;
			}
			break;
		case 'r': // TODO
			game_restart = true;
			break;
		case 'p':
			game_pause = true;
			break;
		default:
			break;
		}
		key_pressed = false;
	}
}

void draw_space_ship(bool draw)
{
	if (draw)
	{
		kprint_at(space_ship.x, space_ship.y, "/-^-\\", WHITE);
	}
	else
	{
		kprint_at(space_ship.x - 1, space_ship.y, "       ", BLACK);
	}
}

void move_bullets()
{
	if (bullet.y > 1)
	{
		bullet.y--;
	}
	else if (bullet.active)
	{
		bullet.active = false;
	}
}

void draw_bullets(bool draw)
{
	if (draw && bullet.active)
	{
		kprint_at(bullet.x, bullet.y, "|", YELLOW);
	}
	else
	{
		kprint_at(bullet.x, bullet.y, " ", BLACK);
	}
}

void spawn_asteroids()
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		if (!asteroids[i].active)
		{
			asteroids[i].x_start = rand() % (WIDTH - 9 - 1) + 1;
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
		if (asteroids[i].active)
		{
			asteroids[i].y_start++;
			if (asteroids[i].explode)
			{
				asteroids[i].explode = false;
				if (asteroids[i].phase < 3)
				{
					asteroids[i].phase++;
					asteroids[i].x_start++;
				}
				else
					asteroids[i].active = false;
			}
			if (asteroids[i].y_end > HEIGHT - 3)
			{
				asteroids[i].active = false;
				life--;
			}
		}
	}
}

void draw_asteroid(int id, bool draw)
{
	if (asteroids[id].active && !asteroids[id].explode)
	{
		switch (asteroids[id].phase)
		{
		case 0:
			asteroids[id].x_end = asteroids[id].x_start + 7;
			asteroids[id].y_end = asteroids[id].y_start + 3;
			if (draw)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " ######", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "########", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, "########", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 3, " ######", LIGHT_GRAY);
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
			if (draw)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, " ####", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "######", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 2, " ####", LIGHT_GRAY);
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
			if (draw)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "###", LIGHT_GRAY);
				kprint_at(asteroids[id].x_start, asteroids[id].y_start + 1, "###", LIGHT_GRAY);
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
			if (draw)
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "##", LIGHT_GRAY);
			}
			else
			{
				kprint_at(asteroids[id].x_start, asteroids[id].y_start, "  ", BLACK);
			}
			break;
		}
	}
	else if (asteroids[id].active && asteroids[id].explode)
	{
		switch (asteroids[id].phase)
		{
		case 0:
			asteroids[id].x_end = asteroids[id].x_start + 7;
			asteroids[id].y_end = asteroids[id].y_start + 3;
			if (draw)
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
			if (draw)
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
			if (draw)
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
			if (draw)
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

void draw_asteroids(bool draw)
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
	{
		draw_asteroid(i, draw);
	}
}
bool check_collision(int x, int y, int x_start, int x_end, int y_start, int y_end)
{
	if ((x >= x_start && x <= x_end) && (y >= y_start && y <= y_end))
		return true;
	return false;
}

void collision_event()
{
	for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
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

void *KeyListener(void *vargp)
{
	while (true)
	{
		scanf("%c", &current_key);
		key_pressed = 1;
	}
	return NULL;
}

void term_mode(int mode)
{
	static struct termios origt, newt;

	if (mode == 1)
	{
		tcgetattr(STDIN_FILENO, &origt);
		newt = origt;
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}
	else
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &origt);
	}
}

void print_screen()
{
	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			int index = (y * WIDTH) + x;
			// ANSI kaçış dizilerini kullanarak renkli veya biçimlendirilmiş metin yazdır
			// if (color_board[i][j] != '\0')
			// {
			printf("\033[%sm%c\033[0m", color_array[color_buffer[index]], screen_buffer[index]);
			// }
			// else
			// {
			// 	printf("%c", board[i][j]);
			// }
		}
		printf("\n");
	}
}

void main(void)
{
	clear_screen();
	srand(time(NULL));
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, KeyListener, NULL);
	term_mode(1);

Restart_Game:
	game_init();
	if (!game_start)
		kprint_at((WIDTH - 11) / 2, HEIGHT / 2, "Start Game", LIGHT_GRAY);
	game_start = true;

	if (game_restart)
	{
		kprint_at((WIDTH - 9) / 2, (HEIGHT / 2) - 1, "Score:", LIGHT_CYAN);
		kprint_at(((WIDTH - 9) / 2) + 6, (HEIGHT / 2) - 1, Score, WHITE);
		kprint_at((WIDTH - 13) / 2, HEIGHT / 2, "Restart Game", LIGHT_GRAY);
	}
	game_restart = false;

	if (game_over)
	{
		kprint_at((WIDTH - 9) / 2, (HEIGHT / 2) - 1, "Score:", LIGHT_CYAN);
		kprint_at(((WIDTH - 9) / 2) + 6, (HEIGHT / 2) - 1, Score, WHITE);
		kprint_at((WIDTH - 10) / 2, HEIGHT / 2, "Game Over", LIGHT_GRAY);
	}
	game_over = false;

	for (int i = 0; i < 3; i++)
		Score[i] = '0';
	int loop_step = 0;
	kprint_at((WIDTH - 33) / 2, 1, "180101012 - Ibrahim Yusuf Cosgun", DARK_GRAY);
Pause_Game:
	if (game_pause)
		kprint_at((WIDTH - 14) / 2, HEIGHT / 2, "Continue Game", LIGHT_GRAY);
	game_pause = false;

	kprint_at((WIDTH - 14) / 2, (HEIGHT / 2) + 1, "Press [Enter]", DARK_GRAY);
	print_screen();
	while (current_key != '\n' || !key_pressed)
		;
	clear_buffers();
	clear_screen();
	while (1)
	{
		if (game_restart || game_over)
			goto Restart_Game;
		if (game_pause)
		{
			clear_screen();
			goto Pause_Game;
		}

		usleep(35 * 1000);
		clear_screen();
		draw_space_ship(false);
		draw_bullets(false);
		draw_asteroids(false);

		move_and_fire_space_ship();
		move_bullets();
		if (loop_step++ % 75 == 0)
		{
			spawn_asteroids();
		}
		if (loop_step % 15 == 0)
		{
			move_asteroids();
		}
		collision_event();

		draw_space_ship(true);
		draw_bullets(true);
		draw_asteroids(true);
		print_score();
		print_life();
		print_screen();

		// for (int i = 0; i < HEIGHT; i++)
		// {
		// 	for (int j = 0; j < WIDTH; j++)
		// 	{
		// 		// ANSI kaçış dizilerini kullanarak renkli veya biçimlendirilmiş metin yazdır
		// 		// Örneğin, kırmızı bir metin için "\033[1;31m" kullanabilirsiniz
		// 		printf("%c", screen_buffer[i * WIDTH + j]);
		// 	}
		// 	printf("\n");
		// }
	}
}
