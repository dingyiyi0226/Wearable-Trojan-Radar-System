#include "gpio.h"
#include "sysfs.h"

#define POUT 23 /* P1-16 */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 1e6;
static uint16_t delay = 0;

static uint8_t tx[5] = {0xF0, 0xF0, 0xF0, 0xF0, 0xF0};
static uint8_t tx2[5] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
static uint8_t rx[ARRAY_SIZE(tx)] = {0, };

static void 
pabort(const char *s)
{
    perror(s);
    abort();
}

static int
transfer(int fd, struct spi_ioc_transfer *tr)
{
    return ioctl(fd, SPI_IOC_MESSAGE(1), tr);
}

static void 
print_usage(const char *prog)
{
	printf("Usage: %s [-DsbdlHOLC3]\n", prog);
	puts("  -D --device   device to use (default /dev/spidev1.1)\n"
	     "  -s --speed    max speed (Hz)\n"
	     "  -d --delay    delay (usec)\n"
	     "  -b --bpw      bits per word \n"
	     "  -l --loop     loopback\n"
	     "  -H --cpha     clock phase\n"
	     "  -O --cpol     clock polarity\n"
	     "  -L --lsb      least significant bit first\n"
	     "  -C --cs-high  chip select active high\n"
	     "  -3 --3wire    SI/SO signals shared\n");
	exit(1);
}

static void 
parse_opts(int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "device",  1, 0, 'D' },
			{ "speed",   1, 0, 's' },
			{ "delay",   1, 0, 'd' },
			{ "bpw",     1, 0, 'b' },
			{ "loop",    0, 0, 'l' },
			{ "cpha",    0, 0, 'H' },
			{ "cpol",    0, 0, 'O' },
			{ "lsb",     0, 0, 'L' },
			{ "cs-high", 0, 0, 'C' },
			{ "3wire",   0, 0, '3' },
			{ "no-cs",   0, 0, 'N' },
			{ "ready",   0, 0, 'R' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "D:s:d:b:lHOLC3NR", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'H':
			mode |= SPI_CPHA;
			break;
		case 'O':
			mode |= SPI_CPOL;
			break;
		case 'L':
			mode |= SPI_LSB_FIRST;
			break;
		case 'C':
			mode |= SPI_CS_HIGH;
			break;
		case '3':
			mode |= SPI_3WIRE;
			break;
		case 'N':
			mode |= SPI_NO_CS;
			break;
		case 'R':
			mode |= SPI_READY;
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

int 
sysfsMain(int argc, char **argv)
{
	int ret = 0;
	int fd;

	parse_opts(argc, argv);

	fd = open(device, O_RDWR);
	if (fd < 0)
		pabort("can't open device");

	/*
	 * spi mode
	 */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/*
	 * bits per word
	 */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/*
	 * max speed hz
	 */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

    printf("output device: %s\n", device);
	printf("spi mode: %d\n", mode);
	printf("bits per word: %d\n", bits);
    printf("bytes to send: %d\n", ARRAY_SIZE(tx));
	printf("max speed: %d Hz (%d KHz)\n", speed, speed / 1000);

    for (ret = 0; ret < ARRAY_SIZE(tx); ++ret) {
    	printf("%.2X ", tx[ret]);
    }

    printf("\n");

    for (ret = 0; ret < ARRAY_SIZE(tx2); ++ret) {
        printf("%.2X ", tx2[ret]);
    }

    printf("\n");

    struct spi_ioc_transfer tr = { 
        .tx_buf = (unsigned long)tx, 
        .rx_buf = (unsigned long)rx, 
        .len = ARRAY_SIZE(tx), 
        .speed_hz = speed,
        .delay_usecs = delay,
        .bits_per_word = bits,
    };

    struct spi_ioc_transfer tr2 = {
        .tx_buf = (unsigned long)tx2,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx2),
        .speed_hz = speed,
        .delay_usecs = delay,
        .bits_per_word = bits,
    };

    // Enable GPIO
    if (GPIOExport(POUT) == -1)
        return (1);

    if (GPIODirection(POUT, OUT) == -1)
        return (2);

    ret = 1;
	while (ret >= 1) 
    { 
        ret = transfer(fd, &tr); 
        GPIOWrite(POUT, 1);
        usleep(1);
        GPIOWrite(POUT, 0);
        
        ret = transfer(fd, &tr2);
        GPIOWrite(POUT, 1);
        usleep(1);
        GPIOWrite(POUT, 0);
    }

    // Disable GPIO
    if (GPIOUnexport(POUT) == -1)
        return (4);

	close(fd);

	return ret;
}
