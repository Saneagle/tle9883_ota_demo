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
static uint32_t delay_counter = 0;
static uint8_t jump_triggered = 0;

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

  /* 打印应用程序版本信息 */
  printf("\r\n");
  printf("========================================\r\n");
  printf("  Application %d - BSL Jump Test\r\n", APP_VERSION);
  printf("========================================\r\n");
  printf("This is Application %d running...\r\n", APP_VERSION);
  
#if (APP_VERSION == 1)
  printf("APP1 will jump to APP2 in %d ms\r\n", JUMP_DELAY_MS);
#else
  printf("APP2 will jump to APP1 in %d ms\r\n", JUMP_DELAY_MS);
#endif
  
  printf("\r\n");

  for (;;)
  {
    /* Main watchdog service */
    (void) PMU_serviceFailSafeWatchdog();
    
    /* 简单延时实现（每次循环约1ms） */
    if (!jump_triggered)
    {
      delay_counter++;
      
      /* 简单的延时循环 */
      for (volatile uint32_t i = 0; i < 1000; i++);
      
      /* 达到延时时间后执行跳转 */
      if (delay_counter >= JUMP_DELAY_MS)
      {
        jump_triggered = 1;
        
        printf("\r\nTime to jump!\r\n");
        
#if (APP_VERSION == 1)
        /* 工程1跳转到工程2 */
        Jump_To_App2();
#else
        /* 工程2跳转到工程1 */
        Jump_To_App1();
#endif
        
        /* 如果跳转失败，打印错误信息 */
        printf("Jump failed! Staying in APP%d\r\n", APP_VERSION);
        jump_triggered = 0;  /* 重置标志，可以再次尝试 */
        delay_counter = 0;
      }
      
      /* 每秒打印一次心跳信息 */
      if ((delay_counter % 1000) == 0)
      {
        printf("APP%d running... %d sec\r\n", APP_VERSION, delay_counter / 1000);
      }
    }
  }
}
