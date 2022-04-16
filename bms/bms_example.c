
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/epoll.h>

#include <linux/serial.h>

struct tty_data {
    int start;
    int avail;
    uint8_t *buf;
    int fd;
};


int main(void)
{
    struct termios options = {0};

    int tty = open("/dev/ttyS3", O_RDWR);

    if (tty < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    }

    if (isatty(tty) == 0) {
        printf("isatty: %i error: %s\n", isatty(tty), strerror(errno));
    }

    options.c_iflag = IUTF8 | IMAXBEL | IUCLC | INPCK | IGNPAR;
    options.c_oflag = 0;
    options.c_cflag = B115200 | CLOCAL | CREAD | CS8;
    options.c_lflag = 0;
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 0;


    printf("c_iflag: %08x\nc_oflag: %08x\nc_cflag: %08x\nc_lflag: %08x\n",
            options.c_iflag, options.c_oflag, options.c_cflag, options.c_lflag);

    tcsetattr(tty, 0, &options);

    fcntl(tty, F_SETFD, O_NONBLOCK);

    int epfd = epoll_create(1);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    // ev.data.fd = tty;

    struct epoll_event events[32];

    uint8_t buffer[100];

    struct tty_data data = {0, 0, buffer, tty};

    ev.data.ptr = &data;

    epoll_ctl(epfd, EPOLL_CTL_ADD, tty, &ev);

    while(1) {
        int nfds = epoll_wait(epfd, events, 32, -1);

        for (int i = 0; i < nfds; i++) {
            // printf("%i %i %p\n", i, nfds, events[i].data.ptr);

            struct tty_data *data = events[i].data.ptr;

            // printf("%i %i %i %p\n", data->start, data->avail, data->fd, data->buf);

            data->avail += read(data->fd, data->buf + data->avail, 100 - data->avail);

            printf("%i %i %2i [", i, nfds, data->avail);

            for (int j = 0; j < data->avail; j++) {
                printf("%02X ", buffer[j]);
            }
            printf("]\n");

            int ret = parse_data(data);
            if (ret < 0) {
                return -1;
            }

            printf("---------------------------------\n");
        }
    }

    return 0;
}

struct batt_msg {
    // uint8_t length;
    // uint8_t type;
    uint16_t bms_volt;
    uint16_t batt_curr_raw;
    uint16_t batt_temp_10x;
    uint8_t bat_soc;
    uint8_t status;
    uint8_t power_supply;
    uint8_t batt_health;
    uint16_t batt_loop_number;
    uint8_t powerboard_status;
};

int parse_pkt(uint8_t *buf)
{
    uint8_t length = buf[0];
    uint8_t type = buf[1];

    if (type == 0x05 && length == 0x0D) {
        struct batt_msg *b = (struct batt_msg *)(buf + 2);

        int batt_curr_raw = b->batt_curr_raw;
        int curr = (int16_t)(((batt_curr_raw & 0x3FFFU) * 4 + (batt_curr_raw & 0x7FFFU)) << 1);

        printf("bms_volt: %umV\n", b->bms_volt);
        printf("batt_curr_raw: %imA (%u)\n", curr, b->batt_curr_raw);
        printf("batt_temp_10x: %0.1fC (%u)\n", b->batt_temp_10x / 10.0f, b->batt_temp_10x);
        printf("bat_soc: %u%%\n", b->bat_soc);
        printf("status: %u\n", b->status);
        printf("power_supply: %u\n", b->power_supply);
        printf("batt_health: %u\n", b->batt_health);
        printf("batt_loop_number: %u\n", b->batt_loop_number);
        printf("powerboard_status: %u\n", b->powerboard_status);

    } else {
        printf("unsupport packet type\n");
    }

    return 0;
}

int parse_data(struct tty_data *data)
{
    static int errcnt = 0;
    uint8_t *buf = data->buf;

    while (data->start + 3 < data->avail) {

        uint8_t pkt_len = 0;
        uint16_t pkt_sum = 0;

        if (errcnt > 5) {
            return -1;
        }
        
        int i = data->start;

        if (buf[i] == 0x5A && buf[i + 1] == 0xA5) {

            i += 2;

            pkt_len = buf[i];

            // i += 1;

            pkt_sum = *((uint16_t *)(buf + i + 1 + pkt_len));

            if (i + 1 +  pkt_len + 2 > data->avail) {
                break;
            }

            uint16_t cal_sum = 0;
            for (int j = 0; j < 1 + pkt_len; j++) {
                cal_sum += buf[i + j];
            }

            if (pkt_sum != cal_sum) {
                errcnt += 1;
                printf(" ****** length: %u\n", pkt_len);
                printf(" ****** %04X != %04X SKIP\n", pkt_sum, cal_sum);
            }

            parse_pkt(buf + i);

            i += 1 + pkt_len + 2;
        } else {
            printf("skip %02x\n", buf[i]);
            i += 1;
        }

        data->start += i;
        data->avail -= i;
    }

    // copy available bytes to start of buffer
    memcpy(buf, buf + data->start, data->avail);
    data->start = 0;

    return data->avail;
}
