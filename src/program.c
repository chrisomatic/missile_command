#define WIN32_LEAN_AND_MEAN

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
POINT curr_pt = {0};

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
	float vel_x;
	float vel_y;
	float angle;
	float base_x;
	float base_y;
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
    float missile_speed;
} Wave;
Wave waves[10];

typedef enum
{
	MEGA_EXPLOSIONS   = 1
	,FASTER_MISSILES = 2
	,INFINITE_AMMO   = 4
	,GUIDED_MISSILES = 8
} POWERUPTYPE;

char player_powerups = 0x00;

typedef struct
{
    float location_x;
    float location_y;
    char  c;
    POWERUPTYPE type;
} Powerup;

Powerup powerup;

int POWERUPCOUNTER_MAX = 1000;
int POWERUPLIFETIME_MAX = 500;

int powerup_counter;
int powerup_lifetime_counter;

BOOL draw_powerup = FALSE;

int current_wave;
BOOL show_wave_text;
int show_wave_text_countdown;

int enemy_missiles_fired;
int enemy_missiles_blown_up;

Missile player_missiles[100];
Missile enemy_missiles [100];
Explosion explosions[100];

const int HOUSE_WIDTH = 50;

int PLAYER_EXPLOSION_MAX_RADIUS;
int ENEMY_EXPLOSION_MAX_RADIUS;

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

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR lpcmdline, s32 nshowcmd)
{
	setup_window(hinstance);
    init_font("font12.png");

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
		double  seconds_gone_by = seconds_per_tick * (double)interval;

		accum_time += seconds_gone_by;

		if (accum_time >= target_spf)
		{
			accum_time -= target_spf;

            // Clear buffer to bg_color 
            memset(back_buffer,COLOR_BLACK, buffer_width*buffer_height*BYTES_PER_PIXEL);

            update_scene();
            draw_scene();

            // Blit to screen
            StretchDIBits(dc, 0, 0, window_width, window_height, 0, 0, window_width, window_height, back_buffer, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY);
        }

		QueryPerformanceCounter(&t0);
		Sleep(1);	// @NOTE: to prevent CPU spending to much time in this process
	}

	free_font();
	//free(back_buffer);
	ReleaseDC(main_window, dc);

	return EXIT_SUCCESS;
}

static void update_scene()
{
    if (show_wave_text)
    {
        show_wave_text_countdown--;

        if(show_wave_text_countdown == 0)
        {
            show_wave_text = FALSE;
            enemy_missiles_fired = 0;
            enemy_missiles_blown_up = 0;
        }
    }
    
	// update player missiles
	for (int i = 0; i < missile_count_player; ++i)
	{
		if (player_missiles[i].location_y <= player_missiles[i].destination_y)
		{
			add_explosion(player_missiles[i].location_x, player_missiles[i].location_y,PLAYER_EXPLOSION_MAX_RADIUS);
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
			add_explosion(enemy_missiles[i].location_x, enemy_missiles[i].location_y,ENEMY_EXPLOSION_MAX_RADIUS);


			// remove enemy missile
			remove_enemy_missile(i);

		}
		enemy_missiles[i].location_x += enemy_missiles[i].vel_x * waves[current_wave].missile_speed;
		enemy_missiles[i].location_y += enemy_missiles[i].vel_y * waves[current_wave].missile_speed;
    }

    // update powerups
    if(draw_powerup)
    {
        powerup.location_x += cos(powerup_lifetime_counter);
        powerup.location_y += sin(powerup_lifetime_counter);
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
            case MEGA_EXPLOSIONS: powerup.c = 'M'; break;
            case FASTER_MISSILES: powerup.c = 'F'; break;
            case INFINITE_AMMO:   powerup.c = 'I'; break;
            case GUIDED_MISSILES: powerup.c = 'G'; break; 
        }

        draw_powerup = TRUE;
        powerup_lifetime_counter = POWERUPLIFETIME_MAX;
    }

	// update explosions
	for (int i = explosion_count -1; i >= 0; --i)
    {
        // check for collision with enemy missiles
        int missiles_to_remove_count = 0;
        int missiles_to_remove[100];

        for (int j = 0; j < missile_count_enemy; ++j)
        {
			float d1 = get_distance(enemy_missiles[j].location_x, enemy_missiles[j].location_y, explosions[i].location_x, explosions[i].location_y);
			float d2 = get_distance(enemy_missiles[j].location_x + GLYPH_WIDTH , enemy_missiles[j].location_y + GLYPH_HEIGHT, explosions[i].location_x, explosions[i].location_y);

            if (d1 <= explosions[i].radius || d2 <= explosions[i].radius)
                missiles_to_remove[missiles_to_remove_count++] = j;
        }

        // remove collided missiles!
        for (int j = 0; j < missiles_to_remove_count; ++j)
        {
            remove_enemy_missile(missiles_to_remove[j]);

            if(!is_gameover)
                PLAYER_SCORE += 100;
        }

        // check for collision with powerups
        if (draw_powerup)
        {
            float d1 = get_distance(powerup.location_x,powerup.location_y,explosions[i].location_x, explosions[i].location_y);
            float d2 = get_distance(powerup.location_x+GLYPH_WIDTH,powerup.location_y+GLYPH_HEIGHT,explosions[i].location_x, explosions[i].location_y);

            if(d1 <= explosions[i].radius || d2 <= explosions[i].radius)
            {
                // player gets the powerup
                draw_powerup = FALSE;
                player_powerups |= powerup.type;
                apply_powerups();
            }
        }
        
        float house_r = HOUSE_WIDTH / 2;

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

		explosions[i].radius += explosions[i].dir;
	}
}

static void draw_scene()
{
    // draw houses
    for (int i = 0; i < NUM_HOUSES; ++i)
    {
        draw_circle(houses[i],buffer_height -1,HOUSE_WIDTH/2,COLOR_GREY,TRUE);
    } 

    // draw missile bases
    draw_rect8(MISSILE_BASE_A_SRC_X-10,buffer_height - 20,20,20,COLOR_BLUE,TRUE);
    draw_rect8(MISSILE_BASE_B_SRC_X-10,buffer_height - 20,20,20,COLOR_BLUE,TRUE);
    draw_rect8(MISSILE_BASE_C_SRC_X-10,buffer_height - 20,20,20,COLOR_BLUE,TRUE);

	draw_char('Z',MISSILE_BASE_A_SRC_X - GLYPH_WIDTH/2, buffer_height - 20 + GLYPH_HEIGHT/2, COLOR_GREY );
	draw_char('X', MISSILE_BASE_B_SRC_X - GLYPH_WIDTH / 2, buffer_height - 20 + GLYPH_HEIGHT / 2, COLOR_GREY);
	draw_char('C', MISSILE_BASE_C_SRC_X - GLYPH_WIDTH / 2, buffer_height - 20 + GLYPH_HEIGHT / 2, COLOR_GREY);

    // draw missiles
    for(int i = 0; i < missile_count_player; ++i)
    {
		draw_line2(player_missiles[i].base_x, player_missiles[i].base_y, player_missiles[i].location_x, player_missiles[i].location_y, COLOR_BLUE);
        
        // draw target crosshair
		draw_pixel8(player_missiles[i].destination_x, player_missiles[i].destination_y, COLOR_GREY, 1.0f);
		draw_pixel8(player_missiles[i].destination_x - 1, player_missiles[i].destination_y - 1, COLOR_GREY, 1.0f);
		draw_pixel8(player_missiles[i].destination_x - 1, player_missiles[i].destination_y + 1, COLOR_GREY, 1.0f);
		draw_pixel8(player_missiles[i].destination_x + 1, player_missiles[i].destination_y - 1, COLOR_GREY, 1.0f);
		draw_pixel8(player_missiles[i].destination_x + 1, player_missiles[i].destination_y + 1, COLOR_GREY, 1.0f);

        draw_char(MISSILE_CHAR,player_missiles[i].location_x - 4,player_missiles[i].location_y - 8,COLOR_GREEN);  // @TEMP
    }

    for(int i = 0; i < missile_count_enemy; ++i)
    {
		draw_line2(enemy_missiles[i].base_x, enemy_missiles[i].base_y, enemy_missiles[i].location_x, enemy_missiles[i].location_y, COLOR_RED);
        draw_char(MISSILE_CHAR,enemy_missiles[i].location_x - MISSILE_WIDTH /2 ,enemy_missiles[i].location_y - MISSILE_HEIGHT / 2,COLOR_RED);  // @TEMP
    }

    // draw powerup(s)
    if(draw_powerup)
    {
        draw_char(powerup.c,powerup.location_x,powerup.location_y,COLOR_PURPLE);

        powerup_lifetime_counter--;
        if(powerup_lifetime_counter == 0)
            draw_powerup = FALSE;
    }

    // draw powerup icons
    if (player_powerups > 0x00)
    {
        if((player_powerups & MEGA_EXPLOSIONS) == MEGA_EXPLOSIONS)
        {
            draw_char('M',buffer_width-GLYPH_WIDTH-2,GLYPH_HEIGHT*2 + 2,COLOR_WHITE);
        }
        if((player_powerups & FASTER_MISSILES) == FASTER_MISSILES)
        {
            draw_char('F',buffer_width-2*GLYPH_WIDTH-4,GLYPH_HEIGHT*2 + 2,COLOR_WHITE);
        }
        if((player_powerups & INFINITE_AMMO) == INFINITE_AMMO)
        {
            draw_char('I',buffer_width-3*GLYPH_WIDTH-6,GLYPH_HEIGHT*2 + 2,COLOR_WHITE);
        }
        if((player_powerups & GUIDED_MISSILES) == GUIDED_MISSILES)
        {
            draw_char('G',buffer_width-4*GLYPH_WIDTH-8,GLYPH_HEIGHT*2 + 2,COLOR_WHITE);
        }
    }
    
	// draw explosions
	for (int i = 0; i < explosion_count; ++i)
	{
		int w = 2 * explosions[i].radius;
		draw_circle(explosions[i].location_x, explosions[i].location_y, w, rand()%256, TRUE);
	}
    
    //draw hud
	draw_string("SCORE:", 2, 2, COLOR_WHITE);
	char* pscore_str = to_string(PLAYER_SCORE);
    draw_string(pscore_str,66,2,COLOR_GREEN);
	free(pscore_str);

	draw_string("WAVE:", 2, 18, COLOR_WHITE);
	char* current_wave_str = to_string(current_wave + 1);
	draw_string(current_wave_str,55,18, COLOR_GREEN);
	free(current_wave_str);

	//draw_string("Z,X,C to shoot", 2, 34, COLOR_WHITE);
	draw_string("R to restart", 2, 34, COLOR_WHITE);

    draw_string("MISSILES:", buffer_width - 146, 2, COLOR_WHITE);
	char* available_missiles_str = to_string(AVAILABLE_MISSILES);
    draw_string(available_missiles_str,buffer_width - 48,2,COLOR_GREEN);
	free(available_missiles_str);

    // draw new wave text
    if (show_wave_text)
    {
        draw_string("WAVE ",(buffer_width - 99) / 2, (buffer_height - 12) / 2, COLOR_WHITE);
		draw_string(to_string(current_wave + 1), (buffer_width - 99) / 2 + 55, (buffer_height - 12) / 2, COLOR_WHITE);
    }
    
	if (is_gameover)
		draw_string("GAME OVER", (buffer_width - 99) / 2, (buffer_height - 12) / 2, COLOR_WHITE);

	// draw cursor
	int CURSOR_RADIUS = 5;
	draw_line2(curr_pt.x - CURSOR_RADIUS, curr_pt.y, curr_pt.x + CURSOR_RADIUS, curr_pt.y, COLOR_GREEN);
	draw_line2(curr_pt.x, curr_pt.y - CURSOR_RADIUS, curr_pt.x, curr_pt.y + CURSOR_RADIUS, COLOR_GREEN);

    
}
static void init_missiles()
{
	MISSILE_BASE_A_SRC_X =   buffer_width / 8;
	MISSILE_BASE_B_SRC_X =   buffer_width / 2;
	MISSILE_BASE_C_SRC_X = 7*buffer_width / 8;
	MISSILE_BASE_SRC_Y   =   buffer_height - 1;
}

static void begin_new_game()
{
    is_gameover = FALSE;
    is_running  = TRUE;

    AVAILABLE_MISSILES = 25;
    NUM_HOUSES         = 6;
    PLAYER_SCORE       = 0;

    PLAYER_MISSILE_SPEED = 4.0f;

    missile_count_player = 0;
    missile_count_enemy  = 0;
    explosion_count = 0;
    current_wave = 0;
    show_wave_text = TRUE;
    show_wave_text_countdown = 300;

    enemy_missile_counter = 0;
    enemy_missiles_fired  = 0;
    enemy_missiles_blown_up = 0;

    PLAYER_EXPLOSION_MAX_RADIUS = 30;
    ENEMY_EXPLOSION_MAX_RADIUS  = 30;

	powerup_counter = POWERUPCOUNTER_MAX;

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
    waves[0].missile_speed = 1.0f;
    waves[0].missile_period = 200;

    waves[1].missile_count = 15;
    waves[1].missile_speed = 1.2f;
    waves[1].missile_period = 150;

    waves[2].missile_count = 20;
    waves[2].missile_speed = 1.4f;
    waves[2].missile_period = 125;
    
    waves[3].missile_count = 30;
    waves[3].missile_speed = 1.6f;
    waves[3].missile_period = 100;
    
    waves[4].missile_count = 40;
    waves[4].missile_speed = 1.8f;
    waves[4].missile_period = 80;
    
    waves[5].missile_count = 50;
    waves[5].missile_speed = 2.0f;
    waves[5].missile_period = 70;
    
    waves[6].missile_count = 60;
    waves[6].missile_speed = 2.2f;
    waves[6].missile_period = 60; 
    
    waves[7].missile_count = 70;
    waves[7].missile_speed = 2.4f;
    waves[7].missile_period = 50;
    
    waves[8].missile_count = 80;
    waves[8].missile_speed = 2.7f;
    waves[8].missile_period = 30;

    waves[9].missile_count = 100;
    waves[9].missile_speed = 3.0f;
    waves[9].missile_period = 20;

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
    PLAYER_EXPLOSION_MAX_RADIUS = 30;
    PLAYER_MISSILE_SPEED = 4.0f;
    
    // add powerups
    if (player_powerups > 0x00)
    {
        if((player_powerups & MEGA_EXPLOSIONS) == MEGA_EXPLOSIONS)
        {
            PLAYER_EXPLOSION_MAX_RADIUS = 60;
        }
        if((player_powerups & FASTER_MISSILES) == FASTER_MISSILES)
        {
            PLAYER_MISSILE_SPEED = 8.0f;
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
	
	float dst_x = curr_pt.x;
	float dst_y = curr_pt.y;

	float delta_x = abs(dst_x - src_x);
	float delta_y = abs(dst_y - src_y);

	float angle = atan(delta_y / delta_x);
	float vel_x = cos(angle);
	float vel_y = sin(angle);
	
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

	float delta_x = abs(dst_x - src_x);
	float delta_y = abs(dst_y - src_y);

	float angle = atan(delta_y / delta_x);
	float vel_x = cos(angle);
	float vel_y = sin(angle);
	
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
        show_wave_text = TRUE;
        show_wave_text_countdown = 300;
        current_wave++;
        AVAILABLE_MISSILES += 25;
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

            curr_pt.x = pt.x;
            curr_pt.y = pt.y;

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
			if (wparam == 'R')
			{
                begin_new_game();
			}

			if (is_gameover) return;

            switch (wparam)
            {
            case 'Z': add_missile(A); break;
			case 'X': add_missile(B); break;
			case 'C': add_missile(C); break;
            }
            break;

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


