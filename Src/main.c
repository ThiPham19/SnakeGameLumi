 /* File name: SnakeGameSTM32F4
 *
 * Description: code snake game with 2 level speed
 *
 *
 * Last Changed By:  $Author: ThiPham19 $
 * Revision:         $Revision: $
 * Last Changed:     $Date: $September 13, 2023
 *
 * Code sample:
 ******************************************************************************/
/******************************************************************************/
/*                              INCLUDE FILES                                 */
/******************************************************************************/
#include "system_stm32f4xx.h"
#include "stm32f401re_exti.h"
#include "stm32f401re_rcc.h"
#include "timer.h"
#include "eventman.h"
#include "led.h"
#include "melody.h"
#include "eventbutton.h"
#include "button.h"
#include "Ucglib.h"
#include "lightsensor.h"
#include "stm32f401re_rng.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


/******************************************************************************/
/*                     PRIVATE TYPES and DEFINITIONS                         */
/******************************************************************************/
#define LCD_WIDTH	127		//127 pixel -> w = 25cell
#define LCD_HEIGHT	119 	//117 pixel -> h = 23cell
#define SNAKE_MAX_LNG		(uint16_t)(100)
#define SNAKE_INIT_LNG		(uint16_t)(3)
#define INVALID_COORDS		(uint16_t)(-1)
#define GENERAL_ERROR		(uint16_t)(-1)

#define SNAKE_INIT_X_CORD	(uint16_t)(2)
#define SNAKE_INIT_Y_CORD	(uint16_t)(10)
#define SNAKE_WON_LIMIT		(uint16_t)(SNAKE_MAX_LNG - 1)

#define ARENA_MAX_X			(uint16_t)(25)
#define ARENA_MIN_X			(int8_t)(-1)
#define ARENA_MAX_Y			(uint16_t)(22)
#define ARENA_MIN_Y			(int8_t)(-1)

#define FOOD_MAX_X			(uint16_t)(24)
#define FOOD_MIN_X			(uint16_t)(0)
#define FOOD_MAX_Y			(uint16_t)(22)
#define FOOD_MIN_Y			(uint16_t)(0)
#define FOOD_MAX_ITER		(uint16_t)(1000)

#define ARENA_OFFSET_X		(uint16_t)(1)
#define ARENA_OFFSET_Y		(uint16_t)(8)
#define CELL_SIZE			(uint16_t)(5)
#define FOOD_SIZE			(uint16_t)(3)

#define SPEED_1				(uint16_t)(100)
#define SPEED_2				(uint16_t)(500)
/*	@brief Events APIs*/
//Định nghĩa kiểu dữ liệu enum cho các sự kiện và trạng thái của chương trình chính
typedef enum {
	EVENT_EMPTY, EVENT_APP_INIT, EVENT_APP_FLUSHEM_READY,
} event_api_t, *event_api_p;

typedef enum {
	STATE_APP_STARTUP, STATE_APP_IDLE, STATE_APP_RESET
} state_app_t;


typedef enum { UP = 'W', DOWN = 'S', LEFT =  'A', RIGHT = 'D', PAUSE = 'P', QUIT = 'Q' } snake_dir_e;
typedef enum { WAITING, PLACED, EATEN } foodstate_e;
typedef enum { PLAYING, CRASHED, WON } snake_state_e;

typedef struct coord_tag
{
	uint16_t x;
	uint16_t y;
} coord_t;

typedef struct snake_tag
{
	snake_dir_e direction;
	coord_t body[SNAKE_MAX_LNG];
	uint16_t length;
	coord_t ghost;  //bóng của cái đuôi, dùng cho hàm tăng độ dài :vv
	snake_state_e state;
} snake_t;
typedef struct food_tag
{
	coord_t coord;
	foodstate_e state;
	uint16_t time_elapsed;
} food_t;

/******************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                         */
/******************************************************************************/

/******************************************************************************/
/*                              PRIVATE DATA                                  */
/******************************************************************************/
static ucg_t ucg;
static ucg_t ucg1;
static uint8_t IdTimer = NO_TIMER;
static uint16_t gRandSeed;
static uint32_t gPrgCycle = 0;
static char score_display[20] = "";
static uint8_t score = 0;

static uint8_t running = 0;
static uint16_t speed;
/******************************************************************************/
/*                              EXPORTED DATA                                 */
/******************************************************************************/
uint8_t eCurrentState = 0;
char extKeyBoardButton;
/******************************************************************************/
/*                            PRIVATE FUNCTIONS                               */
/******************************************************************************/
static void AppInitCommon(void);
static state_app_t GetStateApp(void);
static void SetStateApp(state_app_t state);
static void AppStateManager(uint8_t event);
static void LoadConfiguration(void);
static uint16_t platform_init_randomizer(void);
static void change_speed();

/******************************************************************************/
/*                            EXPORTED FUNCTIONS                              */
/******************************************************************************/
void DeviceStateMachine(uint8_t event);
void SetupPrg();

void platform_get_control(snake_t * snake);

void snake_init(snake_t* snake);
void snake_move(snake_t* snake);
void snake_haseaten(snake_t* snake, food_t* food);
void snake_display(snake_t* snake);
void platform_eraseCell(uint16_t x, uint16_t y);
void platform_drawCell(uint16_t x, uint16_t y);
void snake_place_food(snake_t* snake, food_t* food, uint32_t tick);
uint16_t generate_food(snake_t* snake, food_t *food);
void platform_drawFood(uint16_t x, uint16_t y);
void platform_deleteFood(uint16_t x, uint16_t y);
void platform_fatal(void);
void displayGameOver();
void Score();
/******************************************************************************/

/**
  * @brief  RNG configuration
  * @param  None
  * @retval None
  */

void update_running(){
	running = 1;
}
void TIM5config()
{
	RCC->APB1ENR |= (1<<3);//bật timer5 clock

	TIM5->PSC = 84-1; //84MHz/84 = 1Mhz ~ 1us
	TIM5->ARR = 0xffff;

	TIM5->CR1 |= (1<<0);
	while(!(TIM5->SR&(1<<0)));
}
void Delay_us(uint16_t us){
	TIM5->CNT = 0;
	while(TIM5->CNT < us);
}
void Delay_ms(uint16_t ms){
	for(uint16_t i=0; i < ms; i++){
		Delay_us(1000);
	}
}
int main(void) {
	AppInitCommon();
	SetStateApp(STATE_APP_STARTUP); 		// Khởi động ở chế độ STATE_APP_STARTUP
	EventSchedulerAdd(EVENT_APP_INIT); 		//Khởi tạo bộ đệm buffer để quản lý các sự kiện của chương trình chính
	TIM5config();							//cấu hình tim5 tạo hàm delay
	platform_init_randomizer();				//tạo gRandSeed phục vụ tạo số ngẫu nhiên
	/* Loop forever */
	while (1) {

		processTimerScheduler(); 			// Xử lý các bộ hẹn giờ đã được tạo
		processEventScheduler();			// Xử lý các sự kiện của chương trình chính do bộ quản lý sự kiện quản lý.

		memset(score_display, 0, sizeof(score_display));
	  	snake_t snake = { 0 };
	  	food_t food = { 0 };
	  	uint32_t gPrgCycle = 0;
	  	snake_init(&snake);
	  	uint8_t gameover = 1;
	  	IdTimer = TimerStart("update_running", 2500, 0, update_running, NULL);
	  	if(running){
	  		for(;;){
		  		platform_get_control(&snake);
		  		snake_move(&snake);

		  		if (snake.state != PLAYING){
		  			memset(score_display, 0, sizeof(score_display));
		  			break;}

		  		snake_haseaten(&snake, &food);
		  		snake_display(&snake);
		  		snake_place_food(&snake, &food, gPrgCycle);
		  		Score();
		  		Delay_ms(speed);
	  		}
			while(1){
				processTimerScheduler();
				processEventScheduler();
				if(gameover){
				ucg_ClearScreen(&ucg1);
				gameover = 0;
				IdTimer = TimerStart("gameOver", 5, 0, displayGameOver, NULL);
				}
			}
	  	}
	}
}
static void change_speed(){
	if(speed == SPEED_1){
		speed = SPEED_2;
	}else{
		speed = SPEED_1;
	}

}
void Score(){
	processTimerScheduler();
	ucg_DrawString(&ucg1, 42, 127, 0, score_display);

}
void displayGameOver()
{
	if(IdTimer != NO_TIMER){
	TimerStop(IdTimer);
	sprintf(score_display, "GameOver-%d", score);
	}
}



static void AppInitCommon(void) {
	SystemCoreClockUpdate(); 				//Initializes system clock để cấu hình clock của hệ thống
	TimerInit();						// Initializes system tick để xử lý các sự kiện thời gian.


	EventSchedulerInit(AppStateManager); 	// Khởi tạo bộ đệm buffer để quản lý các sự kiện của chương trình chính
	EventButton_Init();	 					// Cấu hình chân GPIO của các nút nhấn trên mạch.
	LedControl_Init(); 						// Cấu hình chân GPIO của các led RGB trên mạch.
	LightSensor_Init(ADC_READ_MODE_DMA);

	// Initializes glcd
	Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID); // khởi tạo
	//time_initial = (uint32_t) GetMilSecTick; // lấy thời gian lúc cắm nguồn làm mốc
	Ucglib4WireSWSPI_begin(&ucg, UCG_FONT_MODE_SOLID);
	ucg_ClearScreen(&ucg);
	ucg_SetFont(&ucg, ucg_font_ncenR10_hr);
	ucg_SetColor(&ucg, 0, 255, 255, 255);
	ucg_SetColor(&ucg, 1, 0, 0, 0);
	ucg_SetRotate180(&ucg);
	Ucglib4WireSWSPI_begin(&ucg1, UCG_FONT_MODE_SOLID);
	ucg_SetFont(&ucg1, ucg_font_6x10_tr);
	ucg_SetColor(&ucg1, 0, 255, 255, 255);
	ucg_SetColor(&ucg1, 1, 0, 0, 0);
	ucg_SetRotate180(&ucg1);
}

static state_app_t GetStateApp(void) {
	/*Return state of application */
	return eCurrentState;
}
static void SetStateApp(state_app_t state) {
	/* Set state of application */
	eCurrentState = state;
}
static void AppStateManager(uint8_t event) {
	switch (GetStateApp()) {
	case STATE_APP_STARTUP:
		if (event == EVENT_APP_INIT) {
			// Load configuration
			LoadConfiguration();
			SetStateApp(STATE_APP_IDLE);
		}
		break;

	case STATE_APP_IDLE:
		DeviceStateMachine(event);
		break;

	case STATE_APP_RESET:
		break;
	default:
		break;
	}
}
static void LoadConfiguration(void) {
	// Display output
	ucg_DrawString(&ucg, 55, 24, 0, "IOT");
	ucg_DrawString(&ucg, 5, 48, 0, "Programming by");
	ucg_DrawString(&ucg, 5, 72, 0, "Lumi Smarthome");
	ucg_ClearScreen(&ucg);
	TimerStart("ClearScreenAndSetup", 1200, 0, SetupPrg, NULL);
}

void SetupPrg(){
		ucg_ClearScreen(&ucg);
		ucg_DrawFrame(&ucg1, 0, 7, 127, 112);
		ucg_DrawString(&ucg1, 0, 127, 0, "Score: 0");
}

void snake_init(snake_t* snake)
{
	snake->length = SNAKE_INIT_LNG;
	snake->direction = RIGHT;
	snake->state = PLAYING;
	snake->ghost.x = INVALID_COORDS;
	snake->ghost.y = INVALID_COORDS;

	memset(&snake->body[0], 0, SNAKE_MAX_LNG*sizeof(coord_t));

	for (int idx = 0; idx < SNAKE_INIT_LNG; idx++)
	{
		snake->body[idx].x = SNAKE_INIT_X_CORD + idx;
		snake->body[idx].y = SNAKE_INIT_Y_CORD;
	}
	speed = SPEED_1;
}

static uint16_t platform_init_randomizer(void)//tạo gRandSeed để tạo số ngẫu nhiên cho vị trí food
{
	/* This randomizer is based on ADC noise
	 * as a LFSR seed number */
	gRandSeed = LightSensor_MeasureUseDMAMode();
	//while(gRandSeed < 0x8000) gRandSeed += gRandSeed;
	return gRandSeed;
}

//lấy giá trị khi nhấn nút để di chuyển snake
void platform_get_control(snake_t * snake)
{
	snake_dir_e direction = 0;
	static snake_dir_e prev_direction = RIGHT;

//	typedef enum { UP = 'W', DOWN = 'S', LEFT =  'A', RIGHT = 'D', PAUSE = 'P', QUIT = 'Q' } snake_dir_e;
//you can add tow button for Pause and Quit

    if(!Button_GetLogicInputPin(BUTTON_KIT_ID5))extKeyBoardButton='S';
    if(!Button_GetLogicInputPin(BUTTON_KIT_ID1))extKeyBoardButton='W';
    if(!Button_GetLogicInputPin(BUTTON_KIT_ID4))extKeyBoardButton='D';
    if(!Button_GetLogicInputPin(BUTTON_KIT_ID2))extKeyBoardButton='A';
    if(!Button_GetLogicInputPin(BUTTON_KIT_ID3))extKeyBoardButton='P';

	direction = (snake_dir_e)extKeyBoardButton;

	if (direction == 0)
	{
		return;
	}

	extKeyBoardButton = 0;

	if ((direction != LEFT) && (direction != RIGHT) && (direction != UP) &&
		(direction != DOWN) && (direction != PAUSE) && (direction != QUIT))
	{
		prev_direction = snake->direction;
		snake->direction = PAUSE;
		// nhấn nút khác các nút đã cài đặt -> lưu hướng rắn cũ, hướng rắn đặt là PAUSE
	}
	else
	{
		if (direction == PAUSE)//nhấn PAUSE
		{
			if (snake->direction != PAUSE)
			{
				prev_direction = snake->direction;
				snake->direction = PAUSE;
				change_speed();
			}
			//hướng rắn cũ khác PAUSE thì lưu hướng rắn cũ, hướng rắn đặt là PAUSE
			//-> tạm dừng game
			else
			{
				snake->direction = prev_direction;
			}

			//còn hướng rắn cũ khác PAUSE thì hướng rắn được lấy lại là hướng cũ
			//-> tiếp tục game
		}

		else //nhấn một trong các nút chọn hướng
		{
			if ((snake->direction != PAUSE) &&
				!(snake->direction == LEFT && direction == RIGHT) &&
				!(snake->direction == RIGHT && direction == LEFT) &&
				!(snake->direction == UP && direction == DOWN) &&
				!(snake->direction == DOWN && direction == UP))
			{
				snake->direction = direction;
			}
			//loại trừ các hướng điều khiển quay đầu 180 độ thì
		}
	}
}

void snake_move(snake_t* snake)
{
	if (NULL == snake || PAUSE == snake->direction)
	{
		return;
	}

	snake->ghost = snake->body[0];
	//luôn lưu tọa độ đuôi để khi ăn được food sẽ sử dụng tăng độ dài

	memcpy(&snake->body[0], &snake->body[1], sizeof(coord_t) * (snake->length - 1));
	//di chuyển theo hướng hiện tại -> lưu (length-1) tọa độ lại ngoại trừ phần đầu

	switch (snake->direction)
	/*direction luôn đc cập nhật bởi hàm snake_control sẽ quyết định cho hướng đi
	tiêp theo -> tạo ra tọa độ mới cho phần đầu*/
	{
	case UP:
	{
		if ((snake->body[snake->length - 1].y - 1) == ARENA_MIN_Y)
		{
			snake->state = CRASHED;
			break;
		}
		for (int idx = 0; idx < snake->length; idx++)
		{
			if (((snake->body[snake->length - 1].y - 1) == snake->body[idx].y) &&
				((snake->body[snake->length - 1].x) == snake->body[idx].x))
				//đâm vào chính mình trường hợp hướng lên
			{
				snake->state = CRASHED;
			}
		}
		snake->body[snake->length - 1].y--;
	}
	break;
	case DOWN:
	{
		if ((snake->body[snake->length - 1].y + 1) == ARENA_MAX_Y)
		{
			snake->state = CRASHED;
			break;
		}
		for (int idx = 0; idx < snake->length; idx++)
		{
			if (((snake->body[snake->length - 1].y + 1) == snake->body[idx].y) &&
				((snake->body[snake->length - 1].x) == snake->body[idx].x))
				//đâm vào chính mình trường hợp hướng xuống
			{
				snake->state = CRASHED;
			}
		}

		snake->body[snake->length - 1].y++;
	}
	break;
	case RIGHT:
	{
		if ((snake->body[snake->length - 1].x + 1) == ARENA_MAX_X)
		{
			snake->state = CRASHED;
			break;
		}
		for (int idx = 0; idx < snake->length; idx++)
		{
			if (((snake->body[snake->length - 1].x + 1) == snake->body[idx].x) &&
				((snake->body[snake->length - 1].y) == snake->body[idx].y))
				//đâm vào chính mình trường hợp hướng sang phải
			{
				snake->state = CRASHED;
			}
		}
		snake->body[snake->length - 1].x++;
	}
	break;
	case LEFT:
	{
		if ((snake->body[snake->length - 1].x - 1) == ARENA_MIN_X)
		{
			snake->state = CRASHED;
			break;
		}
		for (int idx = 0; idx < snake->length; idx++)
		{
			if (((snake->body[snake->length - 1].x - 1) == snake->body[idx].x) &&
				((snake->body[snake->length - 1].y) == snake->body[idx].y))
				//đâm vào chính mình trường hợp hướng sang trái
			{
				snake->state = CRASHED;
			}
		}
		snake->body[snake->length - 1].x--;
	}
	break;
	default:
	{
	}
	//nobreak;?
	}

	if (snake->length == SNAKE_WON_LIMIT)
	{
		snake->state = WON;		//thắng thì hết game -> VUA
	}
}

void snake_haseaten(snake_t* snake, food_t* food)
{
	if ((snake->body[snake->length - 1].x == food->coord.x)
		&& (snake->body[snake->length - 1].y == food->coord.y))
		//đầu rắn trùng food
	{
		coord_t tempSnake[SNAKE_MAX_LNG] = {0};
		memcpy(tempSnake, &(snake->body[0]), snake->length*sizeof(coord_t));
		//lưu toàn bộ tọa độ của body vào biến tạm

		memcpy(&(snake->body[1]), tempSnake, snake->length*sizeof(coord_t));
		//dịch toàn bộ tọa độ body sang 1 body

		snake->body[0] = snake->ghost;
		//lấy tọa độ mới cho đuôi vì ghost luôn đc cập nhật bởi hàm snake_move()
		platform_deleteFood(food->coord.x, food->coord.y);
		//xóa food cũ

		snake->ghost.x = INVALID_COORDS;
		snake->ghost.y = INVALID_COORDS;
		snake->length++;
		//sau ăn thì xóa ghost và tăng độ dài
		food->state = EATEN;
		score++;
		memset(score_display, 0, sizeof(score_display));//khởi tạo lại giá trị cho score_display
		sprintf(score_display, "%d", score);
	}
}

void snake_display(snake_t* snake)
{
	if (INVALID_COORDS != snake->ghost.x && INVALID_COORDS != snake->ghost.y)
	{
		platform_eraseCell(snake->ghost.x, snake->ghost.y);//hàm xóa 1 Cell body của snake
	}
	for (int idx = 0; idx < snake->length; idx++)
	{

		platform_drawCell(snake->body[idx].x, snake->body[idx].y);//hàm vẽ 1 Cell body của snake
	}
}

void platform_drawCell(uint16_t x, uint16_t y)
{
	uint16_t x_new= ARENA_OFFSET_X + x*CELL_SIZE;//tọa độ x của cell snake
	uint16_t y_new= ARENA_OFFSET_Y + y*CELL_SIZE;//tọa độ y của cell snake
	for(uint8_t idx = x_new; idx < x_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, idx, y_new);
	}
	for(uint8_t idx = y_new; idx < y_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, x_new, idx);
	}
	for(uint8_t idx = x_new; idx < x_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, idx, y_new + CELL_SIZE-1);
	}
	for(uint8_t idx = y_new; idx < y_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, x_new + CELL_SIZE - 1, idx);
	}
}

void platform_eraseCell(uint16_t x, uint16_t y)
{
	uint16_t x_new= ARENA_OFFSET_X + x*CELL_SIZE;//tọa độ x đặt cell snake
	uint16_t y_new= ARENA_OFFSET_Y + y*CELL_SIZE;//tọa độ y đặt cell snake
	ucg_SetColor(&ucg1, 0, 0, 0, 0);
	for(uint8_t idx = x_new; idx < x_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, idx, y_new);
	}
	for(uint8_t idx = y_new; idx < y_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, x_new, idx);
	}
	for(uint8_t idx = x_new; idx < x_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, idx, y_new + CELL_SIZE-1);
	}
	for(uint8_t idx = y_new; idx < y_new + CELL_SIZE; idx++){
		ucg_DrawPixel(&ucg1, x_new + CELL_SIZE - 1, idx);
	}
	ucg_SetColor(&ucg1, 0, 255, 255, 255);
}

void platform_fatal(void)
{
	while (1){
		processTimerScheduler();
		ucg_DrawString(&ucg1, 50, 50, 0, "ERROR PROG");
	};
}

void snake_place_food(snake_t* snake, food_t* food, uint32_t tick)
{
	if (0 == gPrgCycle % 10 || food->time_elapsed)
	{
		if (food->state != PLACED)
		{
			if (GENERAL_ERROR == generate_food(snake, food))
			{
				platform_fatal();
			}
			else
			{
				food->time_elapsed = 0;
				food->state = PLACED;
			}
		}
		else
		{
			food->time_elapsed = 1;
		}
	}
}

uint16_t generate_food(snake_t* snake, food_t *food)
{
	uint16_t isInvalid = 0;
	uint16_t iter = 0;

	do
	{
		food->coord.x = (uint16_t)(((platform_init_randomizer()) % (FOOD_MAX_X - FOOD_MIN_X + 1)) + FOOD_MIN_X);
		food->coord.y = (uint16_t)(((platform_init_randomizer())% (FOOD_MAX_Y - FOOD_MIN_Y )) + FOOD_MIN_Y);
		//platform_randomize()
		for (int idx = 0; idx < snake->length; idx++)
		{
			if ((snake->body[idx].x == food->coord.x) &&
				(snake->body[idx].y == food->coord.y))
			{
				isInvalid = GENERAL_ERROR;
				break;
			}//else{isInvalid = 0;}
		}
		iter++;
		if (iter > FOOD_MAX_ITER)
		{
			break;
		}
	} while (isInvalid);

	if (!isInvalid)
	{
		platform_drawFood(food->coord.x, food->coord.y);//,
	}
	return isInvalid;
}

void platform_drawFood(uint16_t x, uint16_t y)
{
	uint16_t x_new= ARENA_OFFSET_X + x*CELL_SIZE + 1;//tọa độ x đặt food
	uint16_t y_new= ARENA_OFFSET_Y + y*CELL_SIZE + 1;//tọa độ y đặt food
	for(uint8_t idx_i = x_new; idx_i < x_new + FOOD_SIZE; idx_i++){
		for(uint8_t idx_j = y_new; idx_j < y_new + FOOD_SIZE; idx_j++)
		{
			ucg_DrawPixel(&ucg1, idx_i, idx_j);
		}
	}
}
void platform_deleteFood(uint16_t x, uint16_t y)
{
	ucg_SetColor(&ucg1, 0, 0, 0, 0);
	uint16_t x_new= ARENA_OFFSET_X + x*CELL_SIZE + 1;//tọa độ x đặt food
	uint16_t y_new= ARENA_OFFSET_Y + y*CELL_SIZE + 1;//tọa độ y đặt food
	for(uint8_t idx_i = x_new; idx_i < x_new + FOOD_SIZE; idx_i++){
		for(uint8_t idx_j = y_new; idx_j < y_new + FOOD_SIZE; idx_j++)
		{
			ucg_DrawPixel(&ucg1, idx_i, idx_j);
		}
	}
	ucg_SetColor(&ucg1, 0, 255, 255, 255);
}

void DeviceStateMachine(uint8_t event){
	switch (event) {
		default:
			break;
	}
}
