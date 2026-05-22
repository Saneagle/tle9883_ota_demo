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

#ifndef UART_UPGRADE_H
#define UART_UPGRADE_H

/*******************************************************************************
**                                  Includes                                  **
*******************************************************************************/
#include "types.h"

/*******************************************************************************
**                            宏定义                                          **
*******************************************************************************/

/* 升级协议命令定义 */
#define CMD_START_UPGRADE       0x01    /* 开始升级会话 */
#define CMD_DATA_PACKET         0x02    /* 数据包传输 */
#define CMD_END_UPGRADE         0x03    /* 结束升级会话 */
#define CMD_QUERY_STATUS        0x04    /* 查询升级状态 */
#define CMD_ABORT_UPGRADE       0x05    /* 中止升级 */
#define CMD_ERASE_FLASH         0x06    /* 擦除Flash */

/* 应答代码定义 */
#define ACK_OK                  0xAA    /* 成功 */
#define ACK_ERROR               0x55    /* 错误 */
#define ACK_BUSY                0x33    /* 忙碌中 */
#define ACK_CRC_ERROR           0x66    /* CRC校验错误 */

/* 数据包大小定义 */
#define FLASH_PAGE_SIZE         128     /* TLE989X Flash页大小 */
#define MAX_PACKET_SIZE         128     /* 最大数据包大小 */
#define FLASH_BUFFER_SIZE       (FLASH_PAGE_SIZE * 4)  /* Flash缓冲区 */

/* 升级目标地址（APP2的地址） */
#define UPGRADE_TARGET_ADDR     0x11004000  /* APP2起始地址（16KB偏移） */
#define MAX_UPGRADE_SIZE        (16 * 1024) /* 最大升级文件大小：16KB */

/* 协议帧结构定义 */
#define FRAME_HEADER_SIZE       5       /* 帧头大小 */
#define FRAME_CRC_SIZE          2       /* CRC16大小 */

/*******************************************************************************
**                          类型定义                                          **
*******************************************************************************/

/* 升级状态枚举 */
typedef enum
{
    UPGRADE_IDLE = 0,           /* 空闲状态 */
    UPGRADE_RECEIVING,          /* 正在接收数据 */
    UPGRADE_WRITING_FLASH,      /* 正在写入Flash */
    UPGRADE_VERIFYING,          /* 正在校验 */
    UPGRADE_COMPLETE,           /* 升级完成 */
    UPGRADE_ERROR               /* 升级出错 */
} UpgradeState_t;

/* 升级错误代码 */
typedef enum
{
    ERR_UPGRADE_OK = 0,
    ERR_UPGRADE_CRC,            /* CRC校验错误 */
    ERR_UPGRADE_SIZE,           /* 大小超限 */
    ERR_UPGRADE_FLASH_ERASE,    /* Flash擦除失败 */
    ERR_UPGRADE_FLASH_WRITE,    /* Flash写入失败 */
    ERR_UPGRADE_TIMEOUT,        /* 超时 */
    ERR_UPGRADE_SEQUENCE,       /* 序号错误 */
    ERR_UPGRADE_UNKNOWN         /* 未知错误 */
} UpgradeError_t;

/* 协议帧结构 */
typedef struct
{
    uint8  header;              /* 帧头：0x5A */
    uint8  cmd;                 /* 命令码 */
    uint16 length;              /* 数据长度 */
    uint8  sequence;            /* 序号（用于数据包） */
    uint8  data[MAX_PACKET_SIZE]; /* 数据区 */
    uint16 crc16;               /* CRC16校验 */
} UpgradeFrame_t;

/* 升级会话信息 */
typedef struct
{
    UpgradeState_t state;       /* 当前状态 */
    uint32 total_size;          /* 总大小 */
    uint32 received_size;       /* 已接收大小 */
    uint16 packet_count;        /* 数据包计数 */
    uint16 expected_seq;        /* 期望的序号 */
    uint32 target_addr;         /* 目标地址 */
    uint16 crc_calculated;      /* 计算的CRC */
    uint16 crc_received;        /* 接收的CRC */
    UpgradeError_t last_error;  /* 最后错误 */
} UpgradeSession_t;

/*******************************************************************************
**                         全局函数声明                                       **
*******************************************************************************/

/**
 * @brief 初始化固件升级模块
 */
void UART_Upgrade_Init(void);

/**
 * @brief 处理接收到的字节（从UART ISR调用）
 * @param byte 接收到的字节
 */
void UART_Upgrade_ProcessByte(uint8 byte);

/**
 * @brief 升级状态机处理函数（在main loop中调用）
 */
void UART_Upgrade_Process(void);

/**
 * @brief 开始升级会话
 * @param total_size 固件总大小
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_Start(uint32 total_size);

/**
 * @brief 处理数据包
 * @param sequence 序号
 * @param data 数据指针
 * @param length 数据长度
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_DataPacket(uint8 sequence, const uint8* data, uint16 length);

/**
 * @brief 结束升级会话
 * @param crc16 固件CRC16校验值
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_End(uint16 crc16);

/**
 * @brief 中止升级
 */
void UART_Upgrade_Abort(void);

/**
 * @brief 获取升级状态
 * @return 当前升级状态
 */
UpgradeState_t UART_Upgrade_GetState(void);

/**
 * @brief 获取升级进度（百分比）
 * @return 进度百分比 0-100
 */
uint8 UART_Upgrade_GetProgress(void);

/**
 * @brief 擦除目标Flash区域
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_EraseFlash(void);

/**
 * @brief 写入Flash缓冲区数据
 * @param offset 偏移地址
 * @param data 数据指针
 * @param length 数据长度
 * @return 成功返回0，失败返回错误码
 */
sint32 UART_Upgrade_WriteFlash(uint32 offset, const uint8* data, uint16 length);

/**
 * @brief 计算CRC16校验
 * @param data 数据指针
 * @param length 数据长度
 * @param init_val 初始值
 * @return CRC16值
 */
uint16 UART_Upgrade_CRC16(const uint8* data, uint32 length, uint16 init_val);

/**
 * @brief 发送应答
 * @param ack_code 应答码
 */
void UART_Upgrade_SendAck(uint8 ack_code);

#endif /* UART_UPGRADE_H */
