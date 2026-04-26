/*
 * ATmega2560 @ 16 MHz + WS2812B (800 kHz)
 * Czysty C (avr-gcc), bez bibliotek Arduino.
 *
 * Wyjscie danych: PB7 (pin cyfrowy D13 na Arduino Mega2560)
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define LED_COUNT   16

#define WS_PORT     PORTB
#define WS_DDR      DDRB
#define WS_PIN_BIT  PB7

/*
 * Czy wylaczac przerwania podczas ws_show().
 * 1 = zalecane (stabilna transmisja WS2812B)
 * 0 = tylko gdy MASZ pewnosc, ze ISR nie zaburza timingu (rzadko na AVR)
 */
#ifndef WS2812_DISABLE_IRQ
#define WS2812_DISABLE_IRQ 1
#endif

/* Bufor koloru: WS2812B uzywa kolejnosci GRB */
static uint8_t led_buf[LED_COUNT][3];

static inline void ws_pin_high(void) { WS_PORT |=  (1 << WS_PIN_BIT); }
static inline void ws_pin_low(void)  { WS_PORT &= ~(1 << WS_PIN_BIT); }

/*
 * Nadanie pojedynczego bitu z przyblizona kalibracja do 16 MHz.
 * 1 bit = 1.25 us.
 * "1": TH ~0.8 us, TL ~0.45 us
 * "0": TH ~0.4 us, TL ~0.85 us
 */
static inline void ws_send_bit(uint8_t bit)
{
    if (bit) {
        ws_pin_high();
        /* ~0.8 us high */
        __asm__ __volatile__(
            "nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
            "nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
        );
        ws_pin_low();
        /* ~0.45 us low */
        __asm__ __volatile__(
            "nop\n\t" "nop\n\t" "nop\n\t"
        );
    } else {
        ws_pin_high();
        /* ~0.4 us high */
        __asm__ __volatile__(
            "nop\n\t" "nop\n\t" "nop\n\t"
        );
        ws_pin_low();
        /* ~0.85 us low */
        __asm__ __volatile__(
            "nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
            "nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
        );
    }
}

static inline void ws_send_byte(uint8_t b)
{
    for (uint8_t m = 0x80; m; m >>= 1) {
        ws_send_bit(b & m);
    }
}

/* Wyslanie calej ramki do LED (GRB) */
void ws_show(void)
{
#if WS2812_DISABLE_IRQ
    uint8_t sreg = SREG;
    cli();
#endif

    for (uint16_t i = 0; i < LED_COUNT; i++) {
        ws_send_byte(led_buf[i][0]); /* G */
        ws_send_byte(led_buf[i][1]); /* R */
        ws_send_byte(led_buf[i][2]); /* B */
    }

#if WS2812_DISABLE_IRQ
    SREG = sreg;
#endif

    /* Reset latch: >50 us low */
    for (volatile uint16_t i = 0; i < 1100; i++) {
        __asm__ __volatile__("nop");
    }
}

void ws_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= LED_COUNT) return;
    led_buf[idx][0] = g;
    led_buf[idx][1] = r;
    led_buf[idx][2] = b;
}

void ws_clear(void)
{
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        led_buf[i][0] = 0;
        led_buf[i][1] = 0;
        led_buf[i][2] = 0;
    }
}

static void delay_ms_soft(uint16_t ms)
{
    /* Proste opoznienie programowe (wystarczajace do demo) */
    while (ms--) {
        for (volatile uint16_t i = 0; i < 4000; i++) {
            __asm__ __volatile__("nop");
        }
    }
}

int main(void)
{
    WS_DDR |= (1 << WS_PIN_BIT);
    ws_pin_low();

    ws_clear();
    ws_show();

    uint8_t pos = 0;

    while (1) {
        ws_clear();

        /* Prosta animacja: przesuwajacy sie piksel RGB */
        ws_set_pixel(pos, 255, 0, 0);
        ws_set_pixel((pos + 1) % LED_COUNT, 0, 255, 0);
        ws_set_pixel((pos + 2) % LED_COUNT, 0, 0, 255);

        ws_show();
        delay_ms_soft(80);

        pos++;
        if (pos >= LED_COUNT) pos = 0;
    }
}
