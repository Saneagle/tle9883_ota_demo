/*
 ***********************************************************************************************************************
 *
 * Copyright (c) 2020-2023, Infineon Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,are permitted provided that the
 * following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, this list of conditions and the  following
 *   disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the copyright holders nor the names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE  FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY,OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************************************************************/

/*******************************************************************************
**                             Author(s) Identity                             **
********************************************************************************
** Initials     Name                                                          **
** ---------------------------------------------------------------------------**
** BG           Blandine Guillot                                              **
** JO           Julia Ott                                                     **
** EE           Erich Englbrecht                                              *
*******************************************************************************/

/*******************************************************************************
**                          Revision Control History                          **
********************************************************************************
** V1.0.0: 2020-04-28, EE:   EP-676: Initial version                          **
** V1.0.1: 2021-07-27, BG:   EP-814: Added return code for TLE_init()         **
**                           Removed clearing of status registers, should be  **
**                           done in the init functions if they are passed    **
** V1.0.2: 2021-10-06, JO:   EP-949: Corrected memory layout (IROM1 size,     **
**                           IRAM1 start and size, IRAM2 size)                **
** V1.0.3: 2021-10-15, JO:   EP-962: Updated NAC time                         **
** V1.0.4: 2021-11-11, JO:   EP-937: Updated copyright and branding           **
** V1.0.5: 2021-12-20, JO:   EP-977: Added '--diag_suppress 1609' to Asm Misc **
**                           Control to remove compiler warning               **
** V1.1.0: 2023-04-18, JO:   EP-1235: Changed to C startup file               **
*******************************************************************************/

/*******************************************************************************
**                                  Abstract                                  **
********************************************************************************
** UART: Using printf to write via UART debug pin                             **
********************************************************************************
** A string HelloWorld is written to the standard output.                     **
** A user-defined stdout target is implemented in the uart module.            **
** The redirection is established via compiler-I/O-STDOUT RTE setting.        **
** The pin P1.1 is used as output which is the debug pin on the USB device.   **
*******************************************************************************/

/*******************************************************************************
**                                  Includes                                  **
*******************************************************************************/
#include <stdio.h>
#include "tle_device.h"
#include "types.h"
#include "app_jump.h"
#include "uart_upgrade.h"

/*******************************************************************************
**                            宏定义                                          **
*******************************************************************************/
/* 定义当前应用程序版本（用于区分工程1和工程2） */
#define APP_VERSION     1    /* 工程1设为1，工程2设为2 */

/* 延时计数（用于自动跳转测试） */
#define JUMP_DELAY_MS   3000  /* 5秒后跳转 */

/*******************************************************************************
**                         全局变量                                           **
*******************************************************************************/
/* 下面的变量已不再用于固件升级，仅作为示例保留 */

sint32 main(void)
{
  sint8 s8_returnCode;
  
  /* Clear watchdog fail status */
  PMU_clrFailSafeWatchdogFailSts();

//	PMU_stopFailSafeWatchdog();
  /* Initialization of hardware modules based on Config Wizard configuration */
  s8_returnCode = TLE_init();
  
  if (s8_returnCode != ERR_LOG_SUCCESS)
  {
    /* Place your code here to handle an initialization error */
    printf("TLE_init failed with code: %d\r\n", s8_returnCode);
  }

  /* 初始化固件升级模块 */
  UART_Upgrade_Init();

  /* 打印应用程序版本信息 */
  printf("\r\n");
  printf("========================================\r\n");
  printf("  Application %d - UART Firmware Upgrade\r\n", APP_VERSION);
  printf("========================================\r\n");
  printf("This is Application %d running...\r\n", APP_VERSION);
  printf("Firmware upgrade ready via UART\r\n");
  
#if (APP_VERSION == 1)
  printf("\r\nUpgrade Protocol Commands:\r\n");
  printf("  - Send upgrade firmware via UART\r\n");
  printf("  - Use provided Python script for upgrade\r\n");
#endif
  
  printf("\r\n");

  for (;;)
  {
    /* Main watchdog service */
    (void) PMU_serviceFailSafeWatchdog();
    
    /* 处理固件升级状态机 */
    UART_Upgrade_Process();
    
    /* 检查升级是否完成 */
    if (UART_Upgrade_GetState() == UPGRADE_COMPLETE)
    {
      /* 升级完成，延时后跳转到新应用 */
      static uint32_t jump_delay = 0;
      jump_delay++;
      
      if (jump_delay > 3000000)  /* 延时约3秒 */
      {
        printf("\r\nJumping to upgraded application...\r\n\r\n");
        
#if (APP_VERSION == 1)
        Jump_To_App2();
#else
        Jump_To_App1();
#endif
        
        /* 如果跳转失败，重置状态 */
        printf("Jump failed! Please reset manually.\r\n");
        while(1)
        {
          (void) PMU_serviceFailSafeWatchdog();
        }
      }
    }
    
    /* 简单延时 */
    for (volatile uint32_t i = 0; i < 100; i++);
  }
}


#define BUFFER_SIZE (20U)

/*******************************************************************************
**                                 Variables                                  **
*******************************************************************************/
static volatile bool b_cmdTrigger = true;
static volatile uint8 u8_readCnt = 0;
static uint8 u8_buffer[BUFFER_SIZE];

/* UART receive ISR */
void uart_receive()
{
  uint8 received_byte;
  
  /* Receive byte from P1.2 */
  received_byte = (uint8) stdin_getchar();
  
  /* 将接收到的字节传递给升级模块处理 */
  UART_Upgrade_ProcessByte(received_byte);
  
  /* 旧的缓冲区处理代码（如果需要保留） */
  /* Check for buffer overflow */
  if (u8_readCnt < BUFFER_SIZE)
  {
    u8_buffer[u8_readCnt] = received_byte;
    
    /* Echo byte to stdout to show character on console */
    /* printf("%c", u8_buffer[u8_readCnt]); */  /* 注释掉回显，避免干扰协议 */
    
    u8_readCnt++;
  }
  else
  {
    /* Receive buffer is full -> handle command without newline */
    b_cmdTrigger = true;
  }
}
