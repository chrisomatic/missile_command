#define WIN32_LEAN_AND_MEAN
#define GLOBAL_FONT_SCALE 2.0f
#define MAX_ITEMS 10

#include <windows.h>
#include <windowsx.h> // for GET_X_LPARAM,GET_Y_LPARAM

#include <math.h>     // for abs,atan,cos,sin
#include <time.h>     // for time function
#include "common.h"
#include "cdraw.h"
#include "cutil.h"
#include "cfontsimple.h"

const char* CLASS_NAME = "MC";

WNDCLASSEX wc;
HDC dc;
HWND main_window;

typedef struct
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          acolors[256];
} dibinfo_t;

dibinfo_t bmi = { 0 };

BOOL is_gameover;
BOOL is_running;
BOOL is_paused;
BOOL is_game_won;

POINT curr_pt = {0};
POINT prev_pt = {0};

int buffer_width;
int buffer_height;

int window_width;
int window_height;

const int BYTES_PER_PIXEL = 1;

// colors
char COLOR_BLACK  = 0x00;
char COLOR_WHITE  = 0x01;
char COLOR_RED    = 0x02;
char COLOR_GREEN  = 0x03;
char COLOR_BLUE   = 0x04;
char COLOR_GREY   = 0x05;
char COLOR_PURPLE = 0x06;

typedef enum
{
    PLAYING
    ,TITLE
} GAME_STATE;
GAME_STATE game_state;

int title_selection = 0;

typedef enum
{
	A,
	B,
	C
} MISSILE_BASE;

// structs
typedef struct
{
    float location_x;
	float location_y;
    float destination_x;
	float destination_y;
	double vel_x;
	double vel_y;
	double angle;
	float base_x;
	float base_y;
    float health;
} Missile;

typedef struct
{
	float location_x;
	float location_y;
	float radius;
    float max_radius;
	int   dir;
} Explosion;

typedef struct
{
    int   missile_count;
    int   missile_period;
    int   explosion_radius;
    int   missile_health;
    float missile_speed;
    float missile_size;
} Wave;
Wave waves[11];

// powerups

typedef enum
{
	MEGA_EXPLOSIONS  = 1
	,FASTER_MISSILES = 2
	,INFINITE_AMMO   = 4
	,GUIDED_MISSILES = 8
} POWERUPTYPE;

unsigned char player_powerups = 0x00; // 0000 0000

typedef struct
{
    float location_x;
    float location_y;
    char  c;
    POWERUPTYPE type;
} Powerup;

Powerup powerup;

int POWERUPCOUNTER_MAX    = 1500;
int POWERUPWANDERTIME_MAX = 300;

int powerup_counter;
int powerup_wandertime_counter;

BOOL draw_powerup = FALSE;

// items
typedef enum
{
    EXTRA_HOUSE = 1
    ,CLEAR_SCREEN = 2
    ,PLUS_MISSILES = 4
} ITEMTYPE;

ITEMTYPE items[MAX_ITEMS];

BOOL mouse_moved = FALSE;

int current_wave;
BOOL show_wave_text;
int show_wave_text_countdown;

int enemy_missiles_fired;
int enemy_missiles_blown_up;
int enemy_missiles_destroyed_this_wave;

Missile   player_missiles[500];
Missile   enemy_missiles [500];
Explosion explosions     [500];

const int HOUSE_WIDTH = 50;

int CURSOR_RADIUS = 5;
int PLAYER_EXPLOSION_MAX_RADIUS;

int AVAILABLE_MISSILES;
int NUM_HOUSES;

float PLAYER_MISSILE_SPEED;

int missile_count_player;
int missile_count_enemy;
int explosion_count;
int houses[6];

int enemy_missile_counter;

int PLAYER_SCORE;

double TARGET_FPS = 100.0f;

float MISSILE_BASE_A_SRC_X;
float MISSILE_BASE_B_SRC_X;
float MISSILE_BASE_C_SRC_X;
float MISSILE_BASE_SRC_Y;

// prototypes
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);
static void setup_window(HINSTANCE hInstance);
static void update_scene();
static void draw_scene();
static void add_missile(MISSILE_BASE _base);
static void remove_missile(int i);
static void add_explosion(float location_x, float location_y,float max_radius);
static void remove_explosion(int i);
static void add_enemy_missile();
static void remove_enemy_missile(int i);
static void init_missiles();
static void init_houses();
static void init_waves();
static void begin_new_game();
static void remove_house(int i);
static void apply_powerups();
static BOOL set_working_directory();

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpcmdline, s32 nshowcmd)
{
	set_working_directory();
	setup_window(hinstance);
    init_font("font.png");

    game_state = PLAYING;
    begin_new_game();

	srand(time(NULL));

    // init_timer
	LARGE_INTEGER freq;
	LARGE_INTEGER t0, t1;

	QueryPerformanceFrequency(&freq);

	double seconds_per_tick = 1.0f / (double)freq.QuadPart;
	double target_spf = 1.0f / TARGET_FPS;
	double accum_time = 0.0;

	QueryPerformanceCounter(&t0);
    // end
    
	MSG msg;
	while (is_running)
	{
        // @MainLoop
		while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
        
        // timer query
		QueryPerformanceCounter(&t1);
		__int64 interval = t1.QuadPart - t0.QuadPart;
		double seconds_gone_by = seconds_per_tick * (double)interval;

		accum_time += seconds_gone_by;

		if (accum_time >= target_spf)
		{
			accum_time -= target_spf;

            update_scene();
            draw_scene();

            // Blit to screen
            StretchDIBits(dc, 0, 0, window_width, window_height, 0, 0, window_width, window_height, back_buffer, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY);
        }

		QueryPerformanceCounter(&t0);
		//Sleep(1);	// @NOTE: to prevent CPU spending to much time in this process
	}

	free_font();
	//free(back_buffer);
	ReleaseDC(main_window, dc);

	return EXIT_SUCCESS;
}

static void update_scene()
{
    switch(game_state)
    {
        case TITLE:
        {
            //
        } break;

        case PLAYING: 
        {
            if (is_paused)
                return;

            if (show_wave_text)
            {
                show_wave_text_countdown--;

                if(show_wave_text_countdown == 0)
                {
                    show_wave_text = FALSE;
                    enemy_missiles_fired = 0;
                    enemy_missiles_blown_up = 0;
                    enemy_missiles_destroyed_this_wave = 0;

                }
            }
            
            // update player missiles
            for (int i = 0; i < missile_count_player; ++i)
            {
                if (player_missiles[i].location_y <= player_missiles[i].destination_y)
                {
                    add_explosion(player_missiles[i].location_x, player_missiles[i].location_y,(float)PLAYER_EXPLOSION_MAX_RADIUS);
                    remove_missile(i);
                }
                player_missiles[i].location_x += player_missiles[i].vel_x * PLAYER_MISSILE_SPEED;
                player_missiles[i].location_y -= player_missiles[i].vel_y * PLAYER_MISSILE_SPEED;
            }

            // update enemy missiles
            enemy_missile_counter++;

            if(enemy_missile_counter >= waves[current_wave].missile_period)
            {
                if(enemy_missiles_fired < waves[current_wave].missile_count)
                {
                    add_enemy_missile();
                }

                enemy_missile_counter = 0;
            }

            for (int i = missile_count_enemy - 1; i >= 0; --i)
            {
                if (enemy_missiles[i].location_y >= enemy_missiles[i].destination_y)
                {
                    add_explosion(enemy_missiles[i].location_x, enemy_missiles[i].location_y,waves[current_wave].explosion_radius);

                    // remove enemy missile
                    remove_enemy_missile(i);

                }
                enemy_missiles[i].location_x += enemy_missiles[i].vel_x * waves[current_wave].missile_speed;
                enemy_missiles[i].location_y += enemy_missiles[i].vel_y * waves[current_wave].missile_speed;
            }

            // update powerups
            if(draw_powerup)
            {
                powerup.location_x += cos(powerup_wandertime_counter);
                powerup.location_y += sin(powerup_wandertime_counter);
            }

            powerup_counter--;
            if(powerup_counter == 0)
            {
                // initialize powerup
                powerup_counter = POWERUPCOUNTER_MAX;

                powerup.location_x = rand() % buffer_width;
                powerup.location_y = rand() % buffer_height;
                powerup.type = (POWERUPTYPE)(1 << rand() % 4); 

                switch (powerup.type)
                {
                    case MEGA_EXPLOSIONS: powerup.c = CHAR_MISSILE_M; break;
                    case FASTER_MISSILES: powerup.c = CHAR_FLAME; break;
                    case INFINITE_AMMO:   powerup.c = CHAR_INFINITY; break;
                    case GUIDED_MISSILES: powerup.c = CHAR_SHIELD; break; 
                }

                draw_powerup = TRUE;
                powerup_wandertime_counter = POWERUPWANDERTIME_MAX;
            }

            // update explosions
            for (int i = explosion_count -1; i >= 0; --i)
            {
                // check for collision with enemy missiles
                int missiles_to_remove_count = 0;
                int missiles_to_remove[100];

                for (int j = 0; j < missile_count_enemy; ++j)
                {
                    double d1 = get_distance(enemy_missiles[j].location_x, enemy_missiles[j].location_y, explosions[i].location_x, explosions[i].location_y);
                    double d2 = get_distance(enemy_missiles[j].location_x + GLYPH_WIDTH*GLOBAL_FONT_SCALE, enemy_missiles[j].location_y + GLYPH_HEIGHT*GLOBAL_FONT_SCALE, explosions[i].location_x, explosions[i].location_y);
                    double d3 = get_distance(enemy_missiles[j].location_x + GLYPH_WIDTH*GLOBAL_FONT_SCALE, enemy_missiles[j].location_y, explosions[i].location_x, explosions[i].location_y);
                    double d4 = get_distance(enemy_missiles[j].location_x, enemy_missiles[j].location_y + GLYPH_HEIGHT*GLOBAL_FONT_SCALE, explosions[i].location_x, explosions[i].location_y);

                    if (d1 <= explosions[i].radius || d2 <= explosions[i].radius || d3 <= explosions[i].radius || d4 <= explosions[i].radius)
                    {
                        enemy_missiles[j].health--;
                        if(enemy_missiles[j].health == 0)
                        {
                            missiles_to_remove[missiles_to_remove_count++] = j;
                        }
                    }
                }

                // remove collided missiles!
                for (int j = 0; j < missiles_to_remove_count; ++j)
                {
                    remove_enemy_missile(missiles_to_remove[j]);
                    enemy_missiles_destroyed_this_wave++;

                    if(!is_gameover)
                        PLAYER_SCORE += 100;
                }

                // check for collision with powerups
                if (draw_powerup)
                {
                    double d1 = get_distance(powerup.location_x, powerup.location_y, explosions[i].location_x, explosions[i].location_y);
                    double d2 = get_distance(powerup.location_x + GLYPH_WIDTH*GLOBAL_FONT_SCALE, powerup.location_y + GLYPH_HEIGHT*GLOBAL_FONT_SCALE, explosions[i].location_x, explosions[i].location_y);
                    double d3 = get_distance(powerup.location_x + GLYPH_WIDTH*GLOBAL_FONT_SCALE, powerup.location_y, explosions[i].location_x, explosions[i].location_y);
                    double d4 = get_distance(powerup.location_x, powerup.location_y + GLYPH_HEIGHT*GLOBAL_FONT_SCALE, explosions[i].location_x, explosions[i].location_y);

                    if(d1 <= explosions[i].radius || d2 <= explosions[i].radius || d3 <= explosions[i].radius || d4 <= explosions[i].radius)
                    {
                        // player gets the powerup
                        draw_powerup = FALSE;
                        player_powerups |= powerup.type;
                        apply_powerups();
                    }
                }
                
                float house_r = HOUSE_WIDTH / 2.0f;

                // check to see if it hit a house
                for(int j = NUM_HOUSES-1; j >= 0;--j)
                {
                    if(get_distance(houses[j], buffer_height -1,explosions[i].location_x, explosions[i].location_y) <= house_r + explosions[i].radius )
                    {
                        remove_house(j);

                        if (NUM_HOUSES == 0)
                            is_gameover = TRUE;
                    }
                }

                if (explosions[i].radius > explosions[i].max_radius)
                {
                    explosions[i].dir *= -1;
                    while (explosions[i].radius > explosions[i].max_radius)
                        explosions[i].radius--;
                }


                if (explosions[i].radius <= 0)
                    remove_explosion(i);

                explosions[i].radius += 3*explosions[i].dir;
            }
                        
        }break;

    }

}

static void draw_scene()
{
    // Clear buffer to bg_color 
    memset(back_buffer,COLOR_BLACK, buffer_width*buffer_height*BYTES_PER_PIXEL);

    switch(game_state)
    {
        case TITLE:
        {
            // title
            draw_string("MISSILE COMMAND",(buffer_width - 15*5*7) / 2,(buffer_height - 6*7) /3, 7.0f, COLOR_BLUE);

            // options
            int x = (buffer_width - 5*5*2)/2;
            int y = (buffer_height - 6*2)/2;

            draw_string("Start",x,y,2.0f,COLOR_WHITE);
            draw_string("MODE:" ,x,y+20,2.0f,COLOR_WHITE);
            draw_string("Exit" ,x,y+40,2.0f,COLOR_WHITE);

            // draw title cursor
            draw_char_scaled(CHAR_MISSILE,x - 12,y+title_selection*20,2.0f,COLOR_WHITE);

        } break;
        case PLAYING:
        {
            if (is_paused)
            {
                draw_string("PAUSED", (buffer_width - 66) / 2, (buffer_height - 12) / 2,GLOBAL_FONT_SCALE, COLOR_WHITE);
                return;
            }

            // draw houses
            for (int i = 0; i < NUM_HOUSES; ++i)
            {
                draw_circle(houses[i],buffer_height -1,HOUSE_WIDTH/2,COLOR_GREY,TRUE);
            } 

            // draw missile bases
            draw_rect8((int)(MISSILE_BASE_A_SRC_X - 10),buffer_height - 20,20,20,COLOR_BLUE,TRUE);
            draw_rect8((int)(MISSILE_BASE_B_SRC_X - 10), buffer_height - 20, 20, 20, COLOR_BLUE, TRUE);
            draw_rect8((int)(MISSILE_BASE_C_SRC_X - 10), buffer_height - 20, 20, 20, COLOR_BLUE, TRUE);

            draw_char_scaled('Z', (int)(MISSILE_BASE_A_SRC_X - GLYPH_WIDTH*GLOBAL_FONT_SCALE / 2), (int)(buffer_height - 20 + GLYPH_HEIGHT*GLOBAL_FONT_SCALE / 2), GLOBAL_FONT_SCALE, COLOR_GREY);
            draw_char_scaled('X', (int)(MISSILE_BASE_B_SRC_X - GLYPH_WIDTH*GLOBAL_FONT_SCALE / 2), (int)(buffer_height - 20 + GLYPH_HEIGHT*GLOBAL_FONT_SCALE / 2), GLOBAL_FONT_SCALE, COLOR_GREY);
            draw_char_scaled('C', (int)(MISSILE_BASE_C_SRC_X - GLYPH_WIDTH*GLOBAL_FONT_SCALE / 2), (int)(buffer_height - 20 + GLYPH_HEIGHT*GLOBAL_FONT_SCALE / 2), GLOBAL_FONT_SCALE, COLOR_GREY);

            // draw missiles
            for(int i = 0; i < missile_count_player; ++i)
            {
                char missile_line_color;
                if((player_powerups & FASTER_MISSILES) == FASTER_MISSILES)
                    missile_line_color = COLOR_PURPLE;
                else
                    missile_line_color = COLOR_BLUE;
                
                draw_line2((int)player_missiles[i].base_x, (int)player_missiles[i].base_y, (int)player_missiles[i].location_x, (int)player_missiles[i].location_y, missile_line_color);
                // draw target crosshair
                draw_pixel8((int)player_missiles[i].destination_x, (int)player_missiles[i].destination_y, COLOR_GREY);
                draw_pixel8((int)player_missiles[i].destination_x - 1, (int)player_missiles[i].destination_y - 1, COLOR_GREY);
                draw_pixel8((int)player_missiles[i].destination_x - 1, (int)player_missiles[i].destination_y + 1, COLOR_GREY);
                draw_pixel8((int)player_missiles[i].destination_x + 1, (int)player_missiles[i].destination_y - 1, COLOR_GREY);
                draw_pixel8((int)player_missiles[i].destination_x + 1, (int)player_missiles[i].destination_y + 1, COLOR_GREY);

                unsigned char missile_char;
                float player_missile_scale;
                char missile_color;

                if((player_powerups & MEGA_EXPLOSIONS) == MEGA_EXPLOSIONS)
                {
                    missile_char = CHAR_MISSILE_M;
                    player_missile_scale = GLOBAL_FONT_SCALE*2;
                    missile_color = COLOR_GREY;
                }
                else
                {
                    missile_char = CHAR_MISSILE;
                    player_missile_scale = GLOBAL_FONT_SCALE;
                    missile_color = COLOR_GREEN;
                }

                draw_char_scaled(missile_char, (int)(player_missiles[i].location_x - GLYPH_WIDTH*GLOBAL_FONT_SCALE / 2), (int)(player_missiles[i].location_y - GLYPH_HEIGHT*GLOBAL_FONT_SCALE / 2), GLOBAL_FONT_SCALE, missile_color);
            }

            if(current_wave == 10)
            {
                for(int i = 0; i < missile_count_enemy; ++i)
                {
                    draw_line2(enemy_missiles[i].base_x, enemy_missiles[i].base_y, enemy_missiles[i].location_x, enemy_missiles[i].location_y, COLOR_RED);
                    draw_char_scaled(CHAR_MISSILE_R,enemy_missiles[i].location_x - GLYPH_WIDTH*10.0f /2 ,enemy_missiles[i].location_y - GLYPH_HEIGHT*10.0f / 2,10.0f,COLOR_RED);  // @TEMP
                }

            }
            else
            {
                for(int i = 0; i < missile_count_enemy; ++i)
                {
                    draw_line2(enemy_missiles[i].base_x, enemy_missiles[i].base_y, enemy_missiles[i].location_x, enemy_missiles[i].location_y, COLOR_RED);
                    draw_char_scaled(CHAR_MISSILE_R,enemy_missiles[i].location_x - GLYPH_WIDTH*GLOBAL_FONT_SCALE /2 ,enemy_missiles[i].location_y - GLYPH_HEIGHT*GLOBAL_FONT_SCALE / 2,GLOBAL_FONT_SCALE,COLOR_RED);  // @TEMP
                }
            }

            // draw powerup(s)
            if(draw_powerup)
            {
                draw_char_scaled(powerup.c,powerup.location_x,powerup.location_y,GLOBAL_FONT_SCALE,COLOR_PURPLE);

                powerup_wandertime_counter--;
                if(powerup_wandertime_counter == 0)
                    draw_powerup = FALSE;
            }

            // draw powerup icons
            if (player_powerups > 0x00)
            {
                if((player_powerups & MEGA_EXPLOSIONS) == MEGA_EXPLOSIONS)
                {
                    draw_char_scaled(CHAR_MISSILE_M,buffer_width-GLYPH_WIDTH*GLOBAL_FONT_SCALE-2,GLYPH_HEIGHT*GLOBAL_FONT_SCALE*2 + 2,GLOBAL_FONT_SCALE,COLOR_WHITE);
                }
                if((player_powerups & FASTER_MISSILES) == FASTER_MISSILES)
                {
                    draw_char_scaled(CHAR_FLAME,buffer_width-2*GLYPH_WIDTH*GLOBAL_FONT_SCALE-4,GLYPH_HEIGHT*GLOBAL_FONT_SCALE*2 + 2,GLOBAL_FONT_SCALE,COLOR_WHITE);
                }
                if((player_powerups & INFINITE_AMMO) == INFINITE_AMMO)
                {
                    draw_char_scaled(CHAR_INFINITY,buffer_width-3*GLYPH_WIDTH*GLOBAL_FONT_SCALE-6,GLYPH_HEIGHT*GLOBAL_FONT_SCALE*2 + 2,GLOBAL_FONT_SCALE,COLOR_WHITE);
                }
                if((player_powerups & GUIDED_MISSILES) == GUIDED_MISSILES)
                {
                    draw_char_scaled(CHAR_SHIELD,buffer_width-4*GLYPH_WIDTH*GLOBAL_FONT_SCALE-8,GLYPH_HEIGHT*GLOBAL_FONT_SCALE*2 + 2,GLOBAL_FONT_SCALE,COLOR_WHITE);
                }
            }
            
            // draw explosions
            for (int i = 0; i < explosion_count; ++i)
            {
                draw_circle(explosions[i].location_x, explosions[i].location_y,explosions[i].radius, rand()%256, TRUE);
            }
            
            //draw hud
            draw_string("SCORE:", 2, 2, GLOBAL_FONT_SCALE, COLOR_WHITE);
            char* pscore_str = to_string(PLAYER_SCORE);
            draw_string(pscore_str,66,2,GLOBAL_FONT_SCALE, COLOR_GREEN);
            free(pscore_str);

            draw_string("WAVE:", 2, 18, GLOBAL_FONT_SCALE, COLOR_WHITE);
            char* current_wave_str = to_string(current_wave + 1);
            draw_string(current_wave_str,55,18, GLOBAL_FONT_SCALE, COLOR_GREEN);
            free(current_wave_str);

            //draw_string("Z,X,C to shoot", 2, 34, COLOR_WHITE);
            draw_string("P to pause",   2, 34, GLOBAL_FONT_SCALE, COLOR_WHITE);
            draw_string("R to restart", 2, 50, GLOBAL_FONT_SCALE, COLOR_WHITE);

            draw_string("MISSILES:", buffer_width - 146, 2,GLOBAL_FONT_SCALE,COLOR_WHITE);
            char* available_missiles_str = to_string(AVAILABLE_MISSILES);
            draw_string(available_missiles_str,buffer_width - 48,2,GLOBAL_FONT_SCALE,COLOR_GREEN);
            free(available_missiles_str);

            // draw new wave text
            if (show_wave_text)
            {
                draw_string("WAVE ",(buffer_width - 66) / 2, (buffer_height - 12) / 2,GLOBAL_FONT_SCALE, COLOR_WHITE);
                draw_string(to_string(current_wave + 1), (buffer_width - 66) / 2 + 55, (buffer_height - 12) / 2,GLOBAL_FONT_SCALE, COLOR_WHITE);
            }
            
            if (is_gameover)
                draw_string("GAME OVER", (buffer_width - 99) / 2, (buffer_height - 12) / 2,GLOBAL_FONT_SCALE, COLOR_WHITE);
            else if (is_game_won)
                draw_string("YOU WIN!", (buffer_width - 88) / 2, (buffer_height - 12) / 2,GLOBAL_FONT_SCALE, COLOR_WHITE);

            // draw cursor
            draw_line2(curr_pt.x - CURSOR_RADIUS, curr_pt.y, curr_pt.x + CURSOR_RADIUS, curr_pt.y, COLOR_GREEN);
            draw_line2(curr_pt.x, curr_pt.y - CURSOR_RADIUS, curr_pt.x, curr_pt.y + CURSOR_RADIUS, COLOR_GREEN);

        } break;
    }
    
}
static void init_missiles()
{
	MISSILE_BASE_A_SRC_X =   buffer_width / 8;
	MISSILE_BASE_B_SRC_X =   buffer_width / 2;
	MISSILE_BASE_C_SRC_X = 7*buffer_width / 8;
	MISSILE_BASE_SRC_Y   =   buffer_height - 1;
}

static BOOL set_working_directory()
{
	char cwd[256] = { 0 };
    char curr_path[256] = {0};
    GetModuleFileName(NULL, curr_path, 256);

	for (int i = 255; curr_path[i] != '\\' && i > 0; --i)
		curr_path[i] = 0;

    if(!curr_path)
        return FALSE;

    if(!SetCurrentDirectory(curr_path))
		return FALSE;

	return TRUE;
}

static void begin_new_game()
{
    is_gameover = FALSE;
    is_running  = TRUE;
    is_game_won = FALSE;

    AVAILABLE_MISSILES = 25;
    NUM_HOUSES         = 6;
    PLAYER_SCORE       = 0;

    PLAYER_MISSILE_SPEED = 4;

    missile_count_player = 0;
    missile_count_enemy  = 0;
    explosion_count = 0;
    current_wave = 0;
    show_wave_text = TRUE;
    show_wave_text_countdown = 150;

    enemy_missile_counter = 0;
    enemy_missiles_fired  = 0;
    enemy_missiles_blown_up = 0;

    PLAYER_EXPLOSION_MAX_RADIUS = 60;

	powerup_counter = POWERUPCOUNTER_MAX;
    player_powerups = 0x00;
    apply_powerups();

    init_missiles();
    init_houses();
    init_waves();
}

static void init_houses()
{
    houses[0] = 1*buffer_width/6 - 3*HOUSE_WIDTH/2;
    houses[1] = 2*buffer_width/6 - 3*HOUSE_WIDTH/2;
    houses[2] = 3*buffer_width/6 - 3*HOUSE_WIDTH/2;
    houses[3] = 4*buffer_width/6 - 3*HOUSE_WIDTH/2;
    houses[4] = 5*buffer_width/6 - 3*HOUSE_WIDTH/2; 
    houses[5] = 6*buffer_width/6 - 3*HOUSE_WIDTH/2;
}

static void init_waves()
{
    waves[0].missile_count = 10;
    waves[0].missile_speed = 0.7f;
    waves[0].missile_period = 100;
    waves[0].missile_size  = 2.0f;
    waves[0].explosion_radius = 60;
    waves[0].missile_health = 1;

    waves[1].missile_count = 15;
    waves[1].missile_speed = 1.0f;
    waves[1].missile_period = 90;
    waves[1].missile_size  = 2.0f;
    waves[1].explosion_radius = 60;
    waves[1].missile_health = 1;

    waves[2].missile_count = 20;
    waves[2].missile_speed = 1.3f;
    waves[2].missile_period = 80;
    waves[2].missile_size  = 2.0f;
    waves[2].explosion_radius = 60;
    waves[2].missile_health = 1;
    
    waves[3].missile_count = 25;
    waves[3].missile_speed = 1.6f;
    waves[3].missile_period = 70;
    waves[3].missile_size  = 2.0f;
    waves[3].explosion_radius = 60;
    waves[3].missile_health = 1;
    
    waves[4].missile_count = 30;
    waves[4].missile_speed = 2.0f;
    waves[4].missile_period = 60;
    waves[4].missile_size  = 2.0f;
    waves[4].explosion_radius = 60;
    waves[4].missile_health = 1;
    
    waves[5].missile_count = 35;
    waves[5].missile_speed = 2.3f;
    waves[5].missile_period = 50;
    waves[5].missile_size  = 2.0f;
    waves[5].explosion_radius = 60;
    waves[5].missile_health = 1;
    
    waves[6].missile_count = 40;
    waves[6].missile_speed = 2.6f;
    waves[6].missile_period = 40; 
    waves[6].missile_size  = 2.0f;
    waves[6].explosion_radius = 60;
    waves[6].missile_health = 1;
    
    waves[7].missile_count = 50;
    waves[7].missile_speed = 3.0f;
    waves[7].missile_period = 30;
    waves[7].missile_size  = 2.0f;
    waves[7].explosion_radius = 60;
    waves[7].missile_health = 1;
    
    waves[8].missile_count = 60;
    waves[8].missile_speed = 3.3f;
    waves[8].missile_period = 20;
    waves[8].missile_size  = 2.0f;
    waves[8].explosion_radius = 60;
    waves[8].missile_health = 1;

    waves[9].missile_count = 70;
    waves[9].missile_speed = 3.6f;
    waves[9].missile_period = 10;
    waves[9].missile_size  = 2.0f;
    waves[9].explosion_radius = 60;
    waves[9].missile_health = 1;

    // last boss
    waves[10].missile_count = 1;
    waves[10].missile_speed = 0.5f;
    waves[10].missile_period = 1;
    waves[10].missile_size  = 10.0f;
    waves[10].explosion_radius = 1000;
    waves[10].missile_health = 5000;

}
static void remove_missile(int i)
{
	player_missiles[i] = player_missiles[missile_count_player - 1];
	missile_count_player--;
}

static void remove_house(int i)
{
	houses[i] = houses[NUM_HOUSES - 1];
	NUM_HOUSES--;
}

static void remove_explosion(int i)
{
	explosions[i] = explosions[explosion_count - 1];
	explosion_count--;
}

static void apply_powerups()
{
    // set default parameters
    PLAYER_EXPLOSION_MAX_RADIUS = 60;
    PLAYER_MISSILE_SPEED = 4;
    
    // add powerups
    if (player_powerups > 0x00)
    {
        if((player_powerups & MEGA_EXPLOSIONS) == MEGA_EXPLOSIONS)
        {
            PLAYER_EXPLOSION_MAX_RADIUS = 120;
        }
        if((player_powerups & FASTER_MISSILES) == FASTER_MISSILES)
        {
            PLAYER_MISSILE_SPEED = 8;
        }
        if((player_powerups & INFINITE_AMMO) == INFINITE_AMMO)
        {
            AVAILABLE_MISSILES = 999;
        }
        if((player_powerups & GUIDED_MISSILES) == GUIDED_MISSILES)
        {
            // @TODO
        }
    }

}

static void add_missile(MISSILE_BASE _base)
{
    if(AVAILABLE_MISSILES == 0)
        return;

	float src_x = 0.0f;
	float src_y = MISSILE_BASE_SRC_Y;

	switch (_base)
	{
		case A: src_x = MISSILE_BASE_A_SRC_X; break;
		case B: src_x = MISSILE_BASE_B_SRC_X; break;
		case C: src_x = MISSILE_BASE_C_SRC_X; break;
	}
	
	float dst_x = (float)curr_pt.x;
	float dst_y = (float)curr_pt.y;

	float delta_x = (float)abs(dst_x - src_x);
	float delta_y = (float)abs(dst_y - src_y);

	double angle = atan(delta_y / delta_x);
	double vel_x = cos(angle);
	double vel_y = sin(angle);
	
	if (dst_x < src_x) vel_x *= -1;

	player_missiles[missile_count_player].base_x = src_x;
	player_missiles[missile_count_player].base_y = src_y;
	player_missiles[missile_count_player].location_x = src_x;
	player_missiles[missile_count_player].location_y = src_y;
	player_missiles[missile_count_player].destination_x = dst_x;
	player_missiles[missile_count_player].destination_y = dst_y;
	player_missiles[missile_count_player].angle = angle;
	player_missiles[missile_count_player].vel_x = vel_x;
	player_missiles[missile_count_player].vel_y = vel_y;
    
    missile_count_player++;
    AVAILABLE_MISSILES--;
}

static void add_enemy_missile()
{
    if(show_wave_text)
        return;

    int src_x = rand() % (buffer_width - 10) + 5;
    int dst_x = rand() % (buffer_width - 10) + 5;

    int src_y = 0;
    int dst_y = buffer_height - 16;

	float delta_x = (float)abs(dst_x - src_x);
	float delta_y = (float)abs(dst_y - src_y);

	double angle = atan(delta_y / delta_x);
	double vel_x = cos(angle);
	double vel_y = sin(angle);
	
	if (dst_x < src_x) vel_x *= -1;

	enemy_missiles[missile_count_enemy].base_x = src_x;
	enemy_missiles[missile_count_enemy].base_y = src_y;
	enemy_missiles[missile_count_enemy].location_x = src_x;
	enemy_missiles[missile_count_enemy].location_y = src_y;
	enemy_missiles[missile_count_enemy].destination_x = dst_x;
	enemy_missiles[missile_count_enemy].destination_y = dst_y;
	enemy_missiles[missile_count_enemy].angle = angle;
	enemy_missiles[missile_count_enemy].vel_x = vel_x;
	enemy_missiles[missile_count_enemy].vel_y = vel_y;
    enemy_missiles[missile_count_enemy].health = waves[current_wave].missile_health;

    missile_count_enemy++;
    enemy_missiles_fired += 1;
}

static void remove_enemy_missile(int i)
{
	enemy_missiles[i] = enemy_missiles[missile_count_enemy - 1];
	missile_count_enemy--;
    enemy_missiles_blown_up++;

    if(enemy_missiles_blown_up >= waves[current_wave].missile_count)
    {
        //move to next wave
        if(current_wave == 10)
        {
            // win the game
            is_game_won = TRUE;
        }
        else
        {
            show_wave_text = TRUE;
            show_wave_text_countdown = 300;
            current_wave++;
            AVAILABLE_MISSILES += (enemy_missiles_destroyed_this_wave*2); 
        }
    }
}

static void add_explosion(float location_x, float location_y, float max_radius)
{
	explosions[explosion_count].location_x = location_x;
	explosions[explosion_count].location_y = location_y;
	explosions[explosion_count].radius     = 1;
	explosions[explosion_count].dir        = 1;
    explosions[explosion_count].max_radius = max_radius;
	explosion_count++;
}

static void setup_window(HINSTANCE hInstance)
{
	buffer_width = 1024;
	buffer_height = 768;

	window_width = 1024;
	window_height = 768;

	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;
	wc.style = CS_DBLCLKS;
	wc.cbSize = sizeof(WNDCLASSEX);

	//wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	//wc.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wc.lpszMenuName = NULL;
	wc.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(0x0f, 0x0f, 0x0f)));
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;

	if (!RegisterClassExA(&wc))
		return;

	DWORD window_style_ex = 0;
	DWORD window_style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
	
	RECT r = { 0 };

	r.right = window_width;
	r.bottom = window_height;

	// To make the drawable space truly BUFFER_WIDTH & BUFFER_HEIGHT
	AdjustWindowRectEx(&r, window_style, 0, window_style_ex);

	// Get screen dimensions to startup window at center-screen
	RECT w = { 0 };
	GetClientRect(GetDesktopWindow(), &w);

	main_window = CreateWindowEx(
		window_style_ex
		, CLASS_NAME
		, "Missile Command"
		, window_style
		, (w.right / 2) - (window_width / 2)
		, (w.bottom / 2) - (window_height / 2)
		, r.right - r.left
		, r.bottom - r.top
		, NULL
		, NULL
		, hInstance
		, 0
		);

	ShowWindow(main_window, SW_SHOWDEFAULT);

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = buffer_width;
	bmi.bmiHeader.biHeight = -buffer_height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 8;
	bmi.bmiHeader.biCompression = BI_RGB;

	back_buffer = malloc(buffer_width*buffer_height * BYTES_PER_PIXEL);

	// black
    bmi.acolors[0].rgbRed   = 0;
    bmi.acolors[0].rgbGreen = 0; 
    bmi.acolors[0].rgbBlue  = 0;

	// white
	bmi.acolors[1].rgbRed   = 255;
	bmi.acolors[1].rgbGreen = 255;
	bmi.acolors[1].rgbBlue  = 255;

	// red
	bmi.acolors[2].rgbRed   = 255;
	bmi.acolors[2].rgbGreen = 0;
	bmi.acolors[2].rgbBlue  = 0;

	// green
	bmi.acolors[3].rgbRed   = 0;
	bmi.acolors[3].rgbGreen = 255;
	bmi.acolors[3].rgbBlue  = 0;

	// blue
	bmi.acolors[4].rgbRed   = 0;
	bmi.acolors[4].rgbGreen = 0;
	bmi.acolors[4].rgbBlue  = 255;
    
	// grey 
	bmi.acolors[5].rgbRed   = 100;
	bmi.acolors[5].rgbGreen = 100;
	bmi.acolors[5].rgbBlue  = 100;
    
	// purple 
	bmi.acolors[6].rgbRed   = 255;
	bmi.acolors[6].rgbGreen = 0;
	bmi.acolors[6].rgbBlue  = 255;

    for (int i = 7; i < 256; ++i)
    {
        bmi.acolors[i].rgbRed   = rand() % 256;
        bmi.acolors[i].rgbGreen = rand() % 256; 
        bmi.acolors[i].rgbBlue  = rand() % 256;
    }

	dc = GetDC(main_window);

}


static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	switch (umsg)
	{
    
        case WM_LBUTTONDOWN:
        {
            POINT pt;

            pt.x = GET_X_LPARAM(lparam);
            pt.y = GET_Y_LPARAM(lparam);

            break;
        } 
        
        case WM_MOUSEMOVE:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lparam);
            pt.y = GET_Y_LPARAM(lparam);

            prev_pt.x = curr_pt.x;
            prev_pt.y = curr_pt.y;

            curr_pt.x = pt.x;
            curr_pt.y = pt.y;

            mouse_moved = TRUE;
            break;
        } 
        case WM_CLOSE:
        {
            is_running = FALSE;
            PostQuitMessage(0);
            break;
        }
        case WM_DESTROY:
        {
            is_running = FALSE;
            PostQuitMessage(0);
            break;
        }
        case WM_KEYDOWN:
        {
            if(game_state == TITLE)
            {
                if(wparam == VK_UP)
                    title_selection--;
                else if (wparam == VK_DOWN)
                    title_selection++;

                title_selection = max(0,title_selection);
                title_selection = min(2,title_selection);

                if(wparam == VK_RETURN)
                {

                }
            }
            else if(game_state == PLAYING)
            {
                if (wparam == 'R')
                {
                    begin_new_game();
                }
                else if(wparam == 'P')
                {
                    is_paused = !is_paused;
                }

                if (is_gameover) return;

                switch (wparam)
                {
                case 'Z': add_missile(A); break;
                case 'X': add_missile(B); break;
                case 'C': add_missile(C); break;
                }
                break;
            }

            break;
        } 
       
        case WM_SETCURSOR:
        {
            static BOOL HideCursor = FALSE;
            if ((LOWORD(lparam) == HTCLIENT) && !HideCursor)
            {
                HideCursor = TRUE;
                ShowCursor(FALSE);
            }
			else if ((LOWORD(lparam) != HTCLIENT) && HideCursor)
            {
                HideCursor = FALSE;
                ShowCursor(TRUE);
            }
            break;
        } 
        
        default:
        {
            break;
        }
	}

	return DefWindowProc(hwnd, umsg, wparam, lparam);
}
