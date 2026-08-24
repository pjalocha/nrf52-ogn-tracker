#ifndef _VARIANT_SEEED_XIAO_WIO_SX1262_
#define _VARIANT_SEEED_XIAO_WIO_SX1262_

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

#define PIN_LED1 (30u) /* P0.30, green LED */
#define LED_BUILTIN PIN_LED1
#define LED_GREEN PIN_LED1
#define LED_RED (26u)
#define LED_BLUE (6u)
#define LED_STATE_ON 0
#define LED_STATE_OFF 1

#define PIN_BUTTON1 (48u)

#define PIN_A0 (2u)
#define PIN_A1 (3u)
#define PIN_A2 (28u)
#define PIN_A3 (29u)
#define PIN_A4 (4u)
#define PIN_A5 (5u)
#define PIN_A6 (30u)
#define PIN_A7 (31u)
#define PIN_VBAT PIN_A7
#define ADC_RESOLUTION 14

#define PIN_SERIAL1_RX (44u) /* P1.12, GNSS TX -> nRF RX */
#define PIN_SERIAL1_TX (43u) /* P1.11, nRF TX -> GNSS RX */

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO (46u) /* P1.14 */
#define PIN_SPI_MOSI (47u) /* P1.15 */
#define PIN_SPI_SCK (45u)  /* P1.13 */

static const uint8_t SS = 4u; /* P0.04, SX1262 CS */
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (4u)
#define PIN_WIRE_SCL (5u)

#define PIN_QSPI_SCK (21u)
#define PIN_QSPI_CS (25u)
#define PIN_QSPI_IO0 (20u)
#define PIN_QSPI_IO1 (24u)
#define PIN_QSPI_IO2 (22u)
#define PIN_QSPI_IO3 (23u)

#define EXTERNAL_FLASH_USE_QSPI

#ifdef __cplusplus
}
#endif

#endif
