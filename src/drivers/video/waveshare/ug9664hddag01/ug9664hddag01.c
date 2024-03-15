/**
 * @file
 * @brief 
 *
 * @date 
 * @author 
 */

#include <string.h>
#include <unistd.h>

#include <drivers/video/fb.h>
#include <kernel/time/timer.h>
#include <drivers/gpio/gpio.h>
#include <drivers/spi.h>

#include <util/log.h>
#include <embox/unit.h>

#define SPI_BUS OPTION_GET(NUMBER,spi_bus)
#define RST_PORT GPIO_PORT_D
#define RST_PIN 0
#define DATA_CMD_PORT GPIO_PORT_D
#define DATA_CMD_PIN 2

EMBOX_UNIT_INIT(ug9664hddag01_init);

static uint16_t mem_fb[64][96];

static int ug9664hddag01_set_var(struct fb_info *info, struct fb_var_screeninfo const *var) {
	return 0;
}

static int ug9664hddag01_get_var(struct fb_info *info, struct fb_var_screeninfo *var) {
	memset(var, 0, sizeof(*var));
	var->xres = 96;
	var->yres = 64;
	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;
	var->bits_per_pixel = 16;
	var->fmt = BGR565;
	return 0;
}

static void ug9664hddag01_fillrect(struct fb_info *info, const struct fb_fillrect *rect) {
}

static void ug9664hddag01_imageblit(struct fb_info *info, const struct fb_image *image) {
}

static struct fb_ops ug9664hddag01_ops = {
	.fb_set_var = ug9664hddag01_set_var,
	.fb_get_var = ug9664hddag01_get_var,
	.fb_fillrect  = ug9664hddag01_fillrect,
	.fb_imageblit = ug9664hddag01_imageblit,
};

static void ug9664hddag01_update(struct spi_device * dev, int x, int y, int w, int h) {
	uint8_t in[2], out;
	
	// set start/end column & row
	spi_transfer(dev, (uint8_t*)"\x15", &in[0], 1);
	out = x;
	spi_transfer(dev, &out, &in[0], 1);
	out = x + w - 1;
	spi_transfer(dev, &out, &in[0], 1);
	
	spi_transfer(dev, (uint8_t*)"\x75", &in[0], 1);
	out = y;
	spi_transfer(dev, &out, &in[0], 1);
	out = y + h - 1;
	spi_transfer(dev, &out, &in[0], 1);
	
	// Send fb rectangle
	gpio_set(DATA_CMD_PORT, 1<<DATA_CMD_PIN, GPIO_PIN_HIGH);
	for (int j = y; j < y + h; j++) {
		for (int i = x; i < x + w; i++) {
			spi_transfer(dev, (uint8_t*)&mem_fb[j][i], in, 2);
		}
	}
	gpio_set(DATA_CMD_PORT, 1<<DATA_CMD_PIN, GPIO_PIN_LOW);
}

static void timer_handler(sys_timer_t* timer, void *param) {
	// Copy fb to display
	ug9664hddag01_update((struct spi_device *)param, 0, 0, 96, 64);
}

static void ug9664hddag01_reset(void) {
	gpio_set(RST_PORT, 1<<RST_PIN, GPIO_PIN_LOW);
	usleep(500);
	gpio_set(RST_PORT, 1<<RST_PIN, GPIO_PIN_HIGH);
	usleep(500);
}

static int ug9664hddag01_init(void) {
	struct fb_info *info;
	struct spi_device *spi_dev;
	uint8_t in;

	sys_timer_t * timer;

	// Display initialization
	gpio_setup_mode(RST_PORT, 1<<RST_PIN, GPIO_MODE_OUT);
	gpio_set(RST_PORT, 1<<RST_PIN, GPIO_PIN_HIGH);
	ug9664hddag01_reset();
	gpio_setup_mode(DATA_CMD_PORT, 1<<DATA_CMD_PIN, GPIO_MODE_OUT);
	gpio_set(DATA_CMD_PORT, 1<<DATA_CMD_PIN, GPIO_PIN_LOW);

	// SPI initialization
	spi_dev = spi_dev_by_id(SPI_BUS);
	if (spi_dev == NULL) {
		log_error("Failed to select spi bus #%d", SPI_BUS);
		return -1;
	}
	spi_dev->is_master = true;
	spi_dev->flags |= SPI_CS_ACTIVE;
	spi_dev->flags |= SPI_CS_INACTIVE;

	// Display controller initialization
	spi_transfer(spi_dev, (uint8_t*)"\xAF", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\xA0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x60", &in, 1);
	usleep(1000000);
	spi_transfer(spi_dev, (uint8_t*)"\x25", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x5F", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x3F", &in, 1);
	usleep(1000000);
	spi_transfer(spi_dev, (uint8_t*)"\x21", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x5F", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x3F", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x0", &in, 1);
	spi_transfer(spi_dev, (uint8_t*)"\x1f", &in, 1);
	usleep(1000000);

	// Frame buffer initialization
	for (int j = 0; j < 64; j++) {
		for (int i = 0; i < 96; i++) {
			mem_fb[j][i] = i*i+j*j>910?0:i*i+j*j>889?0xFFFF:0;//i==j?0x0208:0;
		}
	}
	info = fb_create(&ug9664hddag01_ops, (void*)mem_fb, 96*64*2);
	if (info == NULL) {
		return -1;
	}
	
	// Timer to copy fb to display
	if (timer_set(&timer, TIMER_PERIODIC, 2000 /* ms */, timer_handler, spi_dev)) {
		log_error("timer init failed");
		return -1;
	}

	return 0;
}
