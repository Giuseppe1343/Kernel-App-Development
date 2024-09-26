#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

#define HEIGHT 25
#define WIDTH 80
#define SCREENSIZE WIDTH *HEIGHT // same screen size to kernel

typedef enum
{
  false,
  true
} bool; // define bool as enum to increase readability

const char *color_array[] = { // The part of the Ansi escape sequence that changes for color is \033[.;..m
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

// These definitions were used to call a color in the color array by its name. It increases readability.
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

/* I imagined a structure that requires a total of 8 chars for each
character on the screen to keep the colors in the buffer.
 Apart from this, I added a 1-char field to the end of each line and
 added '\n' to that field so that it moves to the next line.*/
char screen_buffer[SCREENSIZE * 8 + HEIGHT];

// Create global variables
struct Space_Ship space_ship;
struct Bullet bullet;
struct Asteroid asteroids[NUMBER_OF_ASTEROIDS];

char current_key;
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
  space_ship.x = (WIDTH - 5) / 2; // pos = middle bottom
  space_ship.y = HEIGHT - 2;
}

void init_asteroids() // Function that clears and initializes the asteroids[]
{
  for (int i = 0; i < NUMBER_OF_ASTEROIDS; i++)
  {
    asteroids[i].active = false; // Disable all asteroids so they can be (re)spawn
  }
}

void kprint_at(int x, int y, const char *str, int color) // Function that places specific string and color at specific x and y coordinates into screen_buffer
{
  while (*str != '\0') // For each character
  {
    int index = (((y * WIDTH) + x) * 8) + 2 + y;
    //['\033'] ['['] [ ] [ ] [ ] [ ] ['m'] [ ] Since we do not want to change the first 2 characters, we reach the beginning of the changing part of the color code with + 2.
    const char *color_adress = color_array[color];
    while (*color_adress != '\0') // For the character of the color of each character
    {
      screen_buffer[index++] = *color_adress++;
    }
    screen_buffer[++index /*skip 'm'*/] = *str++; // After adding the colors, we skip 1 character and write our character in the char field before processing.
    x++;
  }
}

void clear_buffer() // Function that clears and initializes the screen_buffer
{
  for (int y = 0; y < HEIGHT; y++)
  {
    for (int x = 0; x < WIDTH; x++)
    {
      int index = ((y * WIDTH) + x) * 8 + y;
      //['\033'] ['['] ['0'] [';'] ['3'] ['0'] ['m'] [' '] every char on screen contains 8 char for colorization
      screen_buffer[index++] = '\033';
      screen_buffer[index++] = '[';
      screen_buffer[index++] = '0';
      screen_buffer[index++] = ';';
      screen_buffer[index++] = '3';
      screen_buffer[index++] = '0';
      screen_buffer[index++] = 'm';
      screen_buffer[index] = ' ';
    }
    screen_buffer[((y * WIDTH) + WIDTH) * 8 + y] = '\n'; // If we reached the end of the line  ['\033'] ['['] ['0'] [';'] ['3'] ['0'] ['m'] [' ']  We add 1 char ['\n'] to the end of this structure
  }
}

void clear_screen() // Function that clears the screen
{
  printf("\033[2J\033[H"); // Ansii escape code that clears the entire screen
}

void game_init() // Function that initializes and resets all game variables
{
  init_space_ship();
  init_asteroids();
  bullet.active = false;
  clear_screen();
  clear_buffer();
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
  kprint_at(WIDTH - 9, 0, "Score:", LIGHT_CYAN);
  kprint_at(WIDTH - 3, 0, Score, WHITE);
}

void print_life() // Function that writes the life in the upper left corner of the screen
{
  char life_char[2] = {(char)life + '0', '\0'};
  kprint_at(0, 0, "Life:", LIGHT_GREEN);
  kprint_at(5, 0, life_char, WHITE);
}

void move_and_fire_space_ship() // Function that takes keyboard inputs and determines the movements of the spaceship and the starting point of the bullet according to these inputs
{
  if (key_pressed) // If the same or a different key is pressed again
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
      asteroids[i].x_start = rand() % (WIDTH - 9 - 1) + 1; // 1 - (Width-9) range
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
        if (asteroids[i].phase < 3)   // If the size of asteroid is not the smalles, reduce the size
        {
          asteroids[i].phase++; // increasing phase means decreasing size
          asteroids[i].x_start++;
        }
        else // If it becomes the smallest, deactivate it
          asteroids[i].active = false;
      }
      if (asteroids[i].y_end > HEIGHT - 3) // If it reaches the bottom of the screen (below the spaceship), lose life and deactivate.
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

void *KeyListener(void *vargp) // Keyboard listener thread used instead of the keyboard_handler_main method in the kernel
{
  while (true)
  {
    scanf("%c", &current_key); // Waiting user input (NonBlocking because thread wait.)
    key_pressed = 1;           // It activates this flag instead of interrupt, thus interaction is made.
  }
  return NULL;
}

void init_term_mode() // Function that change terminal mode
{
  static struct termios origt, newt;

  tcgetattr(STDIN_FILENO, &origt);
  newt = origt;
  newt.c_lflag &= ~(ICANON | ECHO);
  /*A bit operation is performed that inverts the ICANON (disable canonical mode) and ECHO (disable echo) flags of the terminal we just created.
  This prevents the input from being processed on a character-by-character basis and prevents the user from seeing what he entered.*/
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void print_screen() // Function that prints the buffer to the screen
{
  printf("%s", screen_buffer);
}

void main(void)
{
  srand(time(NULL));
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, KeyListener, NULL);
  init_term_mode(); // disable ICANON ECHO

Restart_Game:
  game_init(); // Init and Reset game variables

  if (!game_start)
    kprint_at((WIDTH - 11) / 2, HEIGHT / 2, "Start Game", LIGHT_GRAY); // Appears only on first startup
  game_start = true;

  if (game_restart)
  { // Appears on every restart
    kprint_at((WIDTH - 9) / 2, (HEIGHT / 2) - 1, "Score:", LIGHT_CYAN);
    kprint_at(((WIDTH - 9) / 2) + 6, (HEIGHT / 2) - 1, Score, WHITE);
    kprint_at((WIDTH - 13) / 2, HEIGHT / 2, "Restart Game", LIGHT_GRAY);
  }
  game_restart = false;

  if (game_over)
  { // Appears on every gameover
    kprint_at((WIDTH - 9) / 2, (HEIGHT / 2) - 1, "Score:", LIGHT_CYAN);
    kprint_at(((WIDTH - 9) / 2) + 6, (HEIGHT / 2) - 1, Score, WHITE);
    kprint_at((WIDTH - 10) / 2, HEIGHT / 2, "Game Over", LIGHT_GRAY);
  }
  game_over = false;

  for (int i = 0; i < 3; i++)
    Score[i] = '0';  // Clear Score Array
  int loop_step = 0; // We create(reset) the loop counter to create delay.

  kprint_at((WIDTH - 33) / 2, 1, "180101012 - Ibrahim Yusuf Cosgun", DARK_GRAY);

Pause_Game:
  if (game_pause)
    kprint_at((WIDTH - 14) / 2, HEIGHT / 2, "Continue Game", LIGHT_GRAY); // Appears every pause
  game_pause = false;

  kprint_at((WIDTH - 14) / 2, (HEIGHT / 2) + 1, "Press [Enter]", DARK_GRAY); // Appears always
  print_screen();

  while (current_key != '\n' || !key_pressed) // Wait for [Enter] Key
    ;
  clear_buffer(); // Reset and Inıt Buffer
  clear_screen(); // Reset visible screen
  while (1)
  {
    if (game_restart || game_over) // Proper reset with goto in case of gameover or restart of the game
      goto Restart_Game;
    if (game_pause) // Pause by just freezing the screen without resetting game objects (Buffer is not a game object, it makes the printing process faster and in one piece.)
    {
      clear_screen();
      goto Pause_Game;
    }

    usleep(35 * 1000); // Sleep 35ms

    // Clear
    clear_screen();

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
    
    print_screen();
  }
}
