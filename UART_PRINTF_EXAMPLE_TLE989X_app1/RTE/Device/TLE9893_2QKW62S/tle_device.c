/*
 ***********************************************************************************************************************
 *
 * Copyright (c) Infineon Technologies AG
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
**                                  Includes                                  **
*******************************************************************************/

#include "tle_device.h"
#include "cantrx_defines.h"
#include "dma_defines.h"
#include "pmu_defines.h"
#ifdef UC_FEATURE_SDADC
  #include "sdadc_defines.h"
#endif 
#include "uart_defines.h"

/*******************************************************************************
**                        Private Constant Declarations                       **
*******************************************************************************/

/*******************************************************************************
**                         Private Macro Declarations                         **
*******************************************************************************/

/*******************************************************************************
**                         Global Variable Definitions                        **
*******************************************************************************/
uint32 UC_FLASH0_UBSL_SIZE;
uint32 UC_FLASH0_UDATA_START;
uint32 UC_FLASH0_UDATA_SIZE;

/*******************************************************************************
**                        Private Function Declarations                       **
*******************************************************************************/

/*******************************************************************************
**                            Global Macro Checks                             **
*******************************************************************************/
#if ((ADC1_GLOBCONF & ADC1_GLOBCONF_EN_Msk) == ADC1_GLOBCONF_EN_Msk)
  #ifndef RTE_DEVICE_TLELIB_ADC1
    #error Module ADC1 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((ADC2_GLOBCONF & ADC2_GLOBCONF_EN_Msk) == ADC2_GLOBCONF_EN_Msk)
  #ifndef RTE_DEVICE_TLELIB_ADC2
    #error Module ADC2 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (BDRV_GLOBCONF_EN == 1u)
  #ifndef RTE_DEVICE_TLELIB_BDRV
    #error Module BDRV enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((CANTRX_CTRL & CANTRX_CTRL_EN_Msk) == CANTRX_CTRL_EN_Msk)
  #ifndef RTE_DEVICE_TLELIB_CANTRX
    #error Module CANTRX enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (CCU7_EN == 1)
  #ifndef RTE_DEVICE_TLELIB_CCU7
    #error Module CCU7 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((CSACSC_CTRL1 & CSACSC_CTRL1_CSAC_EN_Msk) == CSACSC_CTRL1_CSAC_EN_Msk)
  #ifndef RTE_DEVICE_TLELIB_CSACSC
    #error Module CSACSC enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (DMA_GLOBCONF_EN == 1u)
  #ifndef RTE_DEVICE_TLELIB_DMA
    #error Module DMA enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((SCU_PMCON & SCU_PMCON_GPT12_DIS_Msk) == 0u)
  #ifndef RTE_DEVICE_TLELIB_GPT12
    #error Module GPT12 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (MON_EN == 1)
  #ifndef RTE_DEVICE_TLELIB_MON
    #error Module MON enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (defined SDADC_GLOBCONF_EN) && (SDADC_GLOBCONF_EN == 1u)
  #ifndef RTE_DEVICE_TLELIB_SDADC
    #error Module SDADC enabled in Config Wizard, but not in RTE!
  #endif
#endif 

#if ((SCU_PMCON & SCU_PMCON_SSC0_DIS_Msk) == 0u)
  #ifndef RTE_DEVICE_TLELIB_SSC
    #error Module SSC0 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((SCU_PMCON & SCU_PMCON_SSC1_DIS_Msk) == 0u)
  #ifndef RTE_DEVICE_TLELIB_SSC
    #error Module SSC1 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((SCU_PMCON & SCU_PMCON_T2_DIS_Msk) == 0u)
  #ifndef RTE_DEVICE_TLELIB_TIMER2X
    #error Module T2 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if ((SCU_PMCON & SCU_PMCON_T21_DIS_Msk) == 0u)
  #ifndef RTE_DEVICE_TLELIB_TIMER2X
    #error Module T21 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (UART0_GLOBCONF_EN == 1u)
  #ifndef RTE_DEVICE_TLELIB_UART
    #error Module UART0 enabled in Config Wizard, but not in RTE!
  #endif
#endif

#if (UART1_GLOBCONF_EN == 1u)
  #ifndef RTE_DEVICE_TLELIB_UART
    #error Module UART1 enabled in Config Wizard, but not in RTE!
  #endif
#endif

/*******************************************************************************
**                         Global Function Definitions                        **
*******************************************************************************/

/** \brief Initialize all modules of the device
 *
 * \return sint8 0: success, <0: error codes
 */
sint8 TLE_init(void)
{
  //lint --e{9027}
  sint8 s8_returnCode;
  uint32 u32_ubslNvmSize;
  uint32 u32_codeNvmSize;
  uint32 u32_dataNvmSize;
  s8_returnCode = ERR_LOG_SUCCESS;
#ifdef RTE_DEVICE_TLELIB_SCU
  SCU_init();
#endif
#ifdef RTE_DEVICE_TLELIB_PMU
  s8_returnCode |= PMU_init();
#endif
#ifdef RTE_DEVICE_TLELIB_ADC1
  s8_returnCode |= ADC1_init();
#endif
#ifdef RTE_DEVICE_TLELIB_ADC2
  s8_returnCode |= ADC2_init();
#endif
#ifdef RTE_DEVICE_TLELIB_BDRV
  s8_returnCode |= BDRV_initCP();
#endif
#ifdef RTE_DEVICE_TLELIB_CSACSC
  s8_returnCode |= CSACSC_init();
#endif
#ifdef RTE_DEVICE_TLELIB_BDRV
  s8_returnCode |= BDRV_init();
#endif
#ifdef RTE_DEVICE_TLELIB_CCU7
  s8_returnCode |= CCU7_init();
#endif
#ifdef RTE_DEVICE_TLELIB_CANTRX
  s8_returnCode |= CANTRX_init();
#endif
#ifdef RTE_DEVICE_TLELIB_DMA
  s8_returnCode |= DMA_init();
#endif
#ifdef RTE_DEVICE_TLELIB_GPIO
  GPIO_init();
#endif
#ifdef RTE_DEVICE_TLELIB_GPT12
  s8_returnCode |= GPT12_init();
#endif
#ifdef RTE_DEVICE_TLELIB_MON
  MON_init();
#endif
#ifdef RTE_DEVICE_TLELIB_SDADC
  s8_returnCode |= SDADC_init();
#endif
#ifdef RTE_DEVICE_TLELIB_SSC
  s8_returnCode |= SSC0_init();
  s8_returnCode |= SSC1_init();
#endif
#ifdef RTE_DEVICE_TLELIB_TIMER2X
  s8_returnCode |= T20_init();
  s8_returnCode |= T21_init();
#endif
#ifdef RTE_DEVICE_TLELIB_UART
  s8_returnCode |= UART0_init();
  s8_returnCode |= UART1_init();
#endif
#ifdef RTE_DEVICE_TLELIB_INT
  INT_init();
#endif
  /* Initialize variables for memory map that can be configured */
  (void)user_nvm_config_get(&u32_ubslNvmSize, &u32_codeNvmSize, &u32_dataNvmSize);
  UC_FLASH0_UBSL_SIZE = (uint32) (u32_ubslNvmSize + 1U) * UC_FLASH_SECTOR_SIZE;
  UC_FLASH0_UDATA_START = UC_FLASH0_UBSL_START + UC_FLASH0_UBSL_SIZE;
  UC_FLASH0_UDATA_SIZE = UC_FLASH0_SIZE - UC_FLASH0_UBSL_SIZE;
  return s8_returnCode;
}
