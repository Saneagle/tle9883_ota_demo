/*
 ***********************************************************************************************************************
 *
 * Copyright (c) 2020-2024, Infineon Technologies AG
 * All rights reserved.
 *
 **********************************************************************************************************************/

/*******************************************************************************
**                                  Abstract                                  **
********************************************************************************
** UART Firmware Upgrade Protocol Implementation                              **
*******************************************************************************/

/*******************************************************************************
**                                  Includes                                  **
*******************************************************************************/
#include "uart_upgrade.h"
#include "bootrom.h"
#include "error_codes.h"
#include "app_jump.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
**                            本地宏定义                                      **
*******************************************************************************/
#define FRAME_HEADER            0x5A    /* 帧头标识 */
#define RX_TIMEOUT_MS           5000    /* 接收超时（毫秒） */

/* NVM操作相关 */
#define NVM_OPTIONS_NONE        0x00
#define NVM_SEG_PROT_CODE_NO_ERASE  0x43219876u  /* UCODE段写保护密码 */

/*******************************************************************************
**                         本地类型定义                                       **
*******************************************************************************/
/* 接收状态机 */
typedef enum
{
    RX_STATE_IDLE = 0,
    RX_STATE_HEADER,
    RX_STATE_CMD,
    RX_STATE_LENGTH_H,
    RX_STATE_LENGTH_L,
    RX_STATE_SEQUENCE,
    RX_STATE_DATA,
    RX_STATE_CRC_H,
    RX_STATE_CRC_L
} RxState_t;

/*******************************************************************************
**                         本地变量                                           **
*******************************************************************************/
static UpgradeSession_t g_session;              /* 升级会话 */
static RxState_t g_rx_state = RX_STATE_IDLE;    /* 接收状态 */
static UpgradeFrame_t g_rx_frame;               /* 接收帧缓冲 */
static uint16 g_rx_data_index = 0;              /* 接收数据索引 */
static uint8 g_flash_buffer[FLASH_BUFFER_SIZE]; /* Flash写入缓冲区 */
static uint16 g_flash_buf_index = 0;            /* Flash缓冲区索引 */
static uint32 g_timeout_counter = 0;            /* 超时计数器 */

/*******************************************************************************
**                         本地函数声明                                       **
*******************************************************************************/
static void ProcessFrame(void);
static void ResetRxState(void);
static sint32 FlushFlashBuffer(void);

/*******************************************************************************
**                         全局函数实现                                       **
*******************************************************************************/

/**
 * @brief 初始化固件升级模块
 */
void UART_Upgrade_Init(void)
{
    memset(&g_session, 0, sizeof(g_session));
    g_session.state = UPGRADE_IDLE;
    g_rx_state = RX_STATE_IDLE;
    g_rx_data_index = 0;
    g_flash_buf_index = 0;
    
    printf("UART Upgrade Module Initialized\r\n");
}

/**
 * @brief 处理接收到的字节（从UART ISR调用）
 * @param byte 接收到的字节
 */
void UART_Upgrade_ProcessByte(uint8 byte)
{
    switch (g_rx_state)
    {
        case RX_STATE_IDLE:
            if (byte == FRAME_HEADER)
            {
                g_rx_frame.header = byte;
                g_rx_state = RX_STATE_CMD;
                g_rx_data_index = 0;
            }
            break;
            
        case RX_STATE_CMD:
            g_rx_frame.cmd = byte;
            g_rx_state = RX_STATE_LENGTH_H;
            break;
            
        case RX_STATE_LENGTH_H:
            g_rx_frame.length = (uint16)byte << 8;
            g_rx_state = RX_STATE_LENGTH_L;
            break;
            
        case RX_STATE_LENGTH_L:
            g_rx_frame.length |= byte;
            g_rx_state = RX_STATE_SEQUENCE;
            break;
            
        case RX_STATE_SEQUENCE:
            g_rx_frame.sequence = byte;
            g_rx_data_index = 0;
            
            /* 如果没有数据，直接跳到CRC */
            if (g_rx_frame.length == 0)
            {
                g_rx_state = RX_STATE_CRC_H;
            }
            else
            {
                g_rx_state = RX_STATE_DATA;
            }
            break;
            
        case RX_STATE_DATA:
            if (g_rx_data_index < MAX_PACKET_SIZE)
            {
                g_rx_frame.data[g_rx_data_index++] = byte;
                
                if (g_rx_data_index >= g_rx_frame.length)
                {
                    g_rx_state = RX_STATE_CRC_H;
                }
            }
            else
            {
                /* 数据溢出，复位状态 */
                ResetRxState();
            }
            break;
            
        case RX_STATE_CRC_H:
            g_rx_frame.crc16 = (uint16)byte << 8;
            g_rx_state = RX_STATE_CRC_L;
            break;
            
        case RX_STATE_CRC_L:
            g_rx_frame.crc16 |= byte;
            
            /* 一帧接收完成，处理 */
            ProcessFrame();
            ResetRxState();
            break;
            
        default:
            ResetRxState();
            break;
    }
}

/**
 * @brief 升级状态机处理函数（在main loop中调用）
 */
void UART_Upgrade_Process(void)
{
    /* 这里可以添加超时检测等逻辑 */
    if (g_session.state == UPGRADE_RECEIVING)
    {
        g_timeout_counter++;
        
        /* 简单的超时处理（需要根据实际定时调整） */
        if (g_timeout_counter > RX_TIMEOUT_MS)
        {
            printf("Upgrade timeout, aborting...\r\n");
            UART_Upgrade_Abort();
        }
    }
}

/**
 * @brief 开始升级会话
 * @param total_size 固件总大小
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_Start(uint32 total_size)
{
    printf("\r\n========================================\r\n");
    printf("Starting Firmware Upgrade Session\r\n");
    printf("Target Size: %u bytes\r\n", (unsigned int)total_size);
    printf("Target Address: 0x%08X\r\n", (unsigned int)UPGRADE_TARGET_ADDR);
    
    /* 检查大小 */
    if (total_size > MAX_UPGRADE_SIZE)
    {
        printf("ERROR: Size exceeds maximum (%u bytes)\r\n", MAX_UPGRADE_SIZE);
        g_session.last_error = ERR_UPGRADE_SIZE;
        return -1;
    }
    
    /* 初始化会话 */
    memset(&g_session, 0, sizeof(g_session));
    g_session.state = UPGRADE_IDLE;
    g_session.total_size = total_size;
    g_session.target_addr = UPGRADE_TARGET_ADDR;
    g_session.expected_seq = 0;
    g_flash_buf_index = 0;
    
    /* 擦除Flash */
    printf("Erasing target Flash area...\r\n");
    if (UART_Upgrade_EraseFlash() != 0)
    {
        printf("ERROR: Flash erase failed\r\n");
        g_session.last_error = ERR_UPGRADE_FLASH_ERASE;
        g_session.state = UPGRADE_ERROR;
        return -2;
    }
    
    printf("Flash erased successfully\r\n");
    printf("Ready to receive data packets\r\n");
    printf("========================================\r\n\r\n");
    
    g_session.state = UPGRADE_RECEIVING;
    g_session.crc_calculated = 0xFFFF;  /* CRC初始值 */
    g_timeout_counter = 0;
    
    return 0;
}

/**
 * @brief 处理数据包
 * @param sequence 序号
 * @param data 数据指针
 * @param length 数据长度
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_DataPacket(uint8 sequence, const uint8* data, uint16 length)
{
    uint16 i;
    sint32 ret;
    
    /* 检查状态 */
    if (g_session.state != UPGRADE_RECEIVING)
    {
        printf("ERROR: Not in receiving state\r\n");
        return -1;
    }
    
    /* 检查序号 */
    if (sequence != (g_session.expected_seq & 0xFF))
    {
        printf("ERROR: Sequence mismatch. Expected %u, got %u\r\n", 
               g_session.expected_seq & 0xFF, sequence);
        g_session.last_error = ERR_UPGRADE_SEQUENCE;
        return -2;
    }
    
    /* 检查是否超过总大小 */
    if ((g_session.received_size + length) > g_session.total_size)
    {
        printf("ERROR: Data exceeds total size\r\n");
        g_session.last_error = ERR_UPGRADE_SIZE;
        return -3;
    }
    
    /* 更新CRC */
    g_session.crc_calculated = UART_Upgrade_CRC16(data, length, g_session.crc_calculated);
    
    /* 将数据复制到Flash缓冲区 */
    for (i = 0; i < length; i++)
    {
        g_flash_buffer[g_flash_buf_index++] = data[i];
        
        /* 缓冲区满，写入Flash */
        if (g_flash_buf_index >= FLASH_PAGE_SIZE)
        {
            ret = FlushFlashBuffer();
            if (ret != 0)
            {
                printf("ERROR: Flash write failed at offset 0x%08X\r\n", 
                       (unsigned int)g_session.received_size);
                g_session.last_error = ERR_UPGRADE_FLASH_WRITE;
                g_session.state = UPGRADE_ERROR;
                return -4;
            }
        }
    }
    
    /* 更新统计 */
    g_session.received_size += length;
    g_session.packet_count++;
    g_session.expected_seq++;
    g_timeout_counter = 0;  /* 重置超时计数 */
    
    /* 打印进度 */
    if ((g_session.packet_count % 10) == 0)
    {
        printf("Progress: %u/%u bytes (%u%%)\r\n", 
               (unsigned int)g_session.received_size,
               (unsigned int)g_session.total_size,
               UART_Upgrade_GetProgress());
    }
    
    return 0;
}

/**
 * @brief 结束升级会话
 * @param crc16 固件CRC16校验值
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_End(uint16 crc16)
{
    sint32 ret;
    
    printf("\r\n========================================\r\n");
    printf("Finalizing Firmware Upgrade...\r\n");
    
    /* 写入剩余缓冲区数据 */
    if (g_flash_buf_index > 0)
    {
        /* 填充剩余部分为0xFF */
        while (g_flash_buf_index < FLASH_PAGE_SIZE)
        {
            g_flash_buffer[g_flash_buf_index++] = 0xFF;
        }
        
        ret = FlushFlashBuffer();
        if (ret != 0)
        {
            printf("ERROR: Final flash write failed\r\n");
            g_session.last_error = ERR_UPGRADE_FLASH_WRITE;
            g_session.state = UPGRADE_ERROR;
            return -1;
        }
    }
    
    /* 校验CRC */
    printf("Verifying CRC...\r\n");
    printf("  Calculated CRC: 0x%04X\r\n", g_session.crc_calculated);
    printf("  Received CRC:   0x%04X\r\n", crc16);
    
    if (g_session.crc_calculated != crc16)
    {
        printf("ERROR: CRC mismatch!\r\n");
        g_session.last_error = ERR_UPGRADE_CRC;
        g_session.state = UPGRADE_ERROR;
        return -2;
    }
    
    printf("CRC verification passed!\r\n");
    printf("Total received: %u bytes\r\n", (unsigned int)g_session.received_size);
    printf("Total packets: %u\r\n", g_session.packet_count);
    printf("Firmware upgrade completed successfully!\r\n");
    printf("========================================\r\n\r\n");
    
    g_session.state = UPGRADE_COMPLETE;
    
    return 0;
}

/**
 * @brief 中止升级
 */
void UART_Upgrade_Abort(void)
{
    printf("Upgrade aborted!\r\n");
    g_session.state = UPGRADE_ERROR;
    g_flash_buf_index = 0;
    ResetRxState();
}

/**
 * @brief 获取升级状态
 * @return 当前升级状态
 */
UpgradeState_t UART_Upgrade_GetState(void)
{
    return g_session.state;
}

/**
 * @brief 获取升级进度（百分比）
 * @return 进度百分比 0-100
 */
uint8 UART_Upgrade_GetProgress(void)
{
    if (g_session.total_size == 0)
    {
        return 0;
    }
    
    return (uint8)((g_session.received_size * 100) / g_session.total_size);
}

/**
 * @brief 擦除目标Flash区域
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_EraseFlash(void)
{
    sint32 ret;
    uint32 addr;
    uint32 num_pages;
    uint32 i;
    
    /* 计算需要擦除的页数 (每页128字节) */
    num_pages = (g_session.total_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    
    printf("Erasing %u pages...\r\n", (unsigned int)num_pages);
    
    /* 解除写保护 */
    ret = user_nvm_ucode_temp_protect_clear(NVM_SEG_PROT_CODE_NO_ERASE);
    if (ret != ERR_LOG_SUCCESS)
    {
        printf("ERROR: Failed to clear write protection: %d\r\n", (int)ret);
        return ret;
    }
    
    /* 逐页擦除 */
    for (i = 0; i < num_pages; i++)
    {
        addr = g_session.target_addr + (i * FLASH_PAGE_SIZE);
        
        ret = user_nvm_page_erase(addr, NVM_OPTIONS_NONE);
        if (ret != ERR_LOG_SUCCESS)
        {
            printf("ERROR: Page erase failed at 0x%08X: %d\r\n", (unsigned int)addr, (int)ret);
            user_nvm_ucode_temp_protect_set(NVM_SEG_PROT_CODE_NO_ERASE);
            return ret;
        }
        
        if ((i % 10) == 0)
        {
            printf("  Erased %u/%u pages\r\n", (unsigned int)i, (unsigned int)num_pages);
        }
    }
    
    /* 恢复写保护 */
    user_nvm_ucode_temp_protect_set(NVM_SEG_PROT_CODE_NO_ERASE);
    
    return ERR_LOG_SUCCESS;
}

/**
 * @brief 写入Flash缓冲区数据
 * @param offset 偏移地址
 * @param data 数据指针
 * @param length 数据长度
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_WriteFlash(uint32 offset, const uint8* data, uint16 length)
{
    sint32 ret;
    uint32 target_addr;
    user_nvm_page_write_t write_params;
    
    target_addr = g_session.target_addr + offset;
    
    /* 设置写参数 */
    write_params.data = (uint8*)data;
    write_params.nbyte = length;
    write_params.options = NVM_OPTIONS_NONE;
    
    /* 解除写保护 */
    ret = user_nvm_ucode_temp_protect_clear(NVM_SEG_PROT_CODE_NO_ERASE);
    if (ret != ERR_LOG_SUCCESS)
    {
        return ret;
    }
    
    /* 写入Flash */
    ret = user_nvm_page_write(target_addr, &write_params);
    
    /* 恢复写保护 */
    user_nvm_ucode_temp_protect_set(NVM_SEG_PROT_CODE_NO_ERASE);
    
    if (ret != ERR_LOG_SUCCESS)
    {
        printf("Flash write error: %d\r\n", (int)ret);
        return ret;
    }
    
    return ERR_LOG_SUCCESS;
}

/**
 * @brief 计算CRC16校验（MODBUS CRC16）
 * @param data 数据指针
 * @param length 数据长度
 * @param init_val 初始值
 * @return CRC16值
 */
uint16 UART_Upgrade_CRC16(const uint8* data, uint32 length, uint16 init_val)
{
    uint16 crc = init_val;
    uint32 i;
    uint8 j;
    
    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief 发送应答
 * @param ack_code 应答码
 */
void UART_Upgrade_SendAck(uint8 ack_code)
{
    /* 简单的应答：0x5A + ACK码 + CRC16 */
    uint8 response[4];
    uint16 crc;
    
    response[0] = FRAME_HEADER;
    response[1] = ack_code;
    
    crc = UART_Upgrade_CRC16(response, 2, 0xFFFF);
    response[2] = (uint8)(crc >> 8);
    response[3] = (uint8)(crc & 0xFF);
    
    /* 发送应答 */
    printf("%c%c%c%c", response[0], response[1], response[2], response[3]);
}

/*******************************************************************************
**                         本地函数实现                                       **
*******************************************************************************/

/**
 * @brief 处理接收到的完整帧
 */
static void ProcessFrame(void)
{
    uint16 calc_crc;
    uint8 crc_data[5 + MAX_PACKET_SIZE];
    uint16 crc_len;
    sint32 ret;
    uint32 fw_size;
    
    /* 准备CRC计算数据 */
    crc_data[0] = g_rx_frame.header;
    crc_data[1] = g_rx_frame.cmd;
    crc_data[2] = (uint8)(g_rx_frame.length >> 8);
    crc_data[3] = (uint8)(g_rx_frame.length & 0xFF);
    crc_data[4] = g_rx_frame.sequence;
    
    memcpy(&crc_data[5], g_rx_frame.data, g_rx_frame.length);
    crc_len = 5 + g_rx_frame.length;
    
    /* 校验CRC */
    calc_crc = UART_Upgrade_CRC16(crc_data, crc_len, 0xFFFF);
    
    if (calc_crc != g_rx_frame.crc16)
    {
        printf("Frame CRC error! Calculated: 0x%04X, Received: 0x%04X\r\n", 
               calc_crc, g_rx_frame.crc16);
        UART_Upgrade_SendAck(ACK_CRC_ERROR);
        return;
    }
    
    /* 处理命令 */
    switch (g_rx_frame.cmd)
    {
        case CMD_START_UPGRADE:
            /* 数据格式：4字节固件大小（大端） */
            if (g_rx_frame.length == 4)
            {
                fw_size = ((uint32)g_rx_frame.data[0] << 24) |
                         ((uint32)g_rx_frame.data[1] << 16) |
                         ((uint32)g_rx_frame.data[2] << 8) |
                         ((uint32)g_rx_frame.data[3]);
                
                ret = UART_Upgrade_Start(fw_size);
                if (ret == 0)
                {
                    UART_Upgrade_SendAck(ACK_OK);
                }
                else
                {
                    UART_Upgrade_SendAck(ACK_ERROR);
                }
            }
            else
            {
                UART_Upgrade_SendAck(ACK_ERROR);
            }
            break;
            
        case CMD_DATA_PACKET:
            ret = UART_Upgrade_DataPacket(g_rx_frame.sequence, 
                                         g_rx_frame.data, 
                                         g_rx_frame.length);
            if (ret == 0)
            {
                UART_Upgrade_SendAck(ACK_OK);
            }
            else
            {
                UART_Upgrade_SendAck(ACK_ERROR);
            }
            break;
            
        case CMD_END_UPGRADE:
            /* 数据格式：2字节CRC（大端） */
            if (g_rx_frame.length == 2)
            {
                uint16 fw_crc = ((uint16)g_rx_frame.data[0] << 8) | g_rx_frame.data[1];
                
                ret = UART_Upgrade_End(fw_crc);
                if (ret == 0)
                {
                    UART_Upgrade_SendAck(ACK_OK);
                    
                    /* 升级成功，延时后跳转到APP2 */
                    printf("\r\nWill jump to new application in 3 seconds...\r\n");
                    
                    /* 这里可以设置一个标志，在main循环中执行跳转 */
                    /* Jump_To_App2(); */
                }
                else
                {
                    UART_Upgrade_SendAck(ACK_ERROR);
                }
            }
            else
            {
                UART_Upgrade_SendAck(ACK_ERROR);
            }
            break;
            
        case CMD_ABORT_UPGRADE:
            UART_Upgrade_Abort();
            UART_Upgrade_SendAck(ACK_OK);
            break;
            
        case CMD_QUERY_STATUS:
            /* 返回当前状态和进度 */
            {
                uint8 progress = UART_Upgrade_GetProgress();
                printf("Status: State=%u, Progress=%u%%\r\n", 
                       g_session.state, progress);
                UART_Upgrade_SendAck(ACK_OK);
            }
            break;
            
        default:
            printf("Unknown command: 0x%02X\r\n", g_rx_frame.cmd);
            UART_Upgrade_SendAck(ACK_ERROR);
            break;
    }
}

/**
 * @brief 重置接收状态机
 */
static void ResetRxState(void)
{
    g_rx_state = RX_STATE_IDLE;
    g_rx_data_index = 0;
}

/**
 * @brief 刷新Flash缓冲区（写入Flash）
 * @return 成功返回0，失败返回错误码
 */
static sint32 FlushFlashBuffer(void)
{
    sint32 ret;
    uint32 write_offset;
    
    if (g_flash_buf_index == 0)
    {
        return 0;  /* 没有数据要写入 */
    }
    
    /* 计算写入偏移 */
    write_offset = g_session.received_size - g_flash_buf_index + g_rx_data_index;
    
    /* 如果不满一页，填充0xFF */
    while (g_flash_buf_index < FLASH_PAGE_SIZE)
    {
        g_flash_buffer[g_flash_buf_index++] = 0xFF;
    }
    
    /* 写入Flash */
    g_session.state = UPGRADE_WRITING_FLASH;
    ret = UART_Upgrade_WriteFlash(write_offset, g_flash_buffer, FLASH_PAGE_SIZE);
    g_session.state = UPGRADE_RECEIVING;
    
    if (ret != ERR_LOG_SUCCESS)
    {
        return ret;
    }
    
    /* 重置缓冲区 */
    g_flash_buf_index = 0;
    
    return 0;
}
