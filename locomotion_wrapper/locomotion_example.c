#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/timerfd.h>
#include <sys/epoll.h>

#include "locomotion_wrapper.h"

int example_frame(uint64_t count)
{
    spi_data_t spi_data;
    spi_command_t spi_command;

    memset(&spi_command, 0, sizeof(spi_command_t));
    memset(&spi_data, 0, sizeof(spi_data_t));


    // spi_command.q_des_knee[2] = 0.1;
    // spi_command.qd_des_knee[2] = 0;
    // spi_command.kp_knee[2] = 100;
    // spi_command.kd_knee[2] = 2;
    // spi_command.tau_knee_ff[2] = 0;

    // sleep 3s, enable motor
    if (count > 3000) {
        spi_command.flags[0] = 1;
        spi_command.flags[1] = 1;
        spi_command.flags[2] = 1;
        spi_command.flags[3] = 1;
    }

    locomotion_flush(&spi_command, &spi_data);

    if ((count % 500) == 0) {
        for(int i = 0; i < 4; i++) {
            printf("leg %i:P %+8.2f %+8.2f %+8.2f\n", i, spi_data.q_abad[i], spi_data.q_hip[i], spi_data.q_knee[i]);
            printf("leg %i:V %+8.2f %+8.2f %+8.2f\n", i, spi_data.qd_abad[i], spi_data.qd_hip[i], spi_data.qd_knee[i]);
            printf("leg %i:T %+8.2f %+8.2f %+8.2f\n", i, spi_data.tau_abad[i], spi_data.tau_hip[i], spi_data.tau_knee[i]);
            printf("leg %i:S %08x %08x %08x\n", i, spi_data.flags[i], spi_data.flags[i + 1], spi_data.flags[i + 2]);
        }
        printf("status: %08x count: %u\n", spi_data.spi_driver_status, (spi_data.spi_driver_status >> 16) - 1);
        printf("----------------\n");

        // locomotion_debug_status();
    }
}

#define MAX_EVENTS 8

int main(void)
{
    if (locomotion_init() < 0) {
        return -1;
    }

    int timerfd;
    int epollfd;

    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    struct itimerspec timerspec;
    timerspec.it_interval.tv_sec = 0;
    timerspec.it_interval.tv_nsec = 2 * 1000 * 1000; // 2ms/500Hz
    timerspec.it_value.tv_sec = 1;
    timerspec.it_value.tv_nsec = 0;

    timerfd_settime(timerfd, 0, &timerspec, 0);

    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = timerfd;

    epollfd = epoll_create(1);

    epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev);

    uint64_t count = 0;

    while(1) {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == timerfd) {
                int ret;
                uint64_t val;
                ret = read(timerfd, &val, sizeof(val));

                example_frame(count++);
            } else {
                printf("ignored event\n");
            }
        }
    }

    close(timerfd);
    close(epollfd);
}
