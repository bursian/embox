/**
 * @file stm32_spi.c
 * @brief
 * @author Andrew Bursian
 * @version
 * @date 22.06.2023
 */

#include <drivers/spi.h>
#include <util/log.h>
#include <string.h>

//#include "stm32_spi.h"

#include <drivers/gpio/gpio.h>
#include <config/board_config.h>
#include <stdint.h>

#if defined(STM32F407xx)
#	if !OPTION_MODULE_GET(platform__stm32__f4__stm32f4_discovery__arch,BOOLEAN,errata_spi_wrong_last_bit)
		#error errata_spi_wrong_last_bit is not enabled for SPI1! \
	       Please, enable this option in platform.stm32.f4.stm32f4_discovery.arch
#	endif
#include "stm32f4_discovery.h"
#elif defined (USE_STM32F429I_DISCOVERY)
#include "stm32f429i_discovery.h"
#elif defined (STM32F429xx)
#include "stm32f4xx_nucleo_144.h"
#elif defined (STM32L476xx)
#include "stm32l4xx_nucleo.h"
#elif defined (STM32L475xx)
#include "stm32l475e_iot01.h"
#else
#error Unsupported platform
#endif

struct stm32_spi {
	void *instance;
	SPI_HandleTypeDef handle;
	GPIO_TypeDef *nss_port;
	uint32_t nss_pin;
//	unsigned short nss_port;
//	gpio_mask_t nss_pin;
	uint16_t clk_div;
	uint16_t bits_per_word;
};
struct spi_ops stm32_spi_ops;

#define STM32_SPI_DEV_DEF(idx)					\
	static struct stm32_spi stm32_spi##idx = {		\
		.instance = SPI##idx,				\
		.nss_port = CONF_SPI##idx##_PIN_CS_PORT,	\
		.nss_pin  = CONF_SPI##idx##_PIN_CS_NR,		\
		.bits_per_word = CONF_SPI##idx##_BITS_PER_WORD,	\
		.clk_div       = CONF_SPI##idx##_CLK_DIV,	\
	}

#ifdef CONF_SPI0_ENABLED
STM32_SPI_DEV_DEF(0);
SPI_DEV_DEF("spi0", &stm32_spi_ops, &stm32_spi0, 0);
#endif

#ifdef CONF_SPI1_ENABLED
STM32_SPI_DEV_DEF(1);
SPI_DEV_DEF("spi1", &stm32_spi_ops, &stm32_spi1, 1);
#endif

#ifdef CONF_SPI2_ENABLED
STM32_SPI_DEV_DEF(2);
SPI_DEV_DEF("spi2", &stm32_spi_ops, &stm32_spi2, 2);
#endif

#ifdef CONF_SPI3_ENABLED
STM32_SPI_DEV_DEF(3);
SPI_DEV_DEF("spi3", &stm32_spi_ops, &stm32_spi3, 3);
#endif

#ifdef CONF_SPI4_ENABLED
STM32_SPI_DEV_DEF(4);
SPI_DEV_DEF("spi4", &stm32_spi_ops, &stm32_spi4, 4);
#endif

#ifdef CONF_SPI5_ENABLED
STM32_SPI_DEV_DEF(5);
SPI_DEV_DEF("spi5", &stm32_spi_ops, &stm32_spi5, 5);
#endif

#define STM32_SPI_INIT__(idx)													\
	do {															\
		gpio_setup_mode(CONF_SPI##idx##_PIN_SCK_PORT,  CONF_SPI##idx##_PIN_SCK_NR,  CONF_SPI##idx##_PIN_SCK_AF);	\
		gpio_setup_mode(CONF_SPI##idx##_PIN_MISO_PORT, CONF_SPI##idx##_PIN_MISO_NR, CONF_SPI##idx##_PIN_MISO_AF);	\
		gpio_setup_mode(CONF_SPI##idx##_PIN_MOSI_PORT, CONF_SPI##idx##_PIN_MOSI_NR, CONF_SPI##idx##_PIN_MOSI_AF);	\
		gpio_setup_mode(CONF_SPI##idx##_PIN_CS_PORT, CONF_SPI##idx##_PIN_CS_NR, GPIO_MODE_OUT);				\
		gpio_set(CONF_SPI##idx##_PIN_CS_PORT, CONF_SPI##idx##_PIN_CS_NR, GPIO_PIN_HIGH);				\
		CONF_SPI##idx##_CLK_ENABLE_SPI();										\
		stm32_spi_setup(&stm32_spi##idx, true);										\
	} while(0)
	
#define STM32_SPI_INIT(idx)													\
	do {															\
		GPIO_InitTypeDef  GPIO_InitStruct;										\
																\
		CONF_SPI##idx##_CLK_ENABLE_SCK();										\
		CONF_SPI##idx##_CLK_ENABLE_MISO();										\
		CONF_SPI##idx##_CLK_ENABLE_MOSI();										\
		CONF_SPI##idx##_CLK_ENABLE_CS();										\
		CONF_SPI##idx##_CLK_ENABLE_SPI();										\
																\
		memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));								\
		GPIO_InitStruct.Pin       = CONF_SPI##idx##_PIN_SCK_NR;								\
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;									\
		GPIO_InitStruct.Pull      = GPIO_NOPULL;									\
		GPIO_InitStruct.Speed     = GPIO_SPEED_FAST;									\
		GPIO_InitStruct.Alternate = CONF_SPI##idx##_PIN_SCK_AF;								\
		HAL_GPIO_Init(CONF_SPI##idx##_PIN_SCK_PORT, &GPIO_InitStruct);							\
																\
		GPIO_InitStruct.Pin = CONF_SPI##idx##_PIN_MISO_NR;								\
		GPIO_InitStruct.Alternate = CONF_SPI##idx##_PIN_MISO_AF;							\
		HAL_GPIO_Init(CONF_SPI##idx##_PIN_MISO_PORT, &GPIO_InitStruct);							\
																\
		GPIO_InitStruct.Pin = CONF_SPI##idx##_PIN_MOSI_NR;								\
		GPIO_InitStruct.Alternate = CONF_SPI##idx##_PIN_MOSI_AF;							\
		HAL_GPIO_Init(CONF_SPI##idx##_PIN_MOSI_PORT, &GPIO_InitStruct);							\
																\
		GPIO_InitStruct.Pin       = CONF_SPI##idx##_PIN_CS_NR;								\
		GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;								\
		GPIO_InitStruct.Alternate = CONF_SPI##idx##_PIN_CS_AF == -1? 0: CONF_SPI##idx##_PIN_CS_AF;			\
		HAL_GPIO_Init(CONF_SPI##idx##_PIN_CS_PORT, &GPIO_InitStruct);							\
																\
		HAL_GPIO_WritePin(CONF_SPI##idx##_PIN_CS_PORT, CONF_SPI##idx##_PIN_CS_NR, GPIO_PIN_SET);			\
		stm32_spi_setup(&stm32_spi##idx, true);										\
	} while(0)
	
// It may be "stm32_spi.h" up to this line

static int stm32_spi_setup(struct stm32_spi *dev, bool is_master) {
	SPI_HandleTypeDef *handle = &dev->handle;
	memset(handle, 0, sizeof(*handle));

	handle->Instance               = dev->instance;
	switch (dev->clk_div) {
	case 2:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
		break;
	case 4:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
		break;
	case 8:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
		break;
	case 32:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
		break;
	case 64:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
		break;
	case 128:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
		break;
	case 256:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
		break;
	case 16:
	default:
		handle->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
		break;
	}
	handle->Init.Direction         = SPI_DIRECTION_2LINES;
	handle->Init.CLKPhase          = SPI_PHASE_1EDGE;
	handle->Init.CLKPolarity       = SPI_POLARITY_LOW;
	handle->Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
	handle->Init.CRCPolynomial     = 7;
	if (dev->bits_per_word == 16) {
		handle->Init.DataSize          = SPI_DATASIZE_16BIT;
	} else {
		handle->Init.DataSize          = SPI_DATASIZE_8BIT;
	}
	handle->Init.FirstBit          = SPI_FIRSTBIT_MSB;
	handle->Init.NSS               = SPI_NSS_SOFT;
	handle->Init.TIMode            = SPI_TIMODE_DISABLE;

	if (is_master) {
		handle->Init.Mode      = SPI_MODE_MASTER;
	} else {
		handle->Init.Mode      = SPI_MODE_SLAVE;
	}

	if (HAL_OK != HAL_SPI_Init(handle)) {
		log_error("Failed to init SPI!");
		return -1;
	}


	return 0;
}

static int stm32_spi_init(struct spi_device *spi_dev) {
	switch(spi_dev_id(spi_dev)) {
#ifdef CONF_SPI0_ENABLED
	case 0: STM32_SPI_INIT(0); break;
#endif
#ifdef CONF_SPI1_ENABLED
	case 1: STM32_SPI_INIT(1); break;
#endif
#ifdef CONF_SPI2_ENABLED
	case 2: STM32_SPI_INIT(2); break;
#endif
#ifdef CONF_SPI3_ENABLED
	case 3: STM32_SPI_INIT(3); break;
#endif
#ifdef CONF_SPI4_ENABLED
	case 4: STM32_SPI_INIT(4); break;
#endif
#ifdef CONF_SPI5_ENABLED
	case 5: STM32_SPI_INIT(5); break;
#endif
	}

	return 0;
}

static int stm32_spi_select(struct spi_device *spi_dev, int cs) {
	struct stm32_spi *dev = spi_dev->priv;
	HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, cs);
	//gpio_set(dev->nss_port, dev->nss_pin, cs);
	return 0;
}

static int stm32_spi_set_mode(struct spi_device *spi_dev, bool is_master) {
	struct stm32_spi *dev = spi_dev->priv;
	return stm32_spi_setup(dev, is_master);
}

static int stm32_spi_transfer(struct spi_device *spi_dev, uint8_t *inbuf, uint8_t *outbuf, int count) {
	struct stm32_spi *dev = spi_dev->priv;
	SPI_HandleTypeDef *handle = &dev->handle;

	if (spi_dev->flags & SPI_CS_ACTIVE && spi_dev->is_master) {
		/* Note: we suppose that there's a single slave device
		 * on the SPI bus, so we lower the same pin all the time */
		HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_RESET);
		//gpio_set(dev->nss_port, dev->nss_pin, GPIO_PIN_LOW);
	}

	int ret = HAL_SPI_TransmitReceive(handle, inbuf, outbuf, count, 5000);
	if (ret != HAL_OK) {
		log_error("fail %d", ret);
	}

	while (HAL_SPI_GetState(handle) != HAL_SPI_STATE_READY) {
	}


	if (spi_dev->flags & SPI_CS_INACTIVE && spi_dev->is_master) {
		/* Note: we suppose that there's a single slave device
		 * on the SPI bus, so we raise the same pin all the time */
		HAL_GPIO_WritePin(dev->nss_port, dev->nss_pin, GPIO_PIN_SET);
		//gpio_set(dev->nss_port, dev->nss_pin, GPIO_PIN_HIGH);
	}

	return 0;
}

struct spi_ops stm32_spi_ops = {
	.init     = stm32_spi_init,
	.select   = stm32_spi_select,
	.set_mode = stm32_spi_set_mode,
	.transfer = stm32_spi_transfer
};
