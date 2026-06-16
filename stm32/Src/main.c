/*
 * TALLER V
 * TAREA 2 - CONTADOR CON FOTOINTERRUPTORES Y DISPLAY 7 SEGMENTOS
 * MELISSA RODRIGUEZ
 *
 * ---------------------FUNCIONAMIENTO-------------------------
 * - El display de 7 segmentos de 4 digitos muestra un contador de 0000 a 9999
 * - El fotointerruptor conectado a PA0 aumenta el contador --> lee flancos de subida 0 V - 3.3 V
 * - El fotointerruptor conectado a PA1 disminuye el contador --> lee flancos de bajada 3.3 V - 0 V
 * - Si el contador se encuentra en 9999 y suma, pasa a 0000
 * - Si el contador se encuentra en 0000 y resta, pasa a 9999
 * - Blinki funcionando en PH1 mediante interrupcion del TIM2 cada 500 ms
 *
 * -----------------------INFORMACION DEL DISPLAY-----------------------
 * - Display anodo comun
 * - Segmento encendido = LOW
 * - Segmento apagado   = HIGH
 * - Digito encendido   = LOW
 * - Digito apagado     = HIGH
 *
 * ---> CONEXIONES DEL DISPLAY
 *
 * Segmentos:
 * A -> PB12
 * B -> PA12
 * C -> PC13
 * D -> PD2
 * E -> PC11
 * F -> PA11
 * G -> PB7
 *
 * Punto decimal -> No conectado
 *
 * Digitos:
 * D1 / Miles     -> PC5
 * D2 / Centenas  -> PC12
 * D3 / Decenas   -> PC6
 * D4 / Unidades  -> PC10
 *
 * -------------------------FOTOINTERRUPTORES------------------------------
 * Sensor suma  -> PA0
 * Sensor resta -> PA1
 *
 * -------------------------LED BLINKI------------------------------
 * LED blinki -> PH1
 */

#include "stm32f411xe.h"

volatile int contador = 0;              // Variable principal del contador
volatile uint8_t digito_actual = 0;     // Indica que digito del display se esta refrescando

/* PROTOTIPOS DE FUNCIONES */
void delay(volatile uint32_t t);
void pin_high(GPIO_TypeDef *GPIOx, uint8_t pin);
void pin_low(GPIO_TypeDef *GPIOx, uint8_t pin);
void segmento(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t on);
void digito(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t on);

void GPIO_Init(void);
void EXTI_Init(void);
void TIM2_Init(void);

void apagar_digitos(void);
void apagar_segmentos(void);
void lightNumber(uint8_t n);
void encender_digito(uint8_t d);
void refrescar_display(void);

/* Retardo simple por software */
void delay(volatile uint32_t t)
{
    while(t--);
}

/* Coloca un pin en estado alto */
void pin_high(GPIO_TypeDef *GPIOx, uint8_t pin)
{
    GPIOx->ODR |= (1 << pin);
}

/* Coloca un pin en estado bajo */
void pin_low(GPIO_TypeDef *GPIOx, uint8_t pin)
{
    GPIOx->ODR &= ~(1 << pin);
}

/* CONTROL DEL 7 SEGMENTOS - Anodo comun
 * Segmento encendido = LOW
 * Segmento apagado   = HIGH
 */
void segmento(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t on)
{
    if(on)
        pin_low(GPIOx, pin);
    else
        pin_high(GPIOx, pin);
}

/* CONTROL DE DIGITOS
 * Digito encendido = LOW
 * Digito apagado   = HIGH
 */
void digito(GPIO_TypeDef *GPIOx, uint8_t pin, uint8_t on)
{
    if(on)
        pin_low(GPIOx, pin);
    else
        pin_high(GPIOx, pin);
}

/* INICIALIZACION DE LOS GPIO */
void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN |
                    RCC_AHB1ENR_GPIODEN |
                    RCC_AHB1ENR_GPIOHEN;

    /* PA0 y PA1 entradas fotointerruptores */
    GPIOA->MODER &= ~((3 << 0) | (3 << 2));     // PA0 y PA1 como entradas
    GPIOA->PUPDR &= ~((3 << 0) | (3 << 2));     // Limpia configuracion pull-up/pull-down
    GPIOA->PUPDR |=  ((1 << 0) | (1 << 2));     // PA0 y PA1 con pull-up interno

    /* PA11 = F, PA12 = B como salidas */
    GPIOA->MODER &= ~((3 << 22) | (3 << 24));
    GPIOA->MODER |=  ((1 << 22) | (1 << 24));

    /* PB7 = G, PB12 = A como salidas */
    GPIOB->MODER &= ~((3 << 14) | (3 << 24));
    GPIOB->MODER |=  ((1 << 14) | (1 << 24));

    /* PC5, PC6, PC10, PC11, PC12, PC13 como salidas */
    GPIOC->MODER &= ~((3 << 10) | (3 << 12) | (3 << 20) |
                      (3 << 22) | (3 << 24) | (3 << 26));

    GPIOC->MODER |=  ((1 << 10) | (1 << 12) | (1 << 20) |
                      (1 << 22) | (1 << 24) | (1 << 26));

    /* PD2 = D como salida */
    GPIOD->MODER &= ~(3 << 4);
    GPIOD->MODER |=  (1 << 4);

    /* PH1 = LED blinki como salida */
    GPIOH->MODER &= ~(3 << 2);
    GPIOH->MODER |=  (1 << 2);
}

/* INICIALIZACION DE INTERRUPCIONES EXTERNAS */
void EXTI_Init(void)
{
    /* Activa el reloj del bloque SYSCFG */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* PA0 y PA1 conectados a EXTI0 y EXTI1 */
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI1);

    /* Limpiar interrupciones pendientes */
    EXTI->PR |= EXTI_PR_PR0;
    EXTI->PR |= EXTI_PR_PR1;

    /* Habilitar interrupciones EXTI0 y EXTI1 */
    EXTI->IMR |= EXTI_IMR_IM0 | EXTI_IMR_IM1;

    /* PA0: flanco de subida */
    EXTI->RTSR |= EXTI_RTSR_TR0;
    EXTI->FTSR &= ~EXTI_FTSR_TR0;

    /* PA1: flanco de bajada */
    EXTI->FTSR |= EXTI_FTSR_TR1;
    EXTI->RTSR &= ~EXTI_RTSR_TR1;

    /* Habilitar interrupciones en NVIC */
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

/* INICIALIZACION DEL TIMER 2 PARA EL BLINKI */
void TIM2_Init(void)
{
    /* Activa el reloj del TIM2 */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /*
     * Configuracion del TIM2:
     * Reloj asumido = 16 MHz
     * Prescaler = 16000 - 1
     * 16 MHz / 16000 = 1000 Hz
     * Cada cuenta equivale aproximadamente a 1 ms
     */
    TIM2->PSC = 16000 - 1;

    /*
     * Periodo = 500 - 1
     * Interrupcion cada 500 ms aproximadamente
     */
    TIM2->ARR = 500 - 1;

    /* Reinicia el contador del timer */
    TIM2->CNT = 0;

    /* Habilita interrupcion por actualizacion */
    TIM2->DIER |= TIM_DIER_UIE;

    /* Habilita el TIM2 */
    TIM2->CR1 |= TIM_CR1_CEN;

    /* Habilita la interrupcion del TIM2 en NVIC */
    NVIC_EnableIRQ(TIM2_IRQn);
}

/* Apaga los 4 digitos */
void apagar_digitos(void)
{
    digito(GPIOC, 5, 0);   // D1
    digito(GPIOC, 12, 0);  // D2
    digito(GPIOC, 6, 0);   // D3
    digito(GPIOC, 10, 0);  // D4
}

/* Apaga todos los segmentos */
void apagar_segmentos(void)
{
    segmento(GPIOB, 12, 0); // A
    segmento(GPIOA, 12, 0); // B
    segmento(GPIOC, 13, 0); // C
    segmento(GPIOD, 2,  0); // D
    segmento(GPIOC, 11, 0); // E
    segmento(GPIOA, 11, 0); // F
    segmento(GPIOB, 7,  0); // G
}

/* Muestra un numero del 0 al 9 en el digito activo */
void lightNumber(uint8_t n)
{
    apagar_segmentos();

    switch(n)
    {
        case 0:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,1);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,0);
            break;

        case 1:
            segmento(GPIOB,12,0);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,0);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,0);
            segmento(GPIOB,7,0);
            break;

        case 2:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,0);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,1);
            segmento(GPIOA,11,0);
            segmento(GPIOB,7,1);
            break;

        case 3:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,0);
            segmento(GPIOB,7,1);
            break;

        case 4:
            segmento(GPIOB,12,0);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,0);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,1);
            break;

        case 5:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,0);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,1);
            break;

        case 6:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,0);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,1);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,1);
            break;

        case 7:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,0);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,0);
            segmento(GPIOB,7,0);
            break;

        case 8:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,1);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,1);
            break;

        case 9:
            segmento(GPIOB,12,1);
            segmento(GPIOA,12,1);
            segmento(GPIOC,13,1);
            segmento(GPIOD,2,1);
            segmento(GPIOC,11,0);
            segmento(GPIOA,11,1);
            segmento(GPIOB,7,1);
            break;
    }
}

/* Enciende uno de los cuatro digitos */
void encender_digito(uint8_t d)
{
    if(d == 0) digito(GPIOC, 5, 1);
    if(d == 1) digito(GPIOC, 12, 1);
    if(d == 2) digito(GPIOC, 6, 1);
    if(d == 3) digito(GPIOC, 10, 1);
}

/* Refresca el display por multiplexado */
void refrescar_display(void)
{
    int v = contador;

    uint8_t miles = v / 1000;
    uint8_t centenas = (v % 1000) / 100;
    uint8_t decenas = (v % 100) / 10;
    uint8_t unidades = v % 10;

    apagar_digitos();

    if(digito_actual == 0)
    {
        lightNumber(miles);
        encender_digito(0);
    }
    else if(digito_actual == 1)
    {
        lightNumber(centenas);
        encender_digito(1);
    }
    else if(digito_actual == 2)
    {
        lightNumber(decenas);
        encender_digito(2);
    }
    else
    {
        lightNumber(unidades);
        encender_digito(3);
    }

    digito_actual++;

    if(digito_actual >= 4)
        digito_actual = 0;
}

/* PROGRAMA PRINCIPAL */
int main(void)
{
    GPIO_Init();
    EXTI_Init();
    TIM2_Init();

    apagar_digitos();
    apagar_segmentos();

	/*
	* Refresco del display por multiplexado.
	* Cada llamada actualiza un solo dígito.
	* La velocidad de refresco depende del retardo.
	*/
    while(1)
    {
        refrescar_display();
        delay(2500);
    }
}

/* INTERRUPCION DEL PA0 - FOTOINTERRUPTOR DE SUMA */
void EXTI0_IRQHandler(void)
{
    if(EXTI->PR & EXTI_PR_PR0)
    {
        EXTI->PR |= EXTI_PR_PR0;

        contador++;

        if(contador > 9999)
            contador = 0;
    }
}

/* INTERRUPCION DEL PA1 - FOTOINTERRUPTOR DE RESTA */
void EXTI1_IRQHandler(void)
{
    if(EXTI->PR & EXTI_PR_PR1)
    {
        EXTI->PR |= EXTI_PR_PR1;

        if(contador > 0)
            contador--;
        else
            contador = 9999;
    }
}

/* INTERRUPCION DEL TIM2 - BLINKI CADA 500 ms */
void TIM2_IRQHandler(void)
{
    if(TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;

        GPIOH->ODR ^= (1 << 1);
    }
}
