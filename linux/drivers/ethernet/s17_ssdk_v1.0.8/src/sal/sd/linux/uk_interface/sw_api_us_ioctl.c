#include <sys/ioctl.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fcntl.h>
#include "sw.h"
#include "sw_api.h"
#include "sw_api_us.h"

#define MISC_CHR_DEV       10
static int glb_socket_fd = 0;

sw_error_t
sw_uk_if(a_uint32_t arg_val[SW_MAX_API_PARAM])
{
    ioctl(glb_socket_fd, SIOCDEVPRIVATE, arg_val);
    return SW_OK;
}

sw_error_t
sw_uk_init(a_uint32_t nl_prot)
{
    if (!glb_socket_fd) {
        /* even mknod fail we not quit, perhaps the device node exist already */
        #if defined UK_MINOR_DEV
        mknod("/dev/switch_ssdk", S_IFCHR, makedev(MISC_CHR_DEV, UK_MINOR_DEV));
        #else
        mknod("/dev/switch_ssdk", S_IFCHR, makedev(MISC_CHR_DEV, nl_prot));
        #endif
        if ((glb_socket_fd = open("/dev/switch_ssdk", O_RDWR)) < 0) {
            return SW_INIT_ERROR;
        }
    }

    return SW_OK;
}

sw_error_t
sw_uk_cleanup(void)
{
    close(glb_socket_fd);
    glb_socket_fd = 0;
    #if 0
    remove("/dev/switch_ssdk");
    #endif
    return SW_OK;
}

