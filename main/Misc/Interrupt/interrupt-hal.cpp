#include "esp_err.h"
#include "hal/gpio_hal.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdlib.h>
#include "sindarin-debug.h"

extern "C" {

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
typedef struct {
    voidFuncPtr fn;
    void* arg;
    bool functional;
} InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[SOC_GPIO_PIN_COUNT] = {0,};

extern void cleanupFunctional(void* arg);

static void __onPinInterrupt(void * arg) {
	InterruptHandle_t * isr = (InterruptHandle_t*)arg;
    if(isr->fn) {
        if(isr->arg){
            ((voidFuncPtrArg)isr->fn)(isr->arg);
        } else {
        	isr->fn();
        }
    }
}

extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional)
{
    static bool interrupt_initialized = false;

    if(!interrupt_initialized) {
    	esp_err_t err = gpio_install_isr_service((int)0);
    	interrupt_initialized = (err == ESP_OK) || (err == ESP_ERR_INVALID_STATE);
    }
    if(!interrupt_initialized) {
    	log_e("GPIO ISR Service Failed To Start");
    	return;
    }

    // if new attach without detach remove old info
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = (voidFuncPtr)userFunc;
    __pinInterruptHandlers[pin].arg = arg;
    __pinInterruptHandlers[pin].functional = functional;

    gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    if(intr_type & 0x8){
    	gpio_wakeup_enable((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    }
    gpio_isr_handler_add((gpio_num_t)pin, __onPinInterrupt, &__pinInterruptHandlers[pin]);


    //FIX interrupts on peripherals outputs (eg. LEDC,...)
    //Enable input in GPIO register
    gpio_hal_context_t gpiohal;
    gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);
    gpio_hal_input_enable(&gpiohal, (gpio_num_t)pin);
}

}