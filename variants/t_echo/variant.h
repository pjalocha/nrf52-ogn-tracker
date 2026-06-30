#ifndef _VARIANT_T_ECHO_
#define _VARIANT_T_ECHO_

#include "WVariant.h"

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#ifdef __cplusplus
extern "C" {
#endif

/* Keep Arduino pin numbers identical to nRF port/pin numbers. This matches
 * the _PINNUM(port, pin) convention used by the tracker pin headers. */
#define PINS_COUNT (48u)
#define NUM_DIGITAL_PINS (48u)
#define NUM_ANALOG_INPUTS (8u)
#define NUM_ANALOG_OUTPUTS (0u)

#define PIN_LED1 (35u) /* P1.03, red LED on common T-Echo rev2 boards */
#define LED_BUILTIN PIN_LED1
#define LED_STATE_ON 0
#define LED_STATE_OFF 1

#define PIN_BUTTON1 (42u) /* P1.10, active low */

#define PIN_A0 (2u)
#define PIN_A1 (3u)
#define PIN_A2 (4u)
#define PIN_A3 (28u)
#define PIN_A4 (29u)
#define PIN_A5 (30u)
#define PIN_A6 (31u)
#define PIN_A7 (5u)
#define PIN_VBAT PIN_A2
#define ADC_RESOLUTION 14

#define PIN_SERIAL1_RX (41u) /* P1.09, GNSS TX -> nRF RX */
#define PIN_SERIAL1_TX (40u) /* P1.08, nRF TX -> GNSS RX */

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO (23u)
#define PIN_SPI_MOSI (22u)
#define PIN_SPI_SCK (19u)

static const uint8_t SS = 24u; /* P0.24, SX1262 CS */
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (26u)
#define PIN_WIRE_SCL (27u)

#define PIN_QSPI_SCK (46u) /* P1.14 */
#define PIN_QSPI_CS (47u)  /* P1.15 */
#define PIN_QSPI_IO0 (44u) /* P1.12, MOSI */
#define PIN_QSPI_IO1 (45u) /* P1.13, MISO */
#define PIN_QSPI_IO2 (7u)  /* P0.07, WP */
#define PIN_QSPI_IO3 (5u)  /* P0.05, HOLD */

#define EXTERNAL_FLASH_USE_QSPI

#ifdef __cplusplus
}
#endif

#endif
