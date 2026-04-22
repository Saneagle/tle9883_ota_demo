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
** Application Jump Functions for User BSL Dual Boot Implementation          **
*******************************************************************************/

/*******************************************************************************
**                                  Includes                                  **
*******************************************************************************/
#include "app_jump.h"
#include "tle989x.h"
#include "error_codes.h"
#include <stdio.h>

/*******************************************************************************
**                         Global Function Definitions                        **
*******************************************************************************/

/**
 * @brief 跳转到应用程序2
 */
sint32 Jump_To_App2(void)
{
    sint32 retval;
    volatile uint32_t *app2_vector_table;
    volatile uint32_t app2_sp;
    volatile uint32_t app2_reset_handler;
    
    printf("\r\n========================================\r\n");
    printf("Preparing to jump to APP2...\r\n");
    printf("APP2 Offset: 0x%08X\r\n", APP2_OFFSET);
    printf("APP2 Absolute Address: 0x%08X\r\n", APP2_ADDR);
    
    /* 检查 APP2 地址处的内容 */
    /* APP2 的向量表应该在 Boot Config 之后的 0x100 偏移处 */
    app2_vector_table = (volatile uint32_t *)(APP2_ADDR + 0x100);
    app2_sp = app2_vector_table[0];           /* 初始栈指针 */
    app2_reset_handler = app2_vector_table[1]; /* 复位向量 */
    
    printf("APP2 Vector Table @ 0x%08X:\r\n", (uint32_t)app2_vector_table);
    printf("  Stack Pointer:   0x%08X\r\n", app2_sp);
    printf("  Reset Handler:   0x%08X\r\n", app2_reset_handler);
    
    /* 检查向量表是否有效 */
    if (app2_sp == 0xFFFFFFFF || app2_reset_handler == 0xFFFFFFFF) {
        printf("WARNING: APP2 appears to be EMPTY (not programmed)!\r\n");
        printf("Vector table contains 0xFFFFFFFF (erased flash pattern)\r\n");
        printf("Please program APP2 before jumping.\r\n");
        printf("========================================\r\n\r\n");
        return -99;  /* 自定义错误码：APP2 未烧录 */
    }
    
    /* 检查栈指针是否在合理范围内（DSRAM 范围） */
    if (app2_sp < 0x18000000 || app2_sp > 0x18010000) {
        printf("WARNING: APP2 Stack Pointer looks invalid!\r\n");
        printf("Expected range: 0x18000000 - 0x18010000\r\n");
    }
    
    printf("APP2 looks valid. Proceeding with jump...\r\n");
    printf("========================================\r\n\r\n");
    
    /* 清理缓存 */
    user_cache_operation(CACHE_OP_AC, 0);
    
    printf("Executing custom jump sequence to APP2...\r\n");
    
    /* 先全局关中断，防止切换过程中旧中断跑新向量表 */
    __disable_irq();
    
    /* 清除所有外设级 NVIC 中断使能和挂起标志 */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    /* 禁用 SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    
    /* 切换到 MSP 特权模式 */
    __set_CONTROL(0);
    __ISB();
    
    /* 将 VTOR 指向 APP2 的向量表（VTOR 要在开中断之前设好）*/
    SCB->VTOR = (uint32_t)app2_vector_table;
    __DSB();
    __ISB();
    
    /* 内联汇编：原子地开中断、重载 MSP、跳转到 Reset_Handler
     * 避免编译器在 MSP 改变前后插入 PUSH/POP 破坏栈 */
    __asm volatile (
        "CPSIE I        \n"  /* 开全局中断 (PRIMASK=0) */
        "MSR   MSP, %0  \n"  /* 重载主栈指针 */
        "BX    %1       \n"  /* 跳转到 Reset_Handler（Thumb 地址） */
        :
        : "r" (app2_sp), "r" (app2_reset_handler)
        :
    );
    
    /* 程序不应该执行到这里 */
    while(1);
    
    return 0;
}

/**
 * @brief 跳转到应用程序1
 */
sint32 Jump_To_App1(void)
{
    sint32 retval;
    
    printf("\r\n========================================\r\n");
    printf("Preparing to jump to APP1...\r\n");
    printf("APP1 Offset: 0x%08X\r\n", APP1_OFFSET);
    printf("APP1 Absolute Address: 0x%08X\r\n", APP1_ADDR);
    printf("========================================\r\n\r\n");
    
    /* 清理缓存 */
    user_cache_operation(CACHE_OP_AC, 0);
    
    printf("Executing custom jump sequence to APP1...\r\n");
    
    /* 获取 APP1 的向量表信息 */
    volatile uint32_t *app1_vector_table = (volatile uint32_t *)(APP1_ADDR + 0x100);
    uint32_t app1_sp = app1_vector_table[0];
    uint32_t app1_reset_handler = app1_vector_table[1];

    /* 禁用所有外设级中断 */
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    /* 禁用 SysTick 定时器 */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    /* 确保使用 MSP */
    __set_CONTROL(0);
    
    /* 开启全局中断掩码 */
    __enable_irq();

    /* 设置 VTOR 寄存器指向 APP1 向量表 */
    SCB->VTOR = (uint32_t)app1_vector_table;
    
    /* 设置主栈指针 MSP */
    __set_MSP(app1_sp);
    
    __DSB();
    __ISB();
    
    /* 跳转程序到 APP1 的 Reset Handler */
    void (*app1_entry)(void) = (void (*)(void))app1_reset_handler;
    app1_entry();
    
    /* 程序不应该执行到这里 */
    while(1);
    
    return 0;
}

/**
 * @brief 软件复位（备用方案）
 */
void Software_Reset(void)
{
    printf("Performing software reset...\r\n");
    
    /* 延时确保 printf 完成 */
    for (volatile uint32_t i = 0; i < 100000; i++);
    
    /* 执行系统复位 */
    NVIC_SystemReset();
    
    /* 不会执行到这里 */
    while(1);
}
