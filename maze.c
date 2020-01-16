#define USE_TI89
#define SAVE_SCREEN

#include <tigcclib.h>
#include <math.h>
#include "extgraph.h"

#define NUM_ANGLES (40)

unsigned char swap_coords;
unsigned char neg_temp;
unsigned char neg_x;
unsigned char neg_y;
int16_t slope_x;
int16_t slope_y;
int16_t vector_x;
int16_t vector_y;
unsigned int current_column = 0;
unsigned int column_width = 2;
int16_t camera_x;
int16_t camera_y;
unsigned int frame;
volatile unsigned char quit = 0;
volatile unsigned char do_render;
unsigned int current_angle = 0;
unsigned char interrupt;
unsigned char on_map;

void *kbq;
INT_HANDLER old_int_5;

int16_t camera_direction_x;
int16_t camera_direction_y;
int16_t plane_direction_x;
int16_t plane_direction_y;
int16_t camera_directions_x[NUM_ANGLES];
int16_t camera_directions_y[NUM_ANGLES];
int16_t plane_directions_x[NUM_ANGLES];
int16_t plane_directions_y[NUM_ANGLES];
uint16_t dist_adjustments[80];

void *gray_buffer;

__attribute__ ((always_inline)) static void draw_column_textured(void *plane_light, void *plane_dark, unsigned char column, uint16_t column_data_light, uint16_t column_data_dark, uint16_t dist){
	unsigned char *screen_column_light;
	unsigned char *screen_column_dark;
	unsigned char mask;
	uint16_t height;
	uint16_t adjusted_dist;
	uint16_t texture_pos = 0x8000;
	uint16_t n = 0;
	unsigned char screen_pos = 0;
	uint16_t stop_height;
	int top;
	uint16_t i;

	screen_column_light = ((unsigned char *) plane_light) + column/8;
	screen_column_dark = ((unsigned char *) plane_dark) + column/8;

	adjusted_dist = ((uint16_t) dist*dist_adjustments[column>>1])/64;
	if(adjusted_dist){
		height = ((UINT16_MAX/adjusted_dist)>>3)/3;
	} else {
		height = ((UINT16_MAX)>>3)/3;
	}
	if(height > 50){
		i = 8*height - 400;
		texture_pos >>= i/height;
		n = i - (i/height)*height;
		top = 0;
		stop_height = 50;
	} else {
		top = (50 - height)/2;
		stop_height = height;
		screen_column_light += top*60;
		screen_column_dark += top*60;
	}
	mask = 0xC0>>(column&0x06);
	if(height < 32){
		while(screen_pos < stop_height){
			if(column_data_light&texture_pos){
				screen_column_light[0] |= mask;
				screen_column_light[30] |= mask;
			} else {
				screen_column_light[0] &= ~mask;
				screen_column_light[30] &= ~mask;
			}
			if(column_data_dark&texture_pos){
				screen_column_dark[0] = screen_column_dark[0] | mask;
				screen_column_dark[30] = screen_column_dark[30] | mask;
			} else {
				screen_column_dark[0] = screen_column_dark[0] & ~mask;
				screen_column_dark[30] = screen_column_dark[30] & ~mask;
			}
			n += 16;
			while(n >= height){
				n -= height;
				texture_pos >>= 1;
			}
			screen_pos++;
			screen_column_light += 60;
			screen_column_dark += 60;
		}
	} else {
		//Optimize for large slices of the texture
		while(screen_pos < stop_height - 1){
			n += 16;
			if(n < height){
				if(column_data_light&texture_pos){
					screen_column_light[0] |= mask;
					screen_column_light[30] |= mask;
					screen_column_light[60] |= mask;
					screen_column_light[90] |= mask;
				} else {
					screen_column_light[0] &= ~mask;
					screen_column_light[30] &= ~mask;
					screen_column_light[60] &= ~mask;
					screen_column_light[90] &= ~mask;
				}
				if(column_data_dark&texture_pos){
					screen_column_dark[0] |= mask;
					screen_column_dark[30] |= mask;
					screen_column_dark[60] |= mask;
					screen_column_dark[90] |= mask;
				} else {
					screen_column_dark[0] &= ~mask;
					screen_column_dark[30] &= ~mask;
					screen_column_dark[60] &= ~mask;
					screen_column_dark[90] &= ~mask;
				}
				n += 16;
				if(n >= height){
					n -= height;
					texture_pos >>= 1;
				}
				screen_pos += 2;
				screen_column_light += 120;
				screen_column_dark += 120;
			} else {
				if(column_data_light&texture_pos){
					screen_column_light[0] |= mask;
					screen_column_light[30] |= mask;
				} else {
					screen_column_light[0] &= ~mask;
					screen_column_light[30] &= ~mask;
				}
				if(column_data_dark&texture_pos){
					screen_column_dark[0] |= mask;
					screen_column_dark[30] |= mask;
				} else {
					screen_column_dark[0] &= ~mask;
					screen_column_dark[30] &= ~mask;
				}
				n -= height;
				texture_pos >>= 1;
				screen_pos++;
				screen_column_light += 60;
				screen_column_dark += 60;
			}
		}
		if(screen_pos < stop_height){
			if(column_data_light&texture_pos){
				screen_column_light[0] |= mask;
				screen_column_light[30] |= mask;
			} else {
				screen_column_light[0] &= ~mask;
				screen_column_light[30] &= ~mask;
			}
			if(column_data_dark&texture_pos){
				screen_column_dark[0] |= mask;
				screen_column_dark[30] |= mask;
			} else {
				screen_column_dark[0] &= ~mask;
				screen_column_dark[30] &= ~mask;
			}
		}
	}
}

uint16_t textures_horizontal_light[][16] = {{
	0b1000001110000101,
	0b1110111110110101,
	0b1111101011111111,
	0b1111000011111111,
	0b1011110110001111,
	0b1111110100000111,
	0b1111111110011011,
	0b1011111111111111,
	0b1110011000111101,
	0b1100010001111111,
	0b1000010001111111,
	0b1100010110110111,
	0b1011111111000011,
	0b1010001110000111,
	0b1110000111000111,
	0b1111111100000111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111}};

uint16_t textures_horizontal_dark[][16] = {{
	0b1111111111111111,
	0b1001110011001111,
	0b1001110111001111,
	0b1111111111111001,
	0b1111001111111001,
	0b1001001111111111,
	0b1001111111100111,
	0b1111001111100111,
	0b1111101111111111,
	0b1111111110011001,
	0b1111111110011101,
	0b1111111001111111,
	0b1111111011111111,
	0b1111111111111001,
	0b1011111111111011,
	0b1001111111111111},
	{
	0b1000000000000001,
	0b1000000000000001,
	0b1000000000000001,
	0b1000000000000001,
	0b1000000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1000000000000001,
	0b1000000000000001,
	0b1000000000000001},
	{
	0b1000000000000001,
	0b1000000000000001,
	0b1000000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1000000000000001},
	{
	0b1000000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1000000110000001},
	{
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001}};

uint16_t textures_vertical_light[][16] = {{
	0b1000001110000101,
	0b1110111110110101,
	0b1111101011111111,
	0b1111000011111111,
	0b1011110110001111,
	0b1111110100000111,
	0b1111111110011011,
	0b1011111111111111,
	0b1110011000111101,
	0b1100010001111111,
	0b1000010001111111,
	0b1100010110110111,
	0b1011111111000011,
	0b1010001110000111,
	0b1110000111000111,
	0b1111111100000111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111},
	{
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111,
	0b1111111111111111}};

uint16_t textures_vertical_dark[][16] = {{
	0b1111111111111111,
	0b1001110011001111,
	0b1001110111001111,
	0b1111111111111001,
	0b1111001111111001,
	0b1001001111111111,
	0b1001111111100111,
	0b1111001111100111,
	0b1111101111111111,
	0b1111111110011001,
	0b1111111110011101,
	0b1111111001111111,
	0b1111111011111111,
	0b1111111111111001,
	0b1011111111111011,
	0b1001111111111111},
	{
	0b1000000000000001,
	0b1000100000000001,
	0b1001100000000001,
	0b1000100000000001,
	0b1001000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1001100110000001,
	0b1000100000000001,
	0b1001100000000001,
	0b1001000000000001,
	0b1000000000000001},
	{
	0b1001100000000001,
	0b1000100000000001,
	0b1001000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1001100110000001,
	0b1000100000000001,
	0b1001100000000001},
	{
	0b1001000110000001,
	0b1000000110000001,
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001,
	0b1000000110000001,
	0b1001100110000001},
	{
	0b1000000000000001,
	0b1111111111111111,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b0000000000000000,
	0b1111111111111111,
	0b1000000000000001}};

uint16_t textures_transparent[] = {
	0b0000000000000000,
	0b0000000000000000,
	0b0000001111000000,
	0b0000111111110000,
	0b0011111111111100};

unsigned char textures_id[256] = {0, 0, 1, 2, 3, 4};

int round_float(float x){
	if(x - (int) x > 0.5){
		return (int) x + 1;
	} else {
		return (int) x;
	}
}

void initialize_vectors(){
	int i;
	uint16_t dist;
	float plane_width;
	float camera_length;
	float plane_x;
	float plane_y;
	float camera_x;
	float camera_y;
	float w;
	float angle;

	plane_x = ((float) plane_direction_x)/64;
	plane_y = ((float) plane_direction_y)/64;
	camera_x = ((float) camera_direction_x)/64;
	camera_y = ((float) camera_direction_y)/64;
	plane_width = sqrt(plane_x*plane_x + plane_y*plane_y);
	camera_length = sqrt(camera_x*camera_x + camera_y*camera_y);

	for(i = 0; i < 80; i++){
		w = (i - 39.5)/39.5*plane_width;
		dist_adjustments[i] = round_float(sqrt(camera_length*camera_length + w*w)/sqrt(w*w/(camera_length*camera_length) + 1)*64);
	}

	for(i = 0; i < NUM_ANGLES; i++){
		angle = 2*PI*i/NUM_ANGLES;
		camera_directions_x[i] = round_float(cos(angle)*camera_direction_x - sin(angle)*camera_direction_y);
		camera_directions_y[i] = round_float(sin(angle)*camera_direction_x + cos(angle)*camera_direction_y);
		plane_directions_x[i] = round_float(cos(angle)*plane_direction_x - sin(angle)*plane_direction_y);
		plane_directions_y[i] = round_float(sin(angle)*plane_direction_x + cos(angle)*plane_direction_y);
	}
}

unsigned char map[33][33];
unsigned char seen[33][33];
unsigned char direction_orders[24] = {
	0b00011011,
	0b00011110,
	0b00100111,
	0b00101101,
	0b00110110,
	0b00111001,
	0b01001011,
	0b01001110,
	0b01100011,
	0b01101100,
	0b01110010,
	0b01111000,
	0b10000111,
	0b10001101,
	0b10010011,
	0b10011100,
	0b10110001,
	0b10110100,
	0b11000110,
	0b11001001,
	0b11010010,
	0b11011000,
	0b11100001,
	0b11100100};

void prepare_maze(){
	unsigned char i;
	unsigned char j;

	memset(seen, 0x00, sizeof(unsigned char)*33*33);
	memset(map, 0x00, sizeof(unsigned char)*33*33);
	for(i = 1; i < 32; i++){
		for(j = 1; j < 32; j++){
			map[i][j] = 1;
		}
	}
}

void move_direction(unsigned char *x, unsigned char *y, unsigned char direction){
	switch(direction){
		case 0:
			++*x;
			break;
		case 1:
			++*y;
			break;
		case 2:
			--*y;
			break;
		case 3:
			--*x;
	}
}

void remove_wall(unsigned char x, unsigned char y, unsigned char direction){
	switch(direction){
		case 0:
			map[2*x - 1][2*y] = (rand()%2 && rand()%2)*2;
			break;
		case 1:
			map[2*x][2*y - 1] = (rand()%2 && rand()%2)*2;
			break;
		case 2:
			map[2*x][2*y + 1] = (rand()%2 && rand()%2)*2;
			break;
		case 3:
			map[2*x + 1][2*y] = (rand()%2 && rand()%2)*2;
	}
}

void generate_maze(unsigned char x, unsigned char y){
	static unsigned char r;
	unsigned char direction;
	unsigned char i;

	map[2*x][2*y] = 0;

	r = rand()%24;
	direction = direction_orders[r];

	for(i = 0; i < 4; i++){
		move_direction(&x, &y, direction&0x03);
		if(map[2*x][2*y]){
			remove_wall(x, y, direction&0x03);
			generate_maze(x, y);
		}
		move_direction(&x, &y, 3 - (direction&0x03));
		direction >>= 2;
	}
}

int16_t abs16(int16_t x){
	if(x < 0){
		return -x;
	}

	return x;
}

__attribute__ ((always_inline)) static unsigned char horizontal(void *plane_light, void *plane_dark, int x, int y, uint16_t pos, uint16_t dist){
	uint16_t abs_vector_x;
	unsigned int texture_column;
	unsigned char texture_id;

	abs_vector_x = abs16(vector_x);
	
	if(swap_coords){
		if(neg_y){
			pos = abs_vector_x - pos;
		}
	} else {
		if(neg_x){
			pos = abs_vector_x - pos;
		}
	}

	texture_column = (pos*16/abs_vector_x)%16;
	texture_id = textures_id[map[x][y]];
	if((textures_transparent[texture_id]>>texture_column)&1){
		return 0;
	} else {
		draw_column_textured(plane_light, plane_dark, current_column, textures_vertical_light[texture_id][texture_column], textures_vertical_dark[texture_id][texture_column], dist);
		return 1;
	}
}

__attribute__ ((always_inline)) static unsigned char vertical(void *plane_light, void *plane_dark, int x, int y, uint16_t pos, uint16_t dist){
	uint16_t abs_vector_y;
	unsigned int texture_column;
	unsigned char texture_id;

	abs_vector_y = abs16(vector_y);

	if(swap_coords){
		if(neg_x){
			pos = abs_vector_y - pos;
		}
	} else {
		if(neg_y){
			pos = abs_vector_y - pos;
		}
	}

	texture_column = (pos*16/abs_vector_y)%16;
	texture_id = textures_id[map[x][y]];
	if((textures_transparent[texture_id]>>texture_column)&1){
		return 0;
	} else {
		draw_column_textured(plane_light, plane_dark, current_column, textures_vertical_light[texture_id][texture_column], textures_vertical_dark[texture_id][texture_column], dist);
		return 1;
	}
}

__attribute__ ((always_inline)) static unsigned char collision1(void *plane_light, void *plane_dark, int x, int y, uint16_t pos, uint16_t dist){
	int temp;

	if(swap_coords){
		temp = y;
		y = x;
		x = temp;
	}

	if(map[x][y]){
		if(map[x][y] == 1){
			seen[x][y] = 1;
		}
		if(!swap_coords){
			return horizontal(plane_light, plane_dark, x, y, pos, dist);
		} else {
			return vertical(plane_light, plane_dark, x, y, pos, dist);
		}
	} else {
		return 0;
	}
}

__attribute__ ((always_inline)) static unsigned char collision2(void *plane_light, void *plane_dark, int x, int y, uint16_t pos, uint16_t dist){
	int temp;

	if(swap_coords){
		temp = y;
		y = x;
		x = temp;
	}

	if(map[x][y]){
		if(map[x][y] == 1){
			seen[x][y] = 1;
		}
		if(!swap_coords){
			return vertical(plane_light, plane_dark, x, y, pos, dist);
		} else {
			return horizontal(plane_light, plane_dark, x, y, pos, dist);
		}
	} else {
		return 0;
	}
}

__attribute__ ((always_inline)) static void render_column(void *plane_light, void *plane_dark, uint16_t x, uint16_t y, int16_t slope_y, int16_t slope_x, unsigned int max){
	int square_x;
	int square_y;
	int x_inc;
	int y_inc;
	int inc_temp;
	uint8_t offset_x;
	uint8_t offset_y;
	int16_t a;
	int16_t b;
	int16_t n;
	int16_t m;
	uint16_t slope_x_extended;
	uint16_t slope_y_extended;
	uint16_t mod_extended;
	uint16_t temp;
	uint16_t dist_vertical;
	uint16_t dist_horizontal;
	uint16_t dist_vertical_change;
	uint16_t dist_horizontal_change;
	unsigned int i;

	if(slope_x < 0){
		neg_x = 1;
		slope_x = -slope_x;
		x_inc = -1;
	} else {
		neg_x = 0;
		x_inc = 1;
	}

	if(slope_y < 0){
		neg_y = 1;
		slope_y = -slope_y;
		y_inc = -1;
	} else {
		neg_y = 0;
		y_inc = 1;
	}

	if(slope_y >= slope_x){
		swap_coords = 1;
		temp = y;
		y = x;
		x = temp;
		temp = slope_y;
		slope_y = slope_x;
		slope_x = temp;
		inc_temp = y_inc;
		y_inc = x_inc;
		x_inc = inc_temp;
		neg_temp = neg_y;
		neg_y = neg_x;
		neg_x = neg_temp;
	} else {
		swap_coords = 0;
	}

	square_x = x>>6;
	square_y = y>>6;
	if(!neg_x){
		offset_x = x&0x3F;
	} else {
		offset_x = 63 - x&0x3F;
	}
	if(!neg_y){
		offset_y = y&0x3F;
	} else {
		offset_y = 63 - y&0x3F;
	}
	a = ((uint16_t) offset_y)*slope_x/64;
	b = ((uint16_t) offset_x)*slope_y/64;
	n = slope_x - a + b;
	if(slope_y){
		m = (slope_y + a - b)%slope_y;
		mod_extended = slope_x%slope_y;
	} else {
		m = 0;
		mod_extended = 0;
	}

	if(slope_x){
		dist_vertical_change = (UINT16_MAX/slope_x)>>4;
	} else {
		dist_vertical_change = UINT16_MAX;
	}
	if(slope_y){
		dist_horizontal_change = (UINT16_MAX/slope_y)>>4;
	} else {
		dist_horizontal_change = UINT16_MAX;
	}
	dist_horizontal = -((int16_t) dist_horizontal_change)*offset_y/64;
	dist_vertical = -((int16_t) dist_vertical_change)*offset_x/64;
	for(i = 0; i < max; i++){
		n -= slope_y;
		if(n <= 0){
			n += slope_x;
			m -= mod_extended;
			if(m <= 0){
				m += slope_y;
			}
			square_y += y_inc;
			dist_horizontal += dist_horizontal_change;
			if(collision1(plane_light, plane_dark, square_x, square_y, m, dist_horizontal)){
				return;
			}
		}
		dist_vertical += dist_vertical_change;
		square_x += x_inc;
		if(collision2(plane_light, plane_dark, square_x, square_y, n, dist_vertical)){
			return;
		}
	}
}

void render(void *plane_light, void *plane_dark){
	int16_t plane_vector_x;
	int16_t plane_vector_y;
	int i;

	memset(plane_dark, 0, 1500);
	memset(plane_dark + 1500, 0xFF, 1500);
	memset(plane_light, 0, 3000);
	for(i = 0; i < 80; i++){
		current_column = i*2;
		plane_vector_x = -((int16_t) i*2 - 79)*plane_direction_x/79;
		plane_vector_y = -((int16_t) i*2 - 79)*plane_direction_y/79;
		vector_x = camera_direction_x + plane_vector_x;
		vector_y = camera_direction_y + plane_vector_y;
		render_column(plane_light, plane_dark, camera_x, camera_y, vector_x, vector_y, 20);
	}
}

void render_map(void *plane_light, void *plane_dark){
	int x;
	int y;
	int screen_x;
	int screen_y;

	FastClearScreen_R(plane_light);
	FastClearScreen_R(plane_dark);
	if(frame%10 > 5){
		FastFillRect_R(plane_dark, 75, 45, 79, 49, A_NORMAL);
	}
	if(current_angle >= 35 || current_angle < 5){
		for(screen_x = 0, x = camera_x/64 - 15; x <= camera_x/64 + 16; screen_x += 5, x++){
			for(screen_y = 0, y = camera_y/64 + 9; y >= camera_y/64 - 10; screen_y += 5, y--){
				if(x > 0 && x < 33 && y > 0 && y < 33 && seen[x][y] == 1){
					FastFillRect_R(plane_light, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
					FastFillRect_R(plane_dark, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
				}
			}
		}
	} else if(current_angle >= 5 && current_angle < 15){
		for(screen_y = 0, x = camera_x/64 + 9; x >= camera_x/64 - 10; screen_y += 5, x--){
			for(screen_x = 0, y = camera_y/64 + 15; y >= camera_y/64 - 16; screen_x += 5, y--){
				if(x > 0 && x < 33 && y > 0 && y < 33 && seen[x][y] == 1){
					FastFillRect_R(plane_light, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
					FastFillRect_R(plane_dark, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
				}
			}
		}
	} else if(current_angle >= 15 && current_angle < 25){
		for(screen_x = 0, x = camera_x/64 + 15; x >= camera_x/64 - 16; screen_x += 5, x--){
			for(screen_y = 0, y = camera_y/64 - 9; y <= camera_y/64 + 10; screen_y += 5, y++){
				if(x > 0 && x < 33 && y > 0 && y < 33 && seen[x][y] == 1){
					FastFillRect_R(plane_light, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
					FastFillRect_R(plane_dark, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
				}
			}
		}
	} else {
		for(screen_y = 0, x = camera_x/64 - 9; x <= camera_x/64 + 10; screen_y += 5, x++){
			for(screen_x = 0, y = camera_y/64 - 15; y <= camera_y/64 + 16; screen_x += 5, y++){
				if(x > 0 && x < 33 && y > 0 && y < 33 && seen[x][y] == 1){
					FastFillRect_R(plane_light, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
					FastFillRect_R(plane_dark, screen_x, screen_y, screen_x + 4, screen_y + 4, A_NORMAL);
				}
			}
		}
	}
}

DEFINE_INT_HANDLER(update){
	unsigned char square_val;

	ExecuteHandler(old_int_5);
	interrupt = !interrupt;
	if(interrupt){
		if(_keytest(RR_ESC)){
			quit = 1;
		}
		if(_keytest(RR_UP)){
			square_val = map[(camera_x + camera_direction_y/24)>>6][camera_y>>6];
			if(!square_val || square_val == 5){
				camera_x += camera_direction_y/32;
			}
			square_val = map[camera_x>>6][(camera_y + camera_direction_x/24)>>6];
			if(!square_val || square_val == 5){
				camera_y += camera_direction_x/32;
			}
			on_map = 0;
		}
		if(_keytest(RR_RIGHT)){
			current_angle = (current_angle + 1)%NUM_ANGLES;
			on_map = 0;
		}
		if(_keytest(RR_DOWN)){
			square_val = map[(camera_x - camera_direction_y/24)>>6][camera_y>>6];
			if(!square_val || square_val == 5){
				camera_x -= camera_direction_y/32;
			}
			square_val = map[camera_x>>6][(camera_y - camera_direction_x/24)>>6];
			if(!square_val || square_val == 5){
				camera_y -= camera_direction_x/32;
			}
			on_map = 0;
		}
		if(_keytest(RR_LEFT)){
			if(current_angle){
				current_angle--;
			} else {
				current_angle = NUM_ANGLES - 1;
			}
			on_map = 0;
		}
		if(_keytest(RR_MODE)){
			on_map = 1;
		}
		if(!on_map && (frame&1)){
			square_val = map[(camera_x + camera_direction_y/3)>>6][(camera_y + camera_direction_x/3)>>6];
			if(square_val > 1 && square_val < 5){
				map[(camera_x + camera_direction_y/3)>>6][(camera_y + camera_direction_x/3)>>6]++;
			}
		}
		camera_direction_x = camera_directions_x[current_angle];
		camera_direction_y = camera_directions_y[current_angle];
		plane_direction_x = plane_directions_x[current_angle];
		plane_direction_y = plane_directions_y[current_angle];
		do_render = 1;
	}
}

void _main(){
	unsigned int i;
	int timerval;

	clrscr();
	gray_buffer = malloc(GRAYDBUFFER_SIZE);
	if(!gray_buffer){
		printf("Not enough memory!\n");
		ngetchx();
		exit(1);
	}
	kbq = kbd_queue();
	old_int_5 = GetIntVec(AUTO_INT_5);

	randomize();
	GrayOn();
	GrayDBufInit(gray_buffer);

	prepare_maze();
	generate_maze(2, 2);
	camera_x = 160;
	camera_y = 160;
	plane_direction_x = 0;
	plane_direction_y = -256;
	camera_direction_x = 256;
	camera_direction_y = 0;
	current_angle = 0;

	initialize_vectors();
	frame = 0;
	interrupt = 0;
	do_render = 0;
	quit = 0;
	on_map = 0;
	SetIntVec(AUTO_INT_5, update);
	OSFreeTimer(USER1_TIMER);
	OSRegisterTimer(USER1_TIMER, 3000);
	while(!quit){
		if(do_render && !on_map){
			render(GrayDBufGetHiddenPlane(LIGHT_PLANE), GrayDBufGetHiddenPlane(DARK_PLANE));
			GrayDBufToggle();
			do_render = 0;
			frame++;
		} else if(do_render && on_map){
			render_map(GrayDBufGetHiddenPlane(LIGHT_PLANE), GrayDBufGetHiddenPlane(DARK_PLANE));
			GrayDBufToggle();
			do_render = 0;
			frame++;
		}
	}
	GrayOff();
	free(gray_buffer);
	SetIntVec(AUTO_INT_5, old_int_5);
	timerval = OSTimerCurVal(USER1_TIMER);
	OSFreeTimer(USER1_TIMER);
	clrscr();
	printf("frame: %d\n", frame);
	printf("timer: %d\n", 3000 - timerval);
	ngetchx();
}

