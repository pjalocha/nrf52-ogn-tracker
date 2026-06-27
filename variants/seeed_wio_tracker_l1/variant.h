#ifndef _VARIANT_SEEED_WIO_TRACKER_L1_
#define _VARIANT_SEEED_WIO_TRACKER_L1_

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

#define PIN_LED1 (33u) /* P1.01, green LED */
#define LED_BUILTIN PIN_LED1
#define LED_STATE_ON 1

#define PIN_BUTTON1 (8u) /* P0.08, active low */

#define PIN_A0 (2u)
#define PIN_A1 (3u)
#define PIN_A2 (28u)
#define PIN_A3 (29u)
#define PIN_A4 (4u)
#define PIN_A5 (5u)
#define PIN_A6 (6u)
#define PIN_A7 (31u)
#define ADC_RESOLUTION 14

#define PIN_SERIAL1_RX (26u) /* P0.26, GNSS TX -> nRF RX */
#define PIN_SERIAL1_TX (27u) /* P0.27, nRF TX -> GNSS RX */

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO (3u)
#define PIN_SPI_MOSI (28u)
#define PIN_SPI_SCK (30u)

static const uint8_t SS = 46u; /* P1.14, SX1262 CS */
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (6u)
#define PIN_WIRE_SCL (5u)

#define PIN_QSPI_SCK (21u)
#define PIN_QSPI_CS (25u)
#define PIN_QSPI_IO0 (20u)
#define PIN_QSPI_IO1 (24u)
#define PIN_QSPI_IO2 (22u)
#define PIN_QSPI_IO3 (23u)

#ifdef __cplusplus
}
#endif

#endif
