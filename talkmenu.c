// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The talking boot menu for the Xiegu X6100 multi-boot card (TomekR1's Xiegu6x00-Multi-Boot).
 *
 * A system of its own on the card, started by the screen menu when no key is pressed there. It runs as
 * /sbin/init, says the choices through the radio's speaker (recorded WAV files, 48 kHz stereo, on the
 * card's BOOT partition, folder TALK), waits for F1..F5, marks the choice for the boot script (the card's
 * sector 2: "BM" and the key's digit) and restarts: the screen menu then starts that system straight away.
 *
 * Sound: the standard ALSA player and mixer (aplay, amixer) of the system whose kernel this runs with
 * (its root partition, doom_modules= on the kernel's command line), run in a chroot of it, read-only.
 * (tinyalsa, as Doom's fallback uses it, never started the card here.) The base board's play mode
 * (register 12, bit 0x8) over I2C sends the codec's sound to the speaker, as OpenX6X00 does.
 *
 * BOOT/talkmenu.txt:
 *   default=N   the key started when nobody presses one (1..5)
 *   wait=S      seconds of talking before the default starts; 0: waits for a key
 *   volume=V    the radio's AF volume while it talks, 0..55
 *   headphone=H the codec's output level to the radio board, 0..63
 *   self=K      the talking menu's own key
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_KEYS  5
#define MAX_DEVS  12
#define POWER_KEY 100

static FILE *log_fp;
static int   cfg_default = 1, cfg_wait = 30, cfg_volume = 20, cfg_headphone = 47;
static bool  present[MAX_KEYS + 1];       /* the keys that have a system (TALK/STARTn.WAV there) */
static int   self_key;                    /* the talking menu's own key: it says the choices again */
static bool  donor_ok;                    /* the player's system mounted at /donor */
static uint32_t t0;

static uint32_t now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000) - t0;
}

static void logmsg(const char *fmt, ...) {
	va_list ap;
	if (!log_fp)
		return;
	fprintf(log_fp, "[%6.1f] ", now_ms() / 1000.0);
	va_start(ap, fmt);
	vfprintf(log_fp, fmt, ap);
	va_end(ap);
	fputc('\n', log_fp);
	fflush(log_fp);
}

static void screen(const char *fmt, ...) {
	va_list ap;
	FILE *f = fopen("/dev/tty0", "w");
	if (!f)
		return;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fclose(f);
}

static void read_settings(void) {
	FILE *f = fopen("/boot/talkmenu.txt", "r");
	char line[160];
	if (!f)
		return;
	while (fgets(line, sizeof line, f)) {
		int v;
		if (sscanf(line, "default=%d", &v) == 1) cfg_default = v;
		else if (sscanf(line, "wait=%d", &v) == 1) cfg_wait = v;
		else if (sscanf(line, "volume=%d", &v) == 1) cfg_volume = v;
		else if (sscanf(line, "headphone=%d", &v) == 1) cfg_headphone = v;
		else if (sscanf(line, "self=%d", &v) == 1) self_key = v;
	}
	fclose(f);
	if (cfg_wait < 0) cfg_wait = 0;
	if (cfg_volume < 0 || cfg_volume > 55) cfg_volume = 20;
	if (cfg_headphone < 0 || cfg_headphone > 63) cfg_headphone = 47;
}

/* ---------------------------------------------------------------------------------------------- */
/* The player's system: read-only at /donor, /dev /proc /sys and the BOOT partition (at /mnt) in it  */
/* ---------------------------------------------------------------------------------------------- */
static void mount_donor(void) {
	char cmd[1024] = "", dev[64] = "";
	FILE *f = fopen("/proc/cmdline", "r");
	if (f) { if (!fgets(cmd, sizeof cmd, f)) cmd[0] = '\0'; fclose(f); }
	char *p = strstr(cmd, "doom_modules=");
	if (p) sscanf(p + strlen("doom_modules="), "%63s", dev);
	if (!dev[0]) { logmsg("no player system on the command line"); return; }
	mkdir("/donor", 0755);
	for (int i = 0; i < 30; i++) {
		if (mount(dev, "/donor", "ext4", MS_RDONLY, NULL) == 0) { donor_ok = true; break; }
		usleep(100000);
	}
	if (!donor_ok) { logmsg("%s not mounted: %s", dev, strerror(errno)); return; }
	mount("/dev", "/donor/dev", NULL, MS_BIND, NULL);
	mount("/proc", "/donor/proc", NULL, MS_BIND, NULL);
	mount("/sys", "/donor/sys", NULL, MS_BIND, NULL);
	if (mount("/boot", "/donor/mnt", NULL, MS_BIND, NULL) != 0)
		logmsg("BOOT not bound into the player's system: %s", strerror(errno));
	mount("tmpfs", "/donor/tmp", "tmpfs", 0, "size=1m");
	logmsg("player system %s mounted", dev);
}

static void umount_donor(void) {
	if (!donor_ok) return;
	umount("/donor/tmp");
	umount("/donor/mnt");
	umount("/donor/sys");
	umount("/donor/proc");
	umount("/donor/dev");
	umount("/donor");
	donor_ok = false;
}

/* A program of the player's system, started in its chroot; its pid */
static pid_t spawn(char *const argv[]) {
	if (!donor_ok) return -1;
	pid_t pid = fork();
	if (pid == 0) {
		int null = open("/dev/null", O_RDWR);
		if (null >= 0) { dup2(null, 0); dup2(null, 1); }
		if (log_fp) dup2(fileno(log_fp), 2);
		if (chroot("/donor") == 0 && chdir("/") == 0)
			execv(argv[0], argv);
		_exit(127);
	}
	return pid;
}

static int run(char *const argv[]) {
	int st = -1;
	pid_t pid = spawn(argv);
	if (pid > 0) waitpid(pid, &st, 0);
	return st;
}

static void cset(const char *name, const char *value) {
	char n[96];
	snprintf(n, sizeof n, "name=%s", name);
	char *argv[] = {"/usr/bin/amixer", "-q", "-c", "0", "cset", n, (char *)value, NULL};
	if (run(argv) != 0) logmsg("amixer %s: failed", name);
}

static void set_mixer(void) {
	char hp[8];
	snprintf(hp, sizeof hp, "%d", cfg_headphone);
	cset("AIF1 DA0 Playback Volume", "161");
	cset("AIF1 Slot 0 Digital DAC Playback Switch", "on");
	cset("AIF1 DA0 Stereo Playback Route", "Mix Mono");
	cset("Headphone Source Playback Route", "DAC");
	cset("Headphone Playback Switch", "on");
	cset("Headphone Playback Volume", hp);
}

/* The base board over I2C as the radio's software does it (bus 0, 0x72): register address big-endian,
 * value little-endian */
static bool base_write(unsigned reg, uint32_t val) {
	int fd = open("/dev/i2c-0", O_RDWR);
	if (fd < 0) { logmsg("/dev/i2c-0: %s", strerror(errno)); return false; }
	uint16_t addr = (uint16_t)(reg * 4);
	uint8_t buf[6] = {(uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)val, (uint8_t)(val >> 8),
	                  (uint8_t)(val >> 16), (uint8_t)(val >> 24)};
	struct i2c_msg msg = {.addr = 0x72, .flags = 0, .len = sizeof buf, .buf = buf};
	struct i2c_rdwr_ioctl_data d = {.msgs = &msg, .nmsgs = 1};
	bool ok = ioctl(fd, I2C_RDWR, &d) >= 0;
	if (!ok) logmsg("base register %u: %s", reg, strerror(errno));
	close(fd);
	return ok;
}

/* ---------------------------------------------------------------------------------------------- */
/* Keys                                                                                             */
/* ---------------------------------------------------------------------------------------------- */
static struct pollfd pfd[MAX_DEVS];
static int n_in;

static void open_inputs(void) {
	for (int tries = 0; tries < 30 && n_in == 0; tries++) {
		DIR *d = opendir("/dev/input");
		struct dirent *e;
		while (d && (e = readdir(d)) && n_in < MAX_DEVS) {
			char p[300];
			if (strncmp(e->d_name, "event", 5)) continue;
			snprintf(p, sizeof p, "/dev/input/%s", e->d_name);
			int fd = open(p, O_RDONLY | O_NONBLOCK);
			if (fd >= 0) {
				pfd[n_in].fd = fd;
				pfd[n_in].events = POLLIN;
				n_in++;
			}
		}
		if (d) closedir(d);
		if (!n_in) usleep(100000);
	}
	logmsg("%d input devices", n_in);
}

/* A key pressed within ms: 1..5 for F1..F5, POWER_KEY, 0 none */
static int wait_key(int ms) {
	int got = 0;
	if (poll(pfd, n_in, ms) <= 0)
		return 0;
	for (int i = 0; i < n_in; i++) {
		struct input_event ev;
		if (!(pfd[i].revents & POLLIN)) continue;
		while (read(pfd[i].fd, &ev, sizeof ev) == sizeof ev) {
			if (ev.type != EV_KEY || ev.value != 1 || got) continue;
			switch (ev.code) {
			case BTN_TRIGGER_HAPPY13: got = 1; break;
			case BTN_TRIGGER_HAPPY14: got = 2; break;
			case BTN_TRIGGER_HAPPY15: got = 3; break;
			case BTN_TRIGGER_HAPPY19: got = 4; break;
			case BTN_TRIGGER_HAPPY20: got = 5; break;
			case KEY_POWER:           got = POWER_KEY; break;
			default:                  logmsg("key %u (not a choice)", ev.code);
			}
		}
	}
	return got;
}

/* ---------------------------------------------------------------------------------------------- */
/* Playing, with the keys looked at meanwhile                                                       */
/* ---------------------------------------------------------------------------------------------- */
/* Plays TALK/<name> (NULL: silence for ms); a key pressed meanwhile stops it and is returned */
static int play(const char *name, int silence_ms, bool keys) {
	if (!name) {
		uint32_t end = now_ms() + (uint32_t)silence_ms;
		while (now_ms() < end) {
			int k = keys ? wait_key(50) : (usleep(50000), 0);
			if (k) return k;
		}
		return 0;
	}
	char path[64], host[64];
	snprintf(host, sizeof host, "/boot/TALK/%s", name);
	if (access(host, R_OK) != 0) { logmsg("no %s", host); return 0; }
	snprintf(path, sizeof path, "/mnt/TALK/%s", name);
	char *argv[] = {"/usr/bin/aplay", "-q", "-D", "hw:0,0", path, NULL};
	uint32_t start = now_ms();
	pid_t pid = spawn(argv);
	if (pid <= 0) { logmsg("aplay not started"); return play(NULL, 3000, keys); }
	for (;;) {
		int st;
		if (waitpid(pid, &st, WNOHANG) == pid) {
			if (!WIFEXITED(st) || WEXITSTATUS(st))
				logmsg("%s: aplay ended with %d after %.1f s", name, st, (now_ms() - start) / 1000.0);
			return 0;
		}
		int k = keys ? wait_key(50) : (usleep(50000), 0);
		if (k) {
			kill(pid, SIGTERM);
			waitpid(pid, &st, 0);
			return k;
		}
	}
}

/* ---------------------------------------------------------------------------------------------- */
/* Leaving                                                                                          */
/* ---------------------------------------------------------------------------------------------- */
static void leave(int key) {
	if (key == POWER_KEY) {
		logmsg("power off");
		screen("\n  Power off\n");
	} else {
		logmsg("start F%d", key);
		int fd = open("/dev/mmcblk0", O_RDWR | O_SYNC);
		unsigned char sec[512] = {'B', 'M', (unsigned char)('0' + key)};
		if (fd < 0 || pwrite(fd, sec, sizeof sec, 2 * 512) != (ssize_t)sizeof sec)
			logmsg("the mark not written: %s", strerror(errno));
		if (fd >= 0) { fsync(fd); close(fd); }
	}
	base_write(12, key == POWER_KEY ? 0x10000u : 0);   /* play mode off; or the base board's power-off */
	umount_donor();
	if (log_fp) { fclose(log_fp); log_fp = NULL; }
	sync();
	umount("/boot");
	sync();
	if (key == POWER_KEY) {
		sleep(1);
		reboot(RB_POWER_OFF);
	}
	reboot(RB_AUTOBOOT);
	for (;;) pause();
}

int main(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	t0 = (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
	signal(SIGCHLD, SIG_DFL);

	mkdir("/proc", 0755); mkdir("/sys", 0755); mkdir("/dev", 0755); mkdir("/boot", 0755);
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
	for (int i = 0; i < 30; i++) {
		if (mount("/dev/mmcblk0p1", "/boot", "vfat", MS_NOATIME, NULL) == 0) break;
		usleep(100000);
	}
	log_fp = fopen("/boot/talkmenu-log.txt", "w");
	logmsg("talking menu");
	read_settings();
	for (int k = 1; k <= MAX_KEYS; k++) {
		char p[64];
		snprintf(p, sizeof p, "/boot/TALK/START%d.WAV", k);
		present[k] = access(p, R_OK) == 0 && k != self_key;
	}
	if (cfg_default < 1 || cfg_default > MAX_KEYS || !present[cfg_default]) cfg_default = 1;
	logmsg("default F%d, wait %d s, volume %d, own key F%d", cfg_default, cfg_wait, cfg_volume, self_key);

	screen("\033[2J\033[H\n  TALKING BOOT MENU\n\n  Press F1 to F4 to start a system.\n");
	if (cfg_wait > 0)
		screen("  No key: F%d starts after %d seconds.\n", cfg_default, cfg_wait);

	mount_donor();
	set_mixer();
	base_write(12, 0x8);                  /* play mode: the codec's sound to the speaker */
	base_write(14, (uint32_t)cfg_volume); /* AF volume */
	open_inputs();
	wait_key(0);                          /* what was pressed before this started */
	logmsg("ready");

	char wait_clip[16];
	snprintf(wait_clip, sizeof wait_clip, "WAIT%d.WAV", cfg_default);
	uint32_t deadline = cfg_wait > 0 ? now_ms() + (uint32_t)cfg_wait * 1000 : 0;
	int key = 0;
	for (;;) {
		key = play("MENU.WAV", 0, true);
		if (!key && cfg_wait > 0) key = play(wait_clip, 0, true);
		if (!key) key = play(NULL, 3000, true);
		if (!key && deadline && now_ms() >= deadline) {
			key = cfg_default;
			logmsg("no key: the default");
		}
		if (key == POWER_KEY) {
			play("OFF.WAV", 0, false);
			leave(key);
		}
		if (key >= 1 && key <= MAX_KEYS && present[key])
			break;
		if (key) {
			logmsg("F%d: no system there, the choices again", key);
			key = 0;
		}
	}
	screen("\n  Starting F%d ...\n", key);
	char start_clip[16];
	snprintf(start_clip, sizeof start_clip, "START%d.WAV", key);
	play(start_clip, 0, false);
	leave(key);
	return 0;
}
