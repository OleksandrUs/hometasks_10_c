/*
 * homework, main.c
 * Purpose: create tasks and delete; assign task priority change priority; create idle task hook.
 * 
 * There are to tasks created in the program from the very begining. The first task is used 
 * to read periodically the state of the user button. The second task calculates the CPU load
 * and this calculation in based on the task profiler values. The idle task hook is also implemented.
 * Each time the user button is pressed, a new task to control LEDs (toggle LEDs state) is created if
 * it hasn't exist or the task is deleted if it has already been created. If a task is active,
 * the color LEDs is blinking.

 * @author Oleksandr Ushkarenko
 * @version 1.0 19/09/2021
 */

#include "stm32f3xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * These identifiers are used to determine the microcontroller pins
 * the eight color the LEDs connected to.
 */
#define BLUE_LED_1		GPIO_PIN_8
#define RED_LED_1 		GPIO_PIN_9
#define ORANGE_LED_1 	GPIO_PIN_10
#define GREEN_LED_1		GPIO_PIN_11
#define BLUE_LED_2 		GPIO_PIN_12
#define RED_LED_2 		GPIO_PIN_13
#define ORANGE_LED_2	GPIO_PIN_14
#define GREEN_LED_2 	GPIO_PIN_15

/*
 * This identifier is used to determine the microcontroller pin
 * the user button connected to.
 */
#define BUTTON_PIN		GPIO_PIN_0

/*
 * The size of the stack (in 4-byte words) for created tasks.
 */
#define STACK_SIZE 64U

/*
 * The maximum number of tasks that can be created when the program is running
 * (one task per one of four colored LEDs, i.e. red, green, blue, orange).
 */
#define MAX_TASKS_NUMBER 4

/*
 * The period of time of each task execution.
 */
#define DELAY pdMS_TO_TICKS(100)

/*
 * The period of time between two CPU load calcutations.
 */
#define CALC_PERIOD pdMS_TO_TICKS(100)

/*
 * The period of user button state checks.
 */
#define READ_BUTTON_STATE_PERIOD pdMS_TO_TICKS(200)

/*
 * These identifiers are used to specify task priorities.
 */
 #define LED_CTRL_TASK_PRIORITY 1
 #define BUTTON_READ_STATE_TASK_PRIORITY 2
 #define CPU_LOAD_CALC_TASK_PRIORITY 3

/*
 * The array is used to store the task handlers created at runtime.
 */
 TaskHandle_t task_handlers[MAX_TASKS_NUMBER] = { NULL, NULL, NULL, NULL };
 
/*
 * The variable is used for indexing of task_handlers array.
 */
 volatile uint32_t th_index = 0;
 
 /*
 * The variable contains the calculated value of the CPU load (in percents).
 */
 volatile float cpu_load = 0;
 
/*
 * These variables are used to count the number of different task functions invocations,
 * including idle task hook, in order to calculate (roughly) the CPU load.
 */
 volatile uint32_t red_led_ctrl_task_profiler = 0;
 volatile uint32_t green_led_ctrl_task_profiler = 0;
 volatile uint32_t blue_led_ctrl_task_profiler = 0;
 volatile uint32_t orange_led_ctrl_task_profiler = 0;
 volatile uint32_t read_button_state_task_profiler = 0;
 volatile uint32_t idle_task_profiler = 0;
 
/*
 * Declaration of the function prototypes. The detailed description of
 * each of the function is above function implementation in this file.
 */
void GPIO_Init(void);
void read_button_state_task(void * param);
void red_led_ctrl_task(void * param);
void green_led_ctrl_task(void * param);
void blue_led_ctrl_task(void * param);
void orange_led_ctrl_task(void * param);
void cpu_load_calc_task(void * param);
void manage_tasks(uint32_t index);
void error_handler(void);

/*
 * The main function of the program (the entry point).
 */
int main()
{
	GPIO_Init();
	
	BaseType_t result;
	
	result = xTaskCreate(read_button_state_task, "Read button state task", 
											 STACK_SIZE, NULL, BUTTON_READ_STATE_TASK_PRIORITY, NULL);
	if(result != pdPASS){
		error_handler();
	}
	
	result = xTaskCreate(cpu_load_calc_task, "CPU load calculation task", 
											 STACK_SIZE, NULL, CPU_LOAD_CALC_TASK_PRIORITY, NULL);
	if(result != pdPASS){
		error_handler();
	}
	
	vTaskStartScheduler();
	while(1) { }
}

/*
 * The function is used to initialize I/O pins of port E (GPIOE). 
 * All microcontroller pins the LEDs connected to configured to output.
 * A microcontroller pin the user button connected to configured to input.
 * The push-pull mode for output pins is used, no pull-ups.
 */
void GPIO_Init()
{
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 
	| GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	
	GPIO_InitTypeDef gpio_init_struct;
	gpio_init_struct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
												 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOE, &gpio_init_struct);
	
	gpio_init_struct.Pin = GPIO_PIN_0;
	gpio_init_struct.Mode = GPIO_MODE_INPUT;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}

/*
 * The function is used to check periodically the state of the button and
 * each time the button is pressed the function manage_tasks is called to create or
 * delete a task.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void read_button_state_task(void * param)
{
	while(1){
		
		if(HAL_GPIO_ReadPin(GPIOA, BUTTON_PIN) == GPIO_PIN_SET) {
			if(th_index < (MAX_TASKS_NUMBER - 1)) {
				th_index++;
			} else {
				th_index = 0;
			}
			manage_tasks(th_index);
		}
		vTaskDelay(READ_BUTTON_STATE_PERIOD);
	}
}

/*
 * The function is used to change periodically the state (on and off) of red LEDs.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void red_led_ctrl_task(void * param)
{
	while(1){
		HAL_GPIO_TogglePin(GPIOE, RED_LED_1 | RED_LED_2);
		red_led_ctrl_task_profiler++;
		vTaskDelay(DELAY);
	}
}

/*
 * The function is used to change periodically the state (on and off) of green LEDs.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void green_led_ctrl_task(void * param)
{
	while(1){
		HAL_GPIO_TogglePin(GPIOE, GREEN_LED_1 | GREEN_LED_2);
		green_led_ctrl_task_profiler++;
		vTaskDelay(DELAY);
	}
}

/*
 * The function is used to change periodically the state (on and off) of blue LEDs.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void blue_led_ctrl_task(void * param)
{
	while(1){
		HAL_GPIO_TogglePin(GPIOE, BLUE_LED_1 | BLUE_LED_2);
		blue_led_ctrl_task_profiler++;
		vTaskDelay(DELAY);
	}
}

/*
 * The function is used to change periodically the state (on and off) of orange LEDs.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void orange_led_ctrl_task(void * param)
{
	while(1){
		HAL_GPIO_TogglePin(GPIOE, ORANGE_LED_1 | ORANGE_LED_2);
		orange_led_ctrl_task_profiler++;
		vTaskDelay(DELAY);
	}
}

/*
 * This function calculates the value of the CPU load. The estimate of the CPU load is based on the
 * values of task profilers. This estimate is very rough and only used to demonstrate one of the possible uses
 * the idle task hook.
 *
 * @param a value that is passed as the parameter to the created task.
 */
void cpu_load_calc_task(void * param)
{
	uint32_t total = 0;
	uint32_t useful = 0;
	while(1) {
		total = red_led_ctrl_task_profiler + green_led_ctrl_task_profiler + blue_led_ctrl_task_profiler
									+ orange_led_ctrl_task_profiler + read_button_state_task_profiler + idle_task_profiler;
		
		// value idle_task_profiler is not taken into account
		useful = red_led_ctrl_task_profiler + green_led_ctrl_task_profiler + blue_led_ctrl_task_profiler
									+ orange_led_ctrl_task_profiler + read_button_state_task_profiler;
		
		cpu_load = (float) 100 * useful / total;
		
		red_led_ctrl_task_profiler = 0;
		green_led_ctrl_task_profiler = 0;
		blue_led_ctrl_task_profiler = 0;
		orange_led_ctrl_task_profiler = 0;
		read_button_state_task_profiler = 0;
		idle_task_profiler = 0;
		
		vTaskDelay(CALC_PERIOD);
	}		
}

/*
 * The function is used as an error handler: if an error occures, this function
 * is invoked and two red LEDs on board will be switched on.
 */
void error_handler(void)
{
	HAL_GPIO_WritePin(GPIOE, RED_LED_1 | RED_LED_2, GPIO_PIN_SET);
	while(1){	}
}

/*
 * The function is used to create or delete a task depending on the value 
 * of a task handle stored in the task_handlers array (if it is NULL, a new
 * task will be created, otherwise the task will be deleted and NULL
 * will be put into the array of task handlers to the spefied position).
 *
 * @param index the index of a task handle in the task_handlers array
 */
void manage_tasks(uint32_t index) {
	
	BaseType_t result;
	
	switch(index){
		case 0:
			if(task_handlers[index] == NULL){
				result = xTaskCreate(red_led_ctrl_task, "Red LED control task", 
														 STACK_SIZE, NULL, LED_CTRL_TASK_PRIORITY, &task_handlers[index]);
					if(result != pdPASS){
						error_handler();
					}
			} else {
				vTaskDelete(task_handlers[index]);
				HAL_GPIO_WritePin(GPIOE, RED_LED_1 | RED_LED_2, GPIO_PIN_RESET);
				task_handlers[index] = NULL;
			}
			break;
		case 1:
			if(task_handlers[index] == NULL){
				result = xTaskCreate(green_led_ctrl_task, "Green LED control task", 
														 STACK_SIZE, NULL, LED_CTRL_TASK_PRIORITY, &task_handlers[index]);
				if(result != pdPASS){
						error_handler();
				}
			} else {
				vTaskDelete(task_handlers[index]);
				HAL_GPIO_WritePin(GPIOE, GREEN_LED_1 | GREEN_LED_2, GPIO_PIN_RESET);
				task_handlers[index] = NULL;
			}
		break;
		case 2:
			if(task_handlers[index] == NULL){
				result = xTaskCreate(blue_led_ctrl_task, "Blue LED control task", 
														 STACK_SIZE, NULL, LED_CTRL_TASK_PRIORITY, &task_handlers[index]);
				if(result != pdPASS){
						error_handler();
				}
			} else {
				vTaskDelete(task_handlers[index]);
				HAL_GPIO_WritePin(GPIOE, BLUE_LED_1 | BLUE_LED_2, GPIO_PIN_RESET);
				task_handlers[index] = NULL;
			}
			break;
		case 3:
			if(task_handlers[index] == NULL){
				result = xTaskCreate(orange_led_ctrl_task, "Orange LED control task", 
														 STACK_SIZE, NULL, LED_CTRL_TASK_PRIORITY, &task_handlers[index]);
				if(result != pdPASS){
						error_handler();
				}
			} else {
				vTaskDelete(task_handlers[index]);
				HAL_GPIO_WritePin(GPIOE, ORANGE_LED_1 | ORANGE_LED_2, GPIO_PIN_RESET);
				task_handlers[index] = NULL;
			}
			break;
		default:
				error_handler();
			break;
	}
}

/*
 * This function is the idle task hook and it is called automatically
 * when there are no tasks in the ready state.
 *
 * @param a value that is passed as the parameter to the idle hook.
 */ 
void vApplicationIdleHook(void *param)
{
	idle_task_profiler++;
}
