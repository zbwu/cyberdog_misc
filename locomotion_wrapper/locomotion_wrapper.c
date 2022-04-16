
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <dlfcn.h>

#include <lcm/lcm.h>

#include "locomotion_wrapper.h"

// 0x2f56d0-0x2F6090, size 2496, 32byte align
typedef struct spi_handler_block_t {
    uint8_t SpiParameters[0x7D0]; // 2000bytes
    // 0x5f0 // Eigen::Matrix<double, 8, 1, 0, 8, 1> const&, findHipOffset
    // spi_command_t offset 0x7d0, length 0x100(256Bytes)
    spi_command_t spi_command;
    // spi_data_t offset 0x8d0, length 0x0c4
    spi_data_t spi_data;
    uint32_t spine2spi_count; // x994 count += 1, spine2spi count
    uint8_t status[4]; // x998/x999/x99a/x99b bool, findHipOffset
    int32_t spi0_fd; // x99c; // fd = spidev0.0 
    int32_t spi1_fd; // x9a0; // fd = spidev0.1
    uint32_t driver_run_count; // x9a4;
} __attribute__ ((aligned (32))) spi_handler_block_t;

static spi_handler_block_t spi_handler_block;
static spi_handler_block_t *spi_handler = &spi_handler_block;

static void (*spi_handler_constructor)(spi_handler_block_t *this) = 0;
static void (*spi_handler_destructor)(spi_handler_block_t *this) = 0;
static void (*spi_handler_driver_run)(spi_handler_block_t *this) = 0;
static void (*spi_handler_flush_spi_buff)(spi_handler_block_t *this) = 0;
static void (*spi_handler_get_spi_data)(spi_handler_block_t *this, spi_data_t *spi_data) = 0;
static void (*spi_handler_init_spi)(spi_handler_block_t *this) = 0;
static void (*spi_handler_publish_spi)(spi_handler_block_t *this, lcm_t *lcm) = 0;
static void (*spi_handler_reset_spi)(spi_handler_block_t *this) = 0;
static void (*spi_handler_set_spi_command)(spi_handler_block_t *this, spi_command_t *spi_command) = 0;
static void (*spi_handler_set_zero)(spi_handler_block_t *this) = 0;
static void (*spi_handler_spi_to_spine)(spi_handler_block_t *this, spi_command_t *spi_command, void *spine_command, int leg_offset) = 0;
static void (*spi_handler_spine_to_spi)(spi_handler_block_t *this, spi_data_t *spi_data, void *spine_data, int leg_offset) = 0;
static void (*spi_handler_resolve_error_flags)(int id, int flag, char *out) = 0;
static void (*spi_handler_resolve_warn_flags)(int id, int flag, char *out) = 0;
static void (*spi_handler_spine_cmd_to_spi_cmd)(spi_handler_block_t *this, spi_command_t *spi_command, void *spine_command, int leg_offset) = 0;
static void (*spi_handler_spi_data_to_spine_data)(spi_handler_block_t *this, spi_data_t *spi_data, void *spine_data, int leg_offset) = 0;
static void (*spi_handler_set_motor_zero_flag)(spi_handler_block_t *this) = 0;
static void (*spi_handler_reset_motor_zero_flag)(spi_handler_block_t *this) = 0;

// static void (*spi_handler_null)(spi_handler_block_t *this) = 0;

typedef struct spi_handler_function {
    void **entry;
    char *symbol;
} spi_handler_function_t;

static spi_handler_function_t functions[] = {
    {.entry = (void **)&spi_handler_constructor, .symbol = "_ZN10SpiHandlerC1Ev"},
    {.entry = (void **)&spi_handler_destructor, .symbol = "_ZN10SpiHandlerD1Ev"},
    {.entry = (void **)&spi_handler_driver_run, .symbol = "_ZN10SpiHandler9driverRunEv"},
    {.entry = (void **)&spi_handler_flush_spi_buff, .symbol = "_ZN10SpiHandler12flushSpiBuffEv"},
    {.entry = (void **)&spi_handler_get_spi_data, .symbol = "_ZN10SpiHandler10getSpiDataER7SpiData"},
    {.entry = (void **)&spi_handler_init_spi, .symbol = "_ZN10SpiHandler7initSpiEv"},
    {.entry = (void **)&spi_handler_publish_spi, .symbol = "_ZN10SpiHandler10publishSpiERN3lcm3LCME"},
    {.entry = (void **)&spi_handler_reset_spi, .symbol = "_ZN10SpiHandler8resetSpiEv"},
    {.entry = (void **)&spi_handler_set_spi_command, .symbol = "_ZN10SpiHandler13setSpiCommandERK10SpiCommand"},
    {.entry = (void **)&spi_handler_set_zero, .symbol = "_ZN10SpiHandler7setZeroEv"},
    {.entry = (void **)&spi_handler_spi_to_spine, .symbol = "_ZN10SpiHandler9spi2spineER13spi_command_tR11spine_cmd_ti"},
    {.entry = (void **)&spi_handler_spine_to_spi, .symbol = "_ZN10SpiHandler9spine2spiER10spi_data_tR12spine_data_ti"},
    {.entry = (void **)&spi_handler_resolve_error_flags, .symbol = "_ZN10SpiHandler17resolveErrorFlagsEiiRNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE"},
    {.entry = (void **)&spi_handler_resolve_warn_flags, .symbol = "_ZN10SpiHandler16resolveWarnFlagsEiiRNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE"},
    {.entry = (void **)&spi_handler_spine_cmd_to_spi_cmd, .symbol = "_ZN10SpiHandler15spineCmd2spiCmdEPK11spine_cmd_tP13spi_command_ti"},
    {.entry = (void **)&spi_handler_spi_data_to_spine_data, .symbol = "_ZN10SpiHandler17spiData2spineDataEPK10spi_data_tP12spine_data_ti"},
    {.entry = (void **)&spi_handler_set_motor_zero_flag, .symbol = "_ZN10SpiHandler16setMotorZeroFlagEv"},
    {.entry = (void **)&spi_handler_reset_motor_zero_flag, .symbol = "_ZN10SpiHandler18resetMotorZeroFlagEv"},
};

static char *library_paths[] = {
    "/mnt/UDISK/robot-software/build",
    "/robot/robot-software/build",
    NULL
};

static char *librobot_libs[] = {
    "librobot.so",
    "libbiomimetics.so",
    "libparam_handler.so",
    "libdog_toolkit.so",
    "libJCQP.so",
    "libosqp.so",
    "liblibvnc.so"
};

int locomotion_init()
{
    char cwd[PATH_MAX];
    char librobot_path[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("[RT SPI] CWD: %s\n", cwd);
    } else {
        perror("getcwd(): error");
    }

    void *librobot = NULL;
    char librobot_so[] = "librobot.so";

#if 1
    for (int i = 0; i < sizeof(library_paths) / sizeof(library_paths[0]); i++) {
        char *library_path = library_paths[i];
        library_path = library_path ? library_path : cwd;

        snprintf(librobot_path, PATH_MAX, "%s/%s", library_path, librobot_so);

        if (access(librobot_path, F_OK) != 0) {
            continue;
        }

        for (int j = sizeof(librobot_libs) / sizeof(librobot_libs[0]); j > 0; j--) {

            snprintf(librobot_path, PATH_MAX, "%s/%s", library_path, librobot_libs[j - 1]);

            librobot = dlopen(librobot_path, RTLD_LAZY | RTLD_GLOBAL);
            if (librobot == NULL) {
                printf("dlopen: %s\n", dlerror());
                return -1;
            }
        }

        if (strcmp(library_path, cwd) != 0) {
            printf("[RT SPI] Change CWD: %s\n", library_path);
            chdir(library_path);
        }
    }
#else
    librobot = dlopen(librobot_so, RTLD_LAZY);
    if (librobot == 0) {
        printf("dlopen： %s\n", dlerror());
        return -1;
    }
#endif

    if (librobot == NULL) {
        return -1;
    }

    for (int i = 0; i < (sizeof(functions) / sizeof(spi_handler_function_t)); i++) {
        void *p = dlsym(librobot, functions[i].symbol);
        if (p != NULL) {
            *functions[i].entry = p;
        } else {
            printf("dlsym: %s\n", functions[i].symbol);
            return -1;
        }
    }

    memset(spi_handler, 0, sizeof(spi_handler));

    spi_handler_constructor(spi_handler);
    spi_handler_init_spi(spi_handler);

    // printf("%lu %lu %lu\n", sizeof(spi_handler_block_t), sizeof(spi_command_t), sizeof(spi_data_t));
    // printf("%p %p %p\n", spi_handler, &spi_handler->spi_command, &spi_handler->spi_data);
    // printf("%08x %08x\n", spi_handler->spi0_fd, spi_handler->spi1_fd);

    spi_handler_flush_spi_buff(spi_handler);

    return 0;
}

int locomotion_flush(spi_command_t *spi_command, spi_data_t *spi_data)
{
    // check motor error flag, or resetSpi

    spi_handler_set_spi_command(spi_handler, spi_command);
    spi_handler_driver_run(spi_handler);
    spi_handler_get_spi_data(spi_handler, spi_data);

    // SpiHandler::publishSpi(&block, LCM *lcm);
}

int locomotion_debug_status()
{
    printf("%i: [%i %i %i %i]\n", spi_handler->driver_run_count,
            spi_handler->status[0], spi_handler->status[1],
            spi_handler->status[2], spi_handler->status[3]);
}

