/* $Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.15/fs/jffs2/comprtest.c#1 $ */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <asm/types.h>
#if 0
#define TESTDATA_LEN 512
static unsigned char testdata[TESTDATA_LEN] = {
 0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x60, 0x83, 0x04, 0x08, 0x34, 0x00, 0x00, 0x00,
 0xb0, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x20, 0x00, 0x06, 0x00, 0x28, 0x00,
 0x1e, 0x00, 0x1b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x34, 0x80, 0x04, 0x08,
 0x34, 0x80, 0x04, 0x08, 0xc0, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf4, 0x00, 0x00, 0x00, 0xf4, 0x80, 0x04, 0x08,
 0xf4, 0x80, 0x04, 0x08, 0x13, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x04, 0x08,
 0x00, 0x80, 0x04, 0x08, 0x0d, 0x05, 0x00, 0x00, 0x0d, 0x05, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
 0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x05, 0x00, 0x00, 0x10, 0x95, 0x04, 0x08,
 0x10, 0x95, 0x04, 0x08, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
 0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x58, 0x05, 0x00, 0x00, 0x58, 0x95, 0x04, 0x08,
 0x58, 0x95, 0x04, 0x08, 0xa0, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x08, 0x81, 0x04, 0x08,
 0x08, 0x81, 0x04, 0x08, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
 0x04, 0x00, 0x00, 0x00, 0x2f, 0x6c, 0x69, 0x62, 0x2f, 0x6c, 0x64, 0x2d, 0x6c, 0x69, 0x6e, 0x75,
 0x78, 0x2e, 0x73, 0x6f, 0x2e, 0x32, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
 0x01, 0x00, 0x00, 0x00, 0x47, 0x4e, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00,
 0x0c, 0x83, 0x04, 0x08, 0x81, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
 0x1c, 0x83, 0x04, 0x08, 0xac, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x57, 0x00, 0x00, 0x00,
 0x2c, 0x83, 0x04, 0x08, 0xdd, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
 0x3c, 0x83, 0x04, 0x08, 0x2e, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
 0x4c, 0x83, 0x04, 0x08, 0x7d, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00,
 0x00, 0x85, 0x04, 0x08, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x67,
 0x6d, 0x6f, 0x6e, 0x5f, 0x73, 0x74, 0x61, 0x72, 0x74, 0x5f, 0x5f, 0x00, 0x6c, 0x69, 0x62, 0x63,
 0x2e, 0x73, 0x6f, 0x2e, 0x36, 0x00, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x00, 0x5f, 0x5f, 0x63};
#else
#define TESTDATA_LEN 3481
static unsigned char testdata[TESTDATA_LEN] = {
 0x23, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x20, 0x22, 0x64, 0x62, 0x65, 0x6e, 0x63, 0x68,
 0x2e, 0x68, 0x22, 0x0a, 0x0a, 0x23, 0x64, 0x65, 0x66, 0x69, 0x6e, 0x65, 0x20, 0x4d, 0x41, 0x58,
 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x20, 0x31, 0x30, 0x30, 0x30, 0x0a, 0x0a, 0x73, 0x74, 0x61,
 0x74, 0x69, 0x63, 0x20, 0x63, 0x68, 0x61, 0x72, 0x20, 0x62, 0x75, 0x66, 0x5b, 0x37, 0x30, 0x30,
 0x30, 0x30, 0x5d, 0x3b, 0x0a, 0x65, 0x78, 0x74, 0x65, 0x72, 0x6e, 0x20, 0x69, 0x6e, 0x74, 0x20,
 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x3b, 0x0a, 0x0a, 0x73, 0x74, 0x61,
 0x74, 0x69, 0x63, 0x20, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x20, 0x7b, 0x0a, 0x09, 0x69, 0x6e,
 0x74, 0x20, 0x66, 0x64, 0x3b, 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c,
 0x65, 0x3b, 0x0a, 0x7d, 0x20, 0x66, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x4d, 0x41, 0x58, 0x5f,
 0x46, 0x49, 0x4c, 0x45, 0x53, 0x5d, 0x3b, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f,
 0x5f, 0x75, 0x6e, 0x6c, 0x69, 0x6e, 0x6b, 0x28, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e,
 0x61, 0x6d, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72,
 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x75,
 0x6e, 0x6c, 0x69, 0x6e, 0x6b, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x20, 0x21, 0x3d, 0x20,
 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28,
 0x25, 0x64, 0x29, 0x20, 0x75, 0x6e, 0x6c, 0x69, 0x6e, 0x6b, 0x20, 0x25, 0x73, 0x20, 0x66, 0x61,
 0x69, 0x6c, 0x65, 0x64, 0x20, 0x28, 0x25, 0x73, 0x29, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09,
 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75,
 0x6e, 0x74, 0x2c, 0x20, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x74, 0x72, 0x65, 0x72,
 0x72, 0x6f, 0x72, 0x28, 0x65, 0x72, 0x72, 0x6e, 0x6f, 0x29, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a,
 0x7d, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x65, 0x78, 0x70, 0x61, 0x6e, 0x64, 0x5f, 0x66,
 0x69, 0x6c, 0x65, 0x28, 0x69, 0x6e, 0x74, 0x20, 0x66, 0x64, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20,
 0x73, 0x69, 0x7a, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x73, 0x3b, 0x0a,
 0x09, 0x77, 0x68, 0x69, 0x6c, 0x65, 0x20, 0x28, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x20, 0x7b, 0x0a,
 0x09, 0x09, 0x73, 0x20, 0x3d, 0x20, 0x4d, 0x49, 0x4e, 0x28, 0x73, 0x69, 0x7a, 0x65, 0x6f, 0x66,
 0x28, 0x62, 0x75, 0x66, 0x29, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x09,
 0x77, 0x72, 0x69, 0x74, 0x65, 0x28, 0x66, 0x64, 0x2c, 0x20, 0x62, 0x75, 0x66, 0x2c, 0x20, 0x73,
 0x29, 0x3b, 0x0a, 0x09, 0x09, 0x73, 0x69, 0x7a, 0x65, 0x20, 0x2d, 0x3d, 0x20, 0x73, 0x3b, 0x0a,
 0x09, 0x7d, 0x0a, 0x7d, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x6f, 0x70,
 0x65, 0x6e, 0x28, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20,
 0x69, 0x6e, 0x74, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20,
 0x73, 0x69, 0x7a, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x66, 0x64, 0x2c,
 0x20, 0x69, 0x3b, 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x66, 0x6c, 0x61, 0x67, 0x73, 0x20, 0x3d,
 0x20, 0x4f, 0x5f, 0x52, 0x44, 0x57, 0x52, 0x7c, 0x4f, 0x5f, 0x43, 0x52, 0x45, 0x41, 0x54, 0x3b,
 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x20, 0x73, 0x74, 0x61, 0x74, 0x20, 0x73, 0x74,
 0x3b, 0x0a, 0x09, 0x73, 0x74, 0x61, 0x74, 0x69, 0x63, 0x20, 0x69, 0x6e, 0x74, 0x20, 0x63, 0x6f,
 0x75, 0x6e, 0x74, 0x3b, 0x0a, 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72, 0x28,
 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x73, 0x69,
 0x7a, 0x65, 0x20, 0x3d, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x66, 0x6c, 0x61, 0x67, 0x73, 0x20, 0x7c,
 0x3d, 0x20, 0x4f, 0x5f, 0x54, 0x52, 0x55, 0x4e, 0x43, 0x3b, 0x0a, 0x0a, 0x09, 0x66, 0x64, 0x20,
 0x3d, 0x20, 0x6f, 0x70, 0x65, 0x6e, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x66, 0x6c,
 0x61, 0x67, 0x73, 0x2c, 0x20, 0x30, 0x36, 0x30, 0x30, 0x29, 0x3b, 0x0a, 0x09, 0x69, 0x66, 0x20,
 0x28, 0x66, 0x64, 0x20, 0x3d, 0x3d, 0x20, 0x2d, 0x31, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70,
 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28, 0x25, 0x64, 0x29, 0x20, 0x6f, 0x70, 0x65, 0x6e,
 0x20, 0x25, 0x73, 0x20, 0x66, 0x61, 0x69, 0x6c, 0x65, 0x64, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x68,
 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x25, 0x64, 0x20, 0x28, 0x25, 0x73, 0x29, 0x5c, 0x6e, 0x22,
 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e, 0x65,
 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x68,
 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c, 0x20, 0x73, 0x74, 0x72, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x28,
 0x65, 0x72, 0x72, 0x6e, 0x6f, 0x29, 0x29, 0x3b, 0x0a, 0x09, 0x09, 0x72, 0x65, 0x74, 0x75, 0x72,
 0x6e, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x66, 0x73, 0x74, 0x61, 0x74, 0x28, 0x66, 0x64, 0x2c,
 0x20, 0x26, 0x73, 0x74, 0x29, 0x3b, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x73, 0x69, 0x7a, 0x65,
 0x20, 0x3e, 0x20, 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x20, 0x7b,
 0x0a, 0x23, 0x69, 0x66, 0x20, 0x44, 0x45, 0x42, 0x55, 0x47, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69,
 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28, 0x25, 0x64, 0x29, 0x20, 0x65, 0x78, 0x70, 0x61, 0x6e, 0x64,
 0x69, 0x6e, 0x67, 0x20, 0x25, 0x73, 0x20, 0x74, 0x6f, 0x20, 0x25, 0x64, 0x20, 0x66, 0x72, 0x6f,
 0x6d, 0x20, 0x25, 0x64, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20,
 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20, 0x66,
 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x2c, 0x20, 0x28, 0x69, 0x6e, 0x74,
 0x29, 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x23, 0x65,
 0x6e, 0x64, 0x69, 0x66, 0x0a, 0x09, 0x09, 0x65, 0x78, 0x70, 0x61, 0x6e, 0x64, 0x5f, 0x66, 0x69,
 0x6c, 0x65, 0x28, 0x66, 0x64, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x20, 0x2d, 0x20, 0x73, 0x74,
 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x20, 0x65, 0x6c,
 0x73, 0x65, 0x20, 0x69, 0x66, 0x20, 0x28, 0x73, 0x69, 0x7a, 0x65, 0x20, 0x3c, 0x20, 0x73, 0x74,
 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72,
 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x74, 0x72, 0x75, 0x6e, 0x63, 0x61, 0x74, 0x69, 0x6e, 0x67,
 0x20, 0x25, 0x73, 0x20, 0x74, 0x6f, 0x20, 0x25, 0x64, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20, 0x25,
 0x64, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x2c, 0x20, 0x28, 0x69, 0x6e,
 0x74, 0x29, 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09,
 0x09, 0x66, 0x74, 0x72, 0x75, 0x6e, 0x63, 0x61, 0x74, 0x65, 0x28, 0x66, 0x64, 0x2c, 0x20, 0x73,
 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x66, 0x6f, 0x72, 0x20, 0x28, 0x69,
 0x3d, 0x30, 0x3b, 0x69, 0x3c, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x3b, 0x69,
 0x2b, 0x2b, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x69, 0x66, 0x20, 0x28, 0x66, 0x74, 0x61, 0x62,
 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x3d, 0x3d, 0x20,
 0x30, 0x29, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6b, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x69, 0x66,
 0x20, 0x28, 0x69, 0x20, 0x3d, 0x3d, 0x20, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53,
 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x66, 0x69,
 0x6c, 0x65, 0x20, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x66, 0x75, 0x6c, 0x6c, 0x20, 0x66, 0x6f,
 0x72, 0x20, 0x25, 0x73, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x3b,
 0x0a, 0x09, 0x09, 0x65, 0x78, 0x69, 0x74, 0x28, 0x31, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09,
 0x66, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65,
 0x20, 0x3d, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x3b, 0x0a, 0x09, 0x66, 0x74, 0x61, 0x62,
 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x66, 0x64, 0x20, 0x3d, 0x20, 0x66, 0x64, 0x3b, 0x0a, 0x09,
 0x69, 0x66, 0x20, 0x28, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2b, 0x2b, 0x20, 0x25, 0x20, 0x31, 0x30,
 0x30, 0x20, 0x3d, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e,
 0x74, 0x66, 0x28, 0x22, 0x2e, 0x22, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x7d, 0x0a, 0x0a, 0x76,
 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x77, 0x72, 0x69, 0x74, 0x65, 0x28, 0x69, 0x6e, 0x74,
 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x7a,
 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x29, 0x0a, 0x7b,
 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x69, 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x62,
 0x75, 0x66, 0x5b, 0x30, 0x5d, 0x20, 0x3d, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x6d, 0x65, 0x6d, 0x73,
 0x65, 0x74, 0x28, 0x62, 0x75, 0x66, 0x2c, 0x20, 0x31, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x6f,
 0x66, 0x28, 0x62, 0x75, 0x66, 0x29, 0x29, 0x3b, 0x0a, 0x0a, 0x09, 0x66, 0x6f, 0x72, 0x20, 0x28,
 0x69, 0x3d, 0x30, 0x3b, 0x69, 0x3c, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x3b,
 0x69, 0x2b, 0x2b, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x69, 0x66, 0x20, 0x28, 0x66, 0x74, 0x61,
 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x3d, 0x3d,
 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x29, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6b, 0x3b, 0x0a,
 0x09, 0x7d, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x69, 0x20, 0x3d, 0x3d, 0x20, 0x4d, 0x41, 0x58,
 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x29, 0x20, 0x7b, 0x0a, 0x23, 0x69, 0x66, 0x20, 0x31, 0x0a,
 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28, 0x25, 0x64, 0x29, 0x20, 0x64,
 0x6f, 0x5f, 0x77, 0x72, 0x69, 0x74, 0x65, 0x3a, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20,
 0x25, 0x64, 0x20, 0x77, 0x61, 0x73, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x6f, 0x70, 0x65, 0x6e, 0x20,
 0x73, 0x69, 0x7a, 0x65, 0x3d, 0x25, 0x64, 0x20, 0x6f, 0x66, 0x73, 0x3d, 0x25, 0x64, 0x5c, 0x6e,
 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e,
 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c,
 0x20, 0x73, 0x69, 0x7a, 0x65, 0x2c, 0x20, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x29, 0x3b, 0x0a,
 0x23, 0x65, 0x6e, 0x64, 0x69, 0x66, 0x0a, 0x09, 0x09, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x3b,
 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x6c, 0x73, 0x65, 0x65, 0x6b, 0x28, 0x66, 0x74, 0x61, 0x62, 0x6c,
 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x66, 0x64, 0x2c, 0x20, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x2c,
 0x20, 0x53, 0x45, 0x45, 0x4b, 0x5f, 0x53, 0x45, 0x54, 0x29, 0x3b, 0x0a, 0x09, 0x69, 0x66, 0x20,
 0x28, 0x77, 0x72, 0x69, 0x74, 0x65, 0x28, 0x66, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d,
 0x2e, 0x66, 0x64, 0x2c, 0x20, 0x62, 0x75, 0x66, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x20,
 0x21, 0x3d, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69,
 0x6e, 0x74, 0x66, 0x28, 0x22, 0x77, 0x72, 0x69, 0x74, 0x65, 0x20, 0x66, 0x61, 0x69, 0x6c, 0x65,
 0x64, 0x20, 0x6f, 0x6e, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x25, 0x64, 0x5c, 0x6e,
 0x22, 0x2c, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x7d,
 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x72, 0x65, 0x61, 0x64, 0x28, 0x69,
 0x6e, 0x74, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20, 0x73,
 0x69, 0x7a, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74, 0x20, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x29,
 0x0a, 0x7b, 0x0a, 0x09, 0x69, 0x6e, 0x74, 0x20, 0x69, 0x3b, 0x0a, 0x09, 0x66, 0x6f, 0x72, 0x20,
 0x28, 0x69, 0x3d, 0x30, 0x3b, 0x69, 0x3c, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53,
 0x3b, 0x69, 0x2b, 0x2b, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x69, 0x66, 0x20, 0x28, 0x66, 0x74,
 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x3d,
 0x3d, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x29, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6b, 0x3b,
 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x69, 0x20, 0x3d, 0x3d, 0x20, 0x4d, 0x41,
 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69,
 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28, 0x25, 0x64, 0x29, 0x20, 0x64, 0x6f, 0x5f, 0x72, 0x65, 0x61,
 0x64, 0x3a, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x25, 0x64, 0x20, 0x77, 0x61, 0x73,
 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x6f, 0x70, 0x65, 0x6e, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x3d, 0x25,
 0x64, 0x20, 0x6f, 0x66, 0x73, 0x3d, 0x25, 0x64, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09,
 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e,
 0x74, 0x2c, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x2c,
 0x20, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x29, 0x3b, 0x0a, 0x09, 0x09, 0x72, 0x65, 0x74, 0x75,
 0x72, 0x6e, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x6c, 0x73, 0x65, 0x65, 0x6b, 0x28, 0x66, 0x74,
 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x66, 0x64, 0x2c, 0x20, 0x6f, 0x66, 0x66, 0x73,
 0x65, 0x74, 0x2c, 0x20, 0x53, 0x45, 0x45, 0x4b, 0x5f, 0x53, 0x45, 0x54, 0x29, 0x3b, 0x0a, 0x09,
 0x72, 0x65, 0x61, 0x64, 0x28, 0x66, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x66,
 0x64, 0x2c, 0x20, 0x62, 0x75, 0x66, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x7d,
 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0x28,
 0x69, 0x6e, 0x74, 0x20, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x69,
 0x6e, 0x74, 0x20, 0x69, 0x3b, 0x0a, 0x09, 0x66, 0x6f, 0x72, 0x20, 0x28, 0x69, 0x3d, 0x30, 0x3b,
 0x69, 0x3c, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c, 0x45, 0x53, 0x3b, 0x69, 0x2b, 0x2b, 0x29,
 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x69, 0x66, 0x20, 0x28, 0x66, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x5b,
 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x3d, 0x3d, 0x20, 0x68, 0x61, 0x6e,
 0x64, 0x6c, 0x65, 0x29, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6b, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09,
 0x69, 0x66, 0x20, 0x28, 0x69, 0x20, 0x3d, 0x3d, 0x20, 0x4d, 0x41, 0x58, 0x5f, 0x46, 0x49, 0x4c,
 0x45, 0x53, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22,
 0x28, 0x25, 0x64, 0x29, 0x20, 0x64, 0x6f, 0x5f, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0x3a, 0x20, 0x68,
 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x25, 0x64, 0x20, 0x77, 0x61, 0x73, 0x20, 0x6e, 0x6f, 0x74,
 0x20, 0x6f, 0x70, 0x65, 0x6e, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20,
 0x20, 0x20, 0x20, 0x20, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20,
 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x09, 0x72, 0x65, 0x74, 0x75, 0x72,
 0x6e, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0x28, 0x66, 0x74, 0x61,
 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x66, 0x64, 0x29, 0x3b, 0x0a, 0x09, 0x66, 0x74, 0x61,
 0x62, 0x6c, 0x65, 0x5b, 0x69, 0x5d, 0x2e, 0x68, 0x61, 0x6e, 0x64, 0x6c, 0x65, 0x20, 0x3d, 0x20,
 0x30, 0x3b, 0x0a, 0x7d, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x6d, 0x6b,
 0x64, 0x69, 0x72, 0x28, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29,
 0x0a, 0x7b, 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72, 0x28, 0x66, 0x6e, 0x61,
 0x6d, 0x65, 0x29, 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x6d, 0x6b, 0x64, 0x69, 0x72,
 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x30, 0x37, 0x30, 0x30, 0x29, 0x20, 0x21, 0x3d,
 0x20, 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x23, 0x69, 0x66, 0x20, 0x44, 0x45, 0x42, 0x55, 0x47, 0x0a,
 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x6d, 0x6b, 0x64, 0x69, 0x72, 0x20,
 0x25, 0x73, 0x20, 0x66, 0x61, 0x69, 0x6c, 0x65, 0x64, 0x20, 0x28, 0x25, 0x73, 0x29, 0x5c, 0x6e,
 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x66, 0x6e, 0x61,
 0x6d, 0x65, 0x2c, 0x20, 0x73, 0x74, 0x72, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x28, 0x65, 0x72, 0x72,
 0x6e, 0x6f, 0x29, 0x29, 0x3b, 0x0a, 0x23, 0x65, 0x6e, 0x64, 0x69, 0x66, 0x0a, 0x09, 0x7d, 0x0a,
 0x7d, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x72, 0x6d, 0x64, 0x69, 0x72,
 0x28, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x0a, 0x7b, 0x0a,
 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29,
 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x72, 0x6d, 0x64, 0x69, 0x72, 0x28, 0x66, 0x6e,
 0x61, 0x6d, 0x65, 0x29, 0x20, 0x21, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70,
 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x72, 0x6d, 0x64, 0x69, 0x72, 0x20, 0x25, 0x73, 0x20,
 0x66, 0x61, 0x69, 0x6c, 0x65, 0x64, 0x20, 0x28, 0x25, 0x73, 0x29, 0x5c, 0x6e, 0x22, 0x2c, 0x20,
 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c,
 0x20, 0x73, 0x74, 0x72, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x28, 0x65, 0x72, 0x72, 0x6e, 0x6f, 0x29,
 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x7d, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f,
 0x5f, 0x72, 0x65, 0x6e, 0x61, 0x6d, 0x65, 0x28, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x6f, 0x6c,
 0x64, 0x2c, 0x20, 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x6e, 0x65, 0x77, 0x29, 0x0a, 0x7b, 0x0a,
 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72, 0x28, 0x6f, 0x6c, 0x64, 0x29, 0x3b, 0x0a,
 0x09, 0x73, 0x74, 0x72, 0x75, 0x70, 0x70, 0x65, 0x72, 0x28, 0x6e, 0x65, 0x77, 0x29, 0x3b, 0x0a,
 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x72, 0x65, 0x6e, 0x61, 0x6d, 0x65, 0x28, 0x6f, 0x6c, 0x64,
 0x2c, 0x20, 0x6e, 0x65, 0x77, 0x29, 0x20, 0x21, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x09,
 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x72, 0x65, 0x6e, 0x61, 0x6d, 0x65, 0x20,
 0x25, 0x73, 0x20, 0x25, 0x73, 0x20, 0x66, 0x61, 0x69, 0x6c, 0x65, 0x64, 0x20, 0x28, 0x25, 0x73,
 0x29, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
 0x6f, 0x6c, 0x64, 0x2c, 0x20, 0x6e, 0x65, 0x77, 0x2c, 0x20, 0x73, 0x74, 0x72, 0x65, 0x72, 0x72,
 0x6f, 0x72, 0x28, 0x65, 0x72, 0x72, 0x6e, 0x6f, 0x29, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x7d,
 0x0a, 0x0a, 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x28,
 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74,
 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74,
 0x20, 0x73, 0x74, 0x61, 0x74, 0x20, 0x73, 0x74, 0x3b, 0x0a, 0x0a, 0x09, 0x73, 0x74, 0x72, 0x75,
 0x70, 0x70, 0x65, 0x72, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x29, 0x3b, 0x0a, 0x0a, 0x09, 0x69,
 0x66, 0x20, 0x28, 0x73, 0x74, 0x61, 0x74, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x26,
 0x73, 0x74, 0x29, 0x20, 0x21, 0x3d, 0x20, 0x30, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72,
 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22, 0x28, 0x25, 0x64, 0x29, 0x20, 0x64, 0x6f, 0x5f, 0x73, 0x74,
 0x61, 0x74, 0x3a, 0x20, 0x25, 0x73, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x3d, 0x25, 0x64, 0x20, 0x25,
 0x73, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20, 0x66, 0x6e, 0x61, 0x6d,
 0x65, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x2c, 0x20, 0x73, 0x74, 0x72, 0x65, 0x72, 0x72, 0x6f,
 0x72, 0x28, 0x65, 0x72, 0x72, 0x6e, 0x6f, 0x29, 0x29, 0x3b, 0x0a, 0x09, 0x09, 0x72, 0x65, 0x74,
 0x75, 0x72, 0x6e, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28, 0x53, 0x5f, 0x49,
 0x53, 0x44, 0x49, 0x52, 0x28, 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x6d, 0x6f, 0x64, 0x65, 0x29,
 0x29, 0x20, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x3b, 0x0a, 0x0a, 0x09, 0x69, 0x66, 0x20, 0x28,
 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x20, 0x21, 0x3d, 0x20, 0x73, 0x69,
 0x7a, 0x65, 0x29, 0x20, 0x7b, 0x0a, 0x09, 0x09, 0x70, 0x72, 0x69, 0x6e, 0x74, 0x66, 0x28, 0x22,
 0x28, 0x25, 0x64, 0x29, 0x20, 0x64, 0x6f, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x3a, 0x20, 0x25, 0x73,
 0x20, 0x77, 0x72, 0x6f, 0x6e, 0x67, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x20, 0x25, 0x64, 0x20, 0x25,
 0x64, 0x5c, 0x6e, 0x22, 0x2c, 0x20, 0x0a, 0x09, 0x09, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x2c, 0x20, 0x66, 0x6e, 0x61, 0x6d,
 0x65, 0x2c, 0x20, 0x28, 0x69, 0x6e, 0x74, 0x29, 0x73, 0x74, 0x2e, 0x73, 0x74, 0x5f, 0x73, 0x69,
 0x7a, 0x65, 0x2c, 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x7d, 0x0a, 0x7d, 0x0a,
 0x0a, 0x76, 0x6f, 0x69, 0x64, 0x20, 0x64, 0x6f, 0x5f, 0x63, 0x72, 0x65, 0x61, 0x74, 0x65, 0x28,
 0x63, 0x68, 0x61, 0x72, 0x20, 0x2a, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x69, 0x6e, 0x74,
 0x20, 0x73, 0x69, 0x7a, 0x65, 0x29, 0x0a, 0x7b, 0x0a, 0x09, 0x64, 0x6f, 0x5f, 0x6f, 0x70, 0x65,
 0x6e, 0x28, 0x66, 0x6e, 0x61, 0x6d, 0x65, 0x2c, 0x20, 0x35, 0x30, 0x30, 0x30, 0x2c, 0x20, 0x73,
 0x69, 0x7a, 0x65, 0x29, 0x3b, 0x0a, 0x09, 0x64, 0x6f, 0x5f, 0x63, 0x6c, 0x6f, 0x73, 0x65, 0x28,
 0x35, 0x30, 0x30, 0x30, 0x29, 0x3b, 0x0a, 0x7d, 0x0a
};
#endif
static unsigned char comprbuf[TESTDATA_LEN];
static unsigned char decomprbuf[TESTDATA_LEN];

int jffs2_decompress(unsigned char comprtype, unsigned char *cdata_in,
		     unsigned char *data_out, uint32_t cdatalen, uint32_t datalen);
unsigned char jffs2_compress(unsigned char *data_in, unsigned char *cpage_out,
			     uint32_t *datalen, uint32_t *cdatalen);

int init_module(void ) {
	unsigned char comprtype;
	uint32_t c, d;
	int ret;

	printk("Original data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       testdata[0],testdata[1],testdata[2],testdata[3],
	       testdata[4],testdata[5],testdata[6],testdata[7],
	       testdata[8],testdata[9],testdata[10],testdata[11],
	       testdata[12],testdata[13],testdata[14],testdata[15]);
	d = TESTDATA_LEN;
	c = TESTDATA_LEN;
	comprtype = jffs2_compress(testdata, comprbuf, &d, &c);

	printk("jffs2_compress used compression type %d. Compressed size %d, uncompressed size %d\n",
	       comprtype, c, d);
	printk("Compressed data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       comprbuf[0],comprbuf[1],comprbuf[2],comprbuf[3],
	       comprbuf[4],comprbuf[5],comprbuf[6],comprbuf[7],
	       comprbuf[8],comprbuf[9],comprbuf[10],comprbuf[11],
	       comprbuf[12],comprbuf[13],comprbuf[14],comprbuf[15]);

	ret = jffs2_decompress(comprtype, comprbuf, decomprbuf, c, d);
	printk("jffs2_decompress returned %d\n", ret);
	printk("Decompressed data:  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	       decomprbuf[0],decomprbuf[1],decomprbuf[2],decomprbuf[3],
	       decomprbuf[4],decomprbuf[5],decomprbuf[6],decomprbuf[7],
	       decomprbuf[8],decomprbuf[9],decomprbuf[10],decomprbuf[11],
	       decomprbuf[12],decomprbuf[13],decomprbuf[14],decomprbuf[15]);
	if (memcmp(decomprbuf, testdata, d))
		printk("Compression and decompression corrupted data\n");
	else
		printk("Compression good for %d bytes\n", d);
	return 1;
}
