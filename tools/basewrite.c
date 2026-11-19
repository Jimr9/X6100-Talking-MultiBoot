// basewrite REG VALUE: one base board register over I2C, as the talking menu writes it
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc < 3) return 2;
	unsigned reg = (unsigned)strtoul(argv[1], NULL, 0);
	uint32_t val = (uint32_t)strtoul(argv[2], NULL, 0);
	int fd = open("/dev/i2c-0", O_RDWR);
	if (fd < 0) { perror("i2c"); return 1; }
	uint16_t addr = (uint16_t)(reg * 4);
	uint8_t buf[6] = {(uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)val, (uint8_t)(val >> 8),
	                  (uint8_t)(val >> 16), (uint8_t)(val >> 24)};
	struct i2c_msg msg = {.addr = 0x72, .flags = 0, .len = sizeof buf, .buf = buf};
	struct i2c_rdwr_ioctl_data d = {.msgs = &msg, .nmsgs = 1};
	int r = ioctl(fd, I2C_RDWR, &d);
	printf("reg %u = 0x%x: %s\n", reg, val, r < 0 ? "failed" : "ok");
	close(fd);
	return r < 0;
}
