/*
 * ep2_isp_i2c.c
 *
 *  Created on: 2022�~10��4��
 *      Author: 901912
 */
#include "ep2_isp_i2c.h"

#include <Wire.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define PROGRESS_MAX (64)

bool probeDevice() {
    uint8_t error, address;

    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            static_assert(CTRL_SLVID == DATA_SLVID);
            if (address == CTRL_SLVID) {
                return true;
            }
        } else if (error == 4) {
            printf("Unknow error at address 0x%.2x\n", address);
        }
    }

    while (Wire.available()) {
        Wire.read();
    }

    return false;
}

void printProgress(double percentage) {
    printf("\rProgress: [");
    int pos = PROGRESS_MAX * percentage;
    for (int i = 0; i < PROGRESS_MAX; ++i) {
        if (i < pos) {
            printf("#");
        } else {
            printf(" ");
        }
    }
    printf("] - %.2f%%\n", percentage * 100);
}

typedef enum I2C_MasterFlag {
    NONE           = 0x80,
    START          = 0x02,
    Repeated_START = 0x03,  // Repeated_START will not send master code in HS mode
    STOP           = 0x04,
    START_AND_STOP = 0x06,  // START condition followed by SEND and STOP condition
} I2C_MasterFlag;

#define UINT8       uint8_t
#define UINT32      uint32_t
#define UINT16      uint16_t
#define FT_STATUS   unsigned long
#define FT4222_OK   0
#define dbg_printf  printf
#define ftdevHandle 0

FT_STATUS FT4222_I2CMaster_Write(
  int _, UINT8 deviceAddress, UINT8* buffer, UINT32 sizeToTransfer, UINT16* sizeTransferred) {
    uint16_t written = 0;

    int ret = 0;

    size_t check = sizeToTransfer - 1;
    Wire.beginTransmission(deviceAddress);
    for (size_t i = 0; i < sizeToTransfer; ++i) {
        written += Wire.write((const uint8_t*)buffer + i, size_t{1});
    }
    ret = Wire.endTransmission(true);

    *sizeTransferred += written;

    if (ret == 0) {
        return FT4222_OK;
    } else {
        return -1;
    }
}

void I2C_burst_write(UINT8 id, UINT8* addr, UINT32 addr_size, UINT8* data, UINT32 data_size) {
    UINT8*    packet;
    UINT32    count           = 0;
    UINT16    sizeTransferred = 0;
    FT_STATUS ftStatus;

    packet = new UINT8[addr_size + data_size];

    for (UINT32 i = 0; i < addr_size; i++) {
        packet[i] = addr[i];
        count += 1;
    }

    for (UINT32 j = 0; j < data_size; j++) {
        packet[j + count] = data[j];
        count += 1;
    }

    ftStatus = FT4222_I2CMaster_Write(ftdevHandle, id, packet, count, &sizeTransferred);

    if (FT4222_OK == ftStatus) {
        // dbg_printf("I2C_burst_write: write done\n");
    } else {
        dbg_printf("I2C_burst_write: write fail\n");
    }

    delete packet;

    return;
}

FT_STATUS FT4222_I2CMaster_WriteEx(
  int _, UINT8 deviceAddress, int flag, UINT8* buffer, UINT32 sizeToTransfer, UINT16* sizeTransferred) {
    uint16_t written = 0;

    int ret = 0;

    size_t check = sizeToTransfer - 1;

    if (flag & START || flag == START_AND_STOP) {
        Wire.beginTransmission(deviceAddress);
    }

    for (size_t i = 0; i < sizeToTransfer; ++i) {
        written += Wire.write((const uint8_t*)buffer + i, size_t{1});
    }

    ret = Wire.endTransmission((flag & STOP || flag == START_AND_STOP));

    *sizeTransferred += written;

    if (ret == 0) {
        return FT4222_OK;
    } else {
        return -1;
    }
}

FT_STATUS FT4222_I2CMaster_ReadEx(
  int _, UINT8 deviceAddress, int flag, UINT8* buffer, UINT32 sizeToTransfer, UINT16* sizeTransferred) {
    uint16_t read = 0, read_size = 0;

    size_t ret = 0;

    while (Wire.available()) {
        printf(".");
        Wire.read();
    }

    read_size = Wire.requestFrom(deviceAddress, static_cast<size_t>(sizeToTransfer), static_cast<bool>(flag & STOP));

    int b_8 = -1;
    while (1) {
        b_8 = Wire.read();
        if (b_8 == -1) {
            printf("Error reading, requested %d, available %d, read %d\n", sizeToTransfer, read_size, read);
            break;
        }
        buffer[read] = static_cast<UINT8>(b_8);
        read += 1;
        if (read >= sizeToTransfer) {
            break;
        }
    }

    *sizeTransferred += read;
    if (ret == 0) {
        return FT4222_OK;
    } else {
        return -1;
    }
}

void I2C_cmd_write_ex(UINT8 id, UINT8* buffer, UINT32 sizeToTransfer, int flag) {
    UINT16    sizeTransferred = 0;
    FT_STATUS ftStatus        = 0x78;

    ftStatus = FT4222_I2CMaster_WriteEx(ftdevHandle, id, flag, buffer, sizeToTransfer, &sizeTransferred);

    if (FT4222_OK == ftStatus) {
        // dbg_printf("I2C_cmd_write_ex: write done\n");
    } else {
        dbg_printf("I2C_cmd_write_ex: write fail\n");
    }
}

void i2c_single_write(unsigned char id, unsigned char* addr, unsigned char* data) {
    UINT16       sizeTransferred = 0;
    UINT8        ret_packet[2]   = {0x0f, 0xff};
    unsigned int test_time;

    UINT8 write_packet[16]    = {0};
    UINT8 write_ack_packet[2] = {0};

    {
        write_packet[0] = 0x00;
        write_packet[2] = 0x01;
        write_packet[4] = 0x02;
        write_packet[6] = 0x03;

        write_packet[8]  = 0x04;
        write_packet[10] = 0x05;
        write_packet[12] = 0x06;
        write_packet[14] = 0x07;

        write_ack_packet[0] = 0x0C;
        write_ack_packet[1] = 0x01;
    }

    // address size must be 4
    write_packet[1] = addr[0];
    write_packet[3] = addr[1];
    write_packet[5] = addr[2];
    write_packet[7] = addr[3];

    // data size must be 4
    write_packet[9]  = data[0];
    write_packet[11] = data[1];
    write_packet[13] = data[2];
    write_packet[15] = data[3];

    // address
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 2, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 4, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 6, 2, &sizeTransferred);

    // data
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 8, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 10, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 12, 2, &sizeTransferred);
    FT4222_I2CMaster_Write(ftdevHandle, id, write_packet + 14, 2, &sizeTransferred);

    FT4222_I2CMaster_Write(ftdevHandle, id, write_ack_packet, 2, &sizeTransferred);

    test_time = 0;
    while (1) {
        test_time += 1;
        FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, ret_packet, 1, &sizeTransferred);

        FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, ret_packet + 1, 1, &sizeTransferred);

        if (ret_packet[1] == 0x0) break;
        if (test_time >= 1000) break;
    }
}

unsigned int i2c_single_read(unsigned char id, unsigned char* addr) {
    UINT16       sizeTransferred = 0;
    UINT8        ret_packet[2]   = {0x0f, 0xff};
    unsigned int test_time;
    UINT8        no_timeout = 1;
    FT_STATUS    ftStatus   = 0;

    UINT8 read_packet[16], read_ack_packet[2];

    {
        read_packet[0] = 0x00;
        read_packet[2] = 0x01;
        read_packet[4] = 0x02;
        read_packet[6] = 0x03;

        read_packet[8]  = 0x08;
        read_packet[10] = 0x09;
        read_packet[12] = 0x0A;
        read_packet[14] = 0x0B;

        read_ack_packet[0] = 0x0C;
        read_ack_packet[1] = 0x00;
    }

    read_packet[9]  = 0xFF;
    read_packet[11] = 0xFF;
    read_packet[13] = 0xFF;
    read_packet[15] = 0xFF;

    // address size must be 4
    read_packet[1] = addr[0];
    read_packet[3] = addr[1];
    read_packet[5] = addr[2];
    read_packet[7] = addr[3];

#ifdef CHECK_AHBBUS_FIRST
    test_time = 0;
    while (1) {
        test_time++;
        ftStatus = FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, ret_packet, 1, &sizeTransferred);

        ftStatus = FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &ret_packet[1], 1, &sizeTransferred);

        if (ret_packet[1] == 0x0) {
            no_timeout = 1;
            break;
        }
        if (test_time >= 1000) {
            no_timeout = 0;
            break;
        }
    }

    if (no_timeout == 0) {
        return ((read_packet[15] << 24) | (read_packet[13] << 16) | (read_packet[11] << 8) | (read_packet[9] << 0));
    }
#endif

    //address
    ftStatus |= FT4222_I2CMaster_Write(ftdevHandle, id, &read_packet[0], 2, &sizeTransferred);
    ftStatus |= FT4222_I2CMaster_Write(ftdevHandle, id, &read_packet[2], 2, &sizeTransferred);
    ftStatus |= FT4222_I2CMaster_Write(ftdevHandle, id, &read_packet[4], 2, &sizeTransferred);
    ftStatus |= FT4222_I2CMaster_Write(ftdevHandle, id, &read_packet[6], 2, &sizeTransferred);

    read_ack_packet[0] = 0x0C;
    read_ack_packet[1] = 0x00;
    //
    ftStatus |= FT4222_I2CMaster_Write(ftdevHandle, id, read_ack_packet, 2, &sizeTransferred);

#ifndef CHECK_AHBBUS_FIRST
    test_time = 0;
    while (1) {
        test_time++;
        ftStatus |= FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, ret_packet, 1, &sizeTransferred);
        ftStatus |=
          FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &ret_packet[1], 1, &sizeTransferred);

        if (ret_packet[1] == 0x0) {
            no_timeout = 1;
            break;
        }
        if (test_time >= 1000) {
            no_timeout = 0;
            break;
        }
    }
#endif

    if (no_timeout == 1) {
        // FT4222_I2CMaster_Read(ftdevHandle, id, &read_packet[8], 2, &sizeTransferred);
        ftStatus |= FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, &read_packet[8], 1, &sizeTransferred);
        ftStatus |=
          FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &read_packet[9], 1, &sizeTransferred);

        // FT4222_I2CMaster_Read(ftdevHandle, id, &read_packet[10], 2, &sizeTransferred);
        ftStatus |= FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, &read_packet[10], 1, &sizeTransferred);
        ftStatus |=
          FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &read_packet[11], 1, &sizeTransferred);

        // FT4222_I2CMaster_Read(ftdevHandle, id, &read_packet[12], 2, &sizeTransferred);
        ftStatus |= FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, &read_packet[12], 1, &sizeTransferred);
        ftStatus |=
          FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &read_packet[13], 1, &sizeTransferred);

        // FT4222_I2CMaster_Read(ftdevHandle, id, &read_packet[14], 2, &sizeTransferred);
        ftStatus |= FT4222_I2CMaster_WriteEx(ftdevHandle, id, START, &read_packet[14], 1, &sizeTransferred);
        ftStatus |=
          FT4222_I2CMaster_ReadEx(ftdevHandle, id, Repeated_START | STOP, &read_packet[15], 1, &sizeTransferred);
    } else {
        read_packet[9]  = 0xFF;
        read_packet[11] = 0xFF;
        read_packet[13] = 0xFF;
        read_packet[15] = 0xFF;
    }

    // dbg_printf("i2c_single_read 0x%08X\n", (uint)ftStatus);

    return ((read_packet[15] << 24) | (read_packet[13] << 16) | (read_packet[11] << 8) | (read_packet[9] << 0));
}

int ep2_isp_i2c_proc(uint8_t* file_buf, uint32_t file_size) {
    uint32_t reg_addr[]   = {ISP_CONTROL_ADDR};
    uint32_t reg_addr_scu = ISP_ENABLE_REG_ADDR;

    uint32_t val_scu = REG_D8_TEST_MODE + REG_D8_ISP_EN;

    uint32_t val[1];
    val[0] = ISP_REG_DEFAULT_VAL;

    // S|50|W|ACK|D8|ACK|-|03|ACK|P
    I2C_burst_write(CTRL_SLVID, (uint8_t*)&reg_addr_scu, 1, (uint8_t*)&val_scu, 1);

    for (uint32_t i = 0; i < sizeof(reg_addr) / sizeof(uint32_t); ++i) {
        i2c_single_write(CTRL_SLVID, (uint8_t*)&reg_addr[i], (uint8_t*)&val[i]);
    }

    uint32_t addr = ISP_DATA_PORT_ADDR;
    uint8_t  buf[256 + 5];
    uint32_t size    = file_size;
    uint32_t upt_val = 0;
    printProgress((double)upt_val / file_size);

#if (EPII_VERC_PPDONE == 0x01)
    uint32_t pp_counter_val = 0;
#endif

    do {
        memset(buf, 0xFF, 261);
        buf[0] = 0xFA;
        buf[1] = addr & 0xFF;
        buf[2] = (addr >> 8) & 0xFF;
        buf[3] = (addr >> 16) & 0xFF;
        buf[4] = (addr >> 24) & 0xFF;

        if (size > 256) {
            memcpy(&buf[5], &file_buf[addr - ISP_DATA_PORT_ADDR], 256);
            upt_val += 256;
            size -= 256;
        } else {
            memcpy(&buf[5], &file_buf[addr - ISP_DATA_PORT_ADDR], size);
            upt_val += size;
            size = 0;
        }

        I2C_cmd_write_ex(DATA_SLVID, buf, 261, START_AND_STOP);

        addr += 256;

#if (EPII_VERC_PPDONE == 0x01)
        pp_counter_val += 1;
#endif

#if (EPII_VERC_PPDONE == 0x01)
        // check PP
        uint32_t pp_stat  = 0;
        uint32_t tmp_addr = ISP_CONTROL_ADDR + ISP_PPDONE_COUNTER_OFFSET;

        int timeout = 0;

        do {
            pp_stat = 0x0;

            pp_stat = i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);

            if (++timeout > 1000) {
                printf("ERROR: PP_STAT %d\n", pp_stat);
                return -1;
            }

        } while (!((pp_stat >> 28) == 1 || (pp_stat & 0xFFFFF) == pp_counter_val));

#else
        // check PP
        uint32_t pp_stat  = 0;
        uint32_t tmp_addr = ISP_CONTROL_ADDR + ISP_PPDONE_OFFSET;
        do {
            pp_stat = 0x0;
            pp_stat = i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);
    #if (EPII_VERB_PPDONE_WORKAROUND == 0x01)
        } while (!((pp_stat >> 28) == 1 || (pp_stat >> 28) == 2 || (pp_stat >> 28) == 3 ||
                   (pp_stat & 0xFFFFFF) == (addr & 0xFFFFFF) - 4));
    #else
        } while (!((pp_stat >> 28) == 1 || (pp_stat >> 28) == 2 || (pp_stat >> 28) == 3));
    #endif
#endif
        // update progress status
        if (upt_val % PROGRESS_STEP == 0) printProgress((double)upt_val / file_size);
        // update progress status

        // CRC Error
        if ((pp_stat >> 28) == 1 || (pp_stat >> 28) == 3) {
            printf("ERROR: PP_STAT %d\n", pp_stat >> 28);
            uint32_t confict_addr = pp_stat & 0x00FFFFFF;

            uint32_t crc_rout, crc_wout;
            tmp_addr = ISP_CONTROL_ADDR + 0x04;
            crc_wout = i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);

            tmp_addr = ISP_CONTROL_ADDR + 0x08;
            crc_rout = i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);

            // clear CRC info
            tmp_addr = ISP_CONTROL_ADDR + ISP_STATUS_CLR_OFFEST;
            i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);

            if (crc_wout != crc_rout) {
                printf("CONFLICT ADDR: 0x%08X \nCRC ERROR: 0x%08X(RCRC) != 0x%08X(WCRC)\n",
                       confict_addr,
                       crc_rout,
                       crc_wout);
                break;
            }
        }

    } while (size > 0);

    printProgress((double)upt_val / file_size);

    {
        uint32_t tmp_addr;

        //        uint32_t crc_rout, crc_wout;
        //        tmp_addr = ISP_CONTROL_ADDR + 0x04;
        //        crc_wout = i2c_single_read(CTRL_SLVID, (uint8_t *)&tmp_addr);
        //
        //        tmp_addr = ISP_CONTROL_ADDR + 0x08;
        //        crc_rout = i2c_single_read(CTRL_SLVID, (uint8_t *)&tmp_addr);

        // clear CRC info
        tmp_addr = ISP_CONTROL_ADDR + ISP_STATUS_CLR_OFFEST;

        i2c_single_read(CTRL_SLVID, (uint8_t*)&tmp_addr);
    }

    val_scu = REG_OFF;

    I2C_burst_write(CTRL_SLVID, (UINT8*)&reg_addr_scu, 1, (UINT8*)&val_scu, 1);

    return 0;
}
