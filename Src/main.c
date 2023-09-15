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
#define LCD_WIDTH	104
#define LCD_HEIGHT	104
#define SNAKE_MAX_LNG		(uint16_t)(100)
#define SNAKE_INIT_LNG		(uint16_t)(3)
#define INVALID_COORDS		(uint16_t)(-1)
#define SNAKE_INIT_X_CORD	(uint16_t)(1)
#define SNAKE_INIT_Y_CORD	(uint16_t)(10)
#define SNAKE_WON_LIMIT		(uint16_t)(SNAKE_MAX_LNG - 1)


#define ARENA_MAX_X			(uint16_t)(25)
#define ARENA_MAX_Y			(uint16_t)(23)
#define ARENA_MIN_X			(uint16_t)(0)
#define ARENA_MIN_Y			(uint16_t)(0)

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
	coord_t ghost;
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
uint8_t eCurrentState = 0;
static uint8_t IdTimer = NO_TIMER;
static uint16_t gRandSeed;
char extKeyBoardButton;


//static uint8_t score;
//static char score_display[20] = "";
//uint32_t gPrgCycle = GetMilSecTick();
/******************************************************************************/
/*                              EXPORTED DATA                                 */
/******************************************************************************/

/******************************************************************************/
/*                            PRIVATE FUNCTIONS                               */
/******************************************************************************/
static void AppInitCommon(void);
static state_app_t GetStateApp(void);
static void SetStateApp(state_app_t state);
static void AppStateManager(uint8_t event);
static void LoadConfiguration(void);
static void platform_init_randomizer(void);
uint32_t RandomNumberGenerate(void);
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
/******************************************************************************/

/**
  * @brief  RNG configuration
  * @param  None
  * @retval None
  */

int main(void) {
	AppInitCommon();
	SetStateApp(STATE_APP_STARTUP); // Khởi động ở chế độ STATE_APP_STARTUP
	EventSchedulerAdd(EVENT_APP_INIT); //Khởi tạo bộ đệm buffer để quản lý các sự kiện của chương trình chính

	/* Loop forever */
	while (1) {
		processTimerScheduler(); 			// Xử lý các bộ hẹn giờ đã được tạo
		processEventScheduler();			// Xử lý các sự kiện của chương trình chính do bộ quản lý sự kiện quản lý.
		platform_init_randomizer();			//tạo gRandSeed
	  	snake_t snake = { 0 };
	  	//food_t food = { 0 };
	  	//uint32_t gPrgCycle = 0;
	  	snake_init(&snake);
	  	for(;;)
	  	{
	  		platform_get_control(&snake);
	  		snake_move(&snake);
	  		if (snake.state != PLAYING) break;
	  	}



	}

}



static void AppInitCommon(void) {
	SystemCoreClockUpdate(); 				//Initializes system clock để cấu hình clock của hệ thống
	TimerInit();							// Initializes system tick để xử lý các sự kiện thời gian.


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
	ucg_SetColor(&ucg, 0, 100, 100, 100);
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
	IdTimer = TimerStart("ClearScreenAndSetup", 1500, 0, SetupPrg, NULL);
}

void SetupPrg(){
	if(IdTimer != NO_TIMER){
		ucg_ClearScreen(&ucg);
		ucg_DrawFrame(&ucg1, 0, 0, 127, 117);
		ucg_DrawString(&ucg1, 0, 127, 0, "Score: 0");
		ucg_DrawBox(&ucg1, 20, 20, 20, 20);
		ucg_DrawBox(&ucg, 40, 40, 20, 20);
		TimerStop(IdTimer);
	}

}

void snake_init(snake_t* snake)
{
	snake->length = SNAKE_INIT_LNG;
	snake->direction = UP;
	snake->state = PLAYING;
	snake->ghost.x = INVALID_COORDS;
	snake->ghost.y = INVALID_COORDS;

	memset(&snake->body[0], 0, SNAKE_MAX_LNG*sizeof(coord_t));

	for (int idx = 0; idx < SNAKE_INIT_LNG; idx++)
	{
		snake->body[idx].x = SNAKE_INIT_X_CORD + idx;
		snake->body[idx].y = SNAKE_INIT_Y_CORD;
	}

	//platform_refresh_hw();
    //snake_diplay_borders();
}

//tạo gRandSeed để tạo số ngẫu nhiên cho vị trí food
static void platform_init_randomizer(void)
{
	/* This randomizer is based on ADC noise
	 * as a LFSR seed number */
	gRandSeed = LightSensor_MeasureUseDMAMode();
	while(gRandSeed < 0x8000) gRandSeed += gRandSeed;
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

//    if(!HAL_GPIO_ReadPin(GPIOx, GPIO_PIN_x))extKeyBoardButton='P';
//    if(!HAL_GPIO_ReadPin(GPIOx, GPIO_PIN_x))extKeyBoardButton='Q';

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
	}
	else
	{
		if (direction == PAUSE)
		{
			if (snake->direction != PAUSE)
			{
				prev_direction = snake->direction;
				snake->direction = PAUSE;
			}
			else
			{
				snake->direction = prev_direction;
			}
		}

		else
		{
			if ((snake->direction != PAUSE) &&
				!(snake->direction == LEFT && direction == RIGHT) &&
				!(snake->direction == RIGHT && direction == LEFT) &&
				!(snake->direction == UP && direction == DOWN) &&
				!(snake->direction == DOWN && direction == UP))
			{
				snake->direction = direction;
			}
		}
	}
}

void snake_move(snake_t* snake)
{
	if (NULL == snake || PAUSE == snake->direction)
	{
		return;
	}
	//luôn lưu tọa độ đuôi để khi ăn được food sẽ sử dụng tăng độ dài
	snake->ghost = snake->body[0];

	/*di chuyển theo hướng hiện tại -> lưu length-1 tọa độ lại ngoại trừ phần đầu,
	direction luôn đc cập nhật bởi hàm snake_control sẽ quyết định cho hướng đi
	tiêp theo -> tạo ra tọa độ mới cho phần đầu*/
	memcpy(&snake->body[0], &snake->body[1], sizeof(coord_t) * (snake->length - 1));

	switch (snake->direction)
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
	//break;?
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
	{
		/* Needed temporary copy for shifting the whole array right - for embedded*/
		//lưu toàn bộ tọa độ của body vào biến tạm
		coord_t tempSnake[SNAKE_MAX_LNG] = {0};
		memcpy(tempSnake, &(snake->body[0]), (size_t)snake->length*sizeof(coord_t));

		/* Just append the ghost to the end, increment length and disable ghost*/
		//dịch toàn bộ tọa độ body sang 1 body
		memcpy(&(snake->body[1]), tempSnake, (size_t)snake->length*sizeof(coord_t));
		snake->body[0] = snake->ghost; //lấy tọa độ mới cho đuôi vì ghost luôn đc cập nhật bởi hàm snake_move()
		snake->ghost.x = INVALID_COORDS;
		snake->ghost.y = INVALID_COORDS;
		snake->length++;//sau ăn thì xóa ghost và tăng độ dài

		food->state = EATEN;
	}
}

//void snake_display(snake_t* snake)
//{
//	if (INVALID_COORDS != snake->ghost.x && INVALID_COORDS != snake->ghost.y)
//	{
//		platform_eraseCell(snake->ghost.x, snake->ghost.y);
//	}
//	for (int idx = 0; idx < snake->length; idx++)
//	{
//		platform_drawCell(snake->body[idx].x, snake->body[idx].y);
//	}
//}

uint32_t RandomNumberGenerate(void)
{
	/* Wait until one RNG number is ready */
	while(RNG_GetFlagStatus(RNG_FLAG_DRDY)== RESET)
	{
	}

	/* Get a 32bit Random number */
	return RNG_GetRandomNumber();
}

void DeviceStateMachine(uint8_t event){
	switch (event) {
		case EVENT_OF_BUTTON_1_PRESS_LOGIC:
		{

		}
			break;
		default:
			break;
	}
}
