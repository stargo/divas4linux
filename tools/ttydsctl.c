#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DIVATTY_CTL_IOCTL 0x3701

void usage(char *progname)
{
	fprintf(stderr, "Usage: %s <ttyid> <command>\n\n", progname);
	fprintf(stderr, "ttyid     Number or name of the TTY\n");
	fprintf(stderr, "command   Command to execute\n\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "status  Show status of TTY\n");
	fprintf(stderr, "dcd     Carrier detect, returns OK when connected or an error on no connection\n");
	fprintf(stderr, "dtr     Clears DTR\n");
	fprintf(stderr, "h       Hangup TTY\n");
	fprintf(stderr, "z       Reset TTY\n");
	fprintf(stderr, "at...   Execute AT command (not when connected)\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int devno;
	char *devid;
	char *endptr;
	int fd;
	int ret;
	char cmd[128] = { 0 };

	if (argc != 3 || strlen(argv[1]) == 0 || strlen(argv[2]) == 0) {
		usage(argv[0]);
	}

	devid = strstr(argv[1], "ttyds");
	if (devid != NULL) {
		if (strlen(devid) > 5) {
			devid += 5;
		} else {
			usage(argv[0]);
		}
	} else {
		devid = argv[1];
	}
	
	devno = strtoul(devid, &endptr, 10);
	if (endptr == NULL || *endptr != '\0') {
		usage(argv[0]);
	}

	/* ttyds0 is the control interface */
	fd = open("/dev/ttyds0", O_NONBLOCK|O_RDWR);
	if (fd == -1) {
		perror("Open /dev/ttyds0");
		exit(EXIT_FAILURE);
	}

	snprintf(cmd, sizeof(cmd), "%d:%s", devno, argv[2]);

	ret = ioctl(fd, DIVATTY_CTL_IOCTL, cmd);
	if (ret < 0) {
		perror("ioctl");
		exit(EXIT_FAILURE);
	}

	if (ret > 0) {
		printf("%s\n", cmd);
	} else {
		printf("OK\n");
	}

	close(fd);
	exit(EXIT_SUCCESS);
}
