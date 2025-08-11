#ifndef _HAPTIC_NV_REG_H_
#define _HAPTIC_NV_REG_H_
/********************************************
 * AW862XX Register List
 *******************************************/
#define AW862XX_REG_ID									(0x00)
#define AW862XX_REG_SYSST								(0x01)
#define AW862XX_REG_SYSINT								(0x02)
#define AW862XX_REG_SYSINTM								(0x03)
#define AW862XX_REG_SYSST2								(0x04)
#define AW862XX_REG_SYSER								(0x05)
#define AW862XX_REG_PLAYCFG2							(0x07)
#define AW862XX_REG_PLAYCFG3							(0x08)
#define AW862XX_REG_PLAYCFG4							(0x09)
#define AW862XX_REG_WAVCFG1								(0x0A)
#define AW862XX_REG_WAVCFG2								(0x0B)
#define AW862XX_REG_WAVCFG3								(0x0C)
#define AW862XX_REG_WAVCFG4								(0x0D)
#define AW862XX_REG_WAVCFG5								(0x0E)
#define AW862XX_REG_WAVCFG6								(0x0F)
#define AW862XX_REG_WAVCFG7								(0x10)
#define AW862XX_REG_WAVCFG8								(0x11)
#define AW862XX_REG_WAVCFG9								(0x12)
#define AW862XX_REG_WAVCFG10							(0x13)
#define AW862XX_REG_WAVCFG11							(0x14)
#define AW862XX_REG_WAVCFG12							(0x15)
#define AW862XX_REG_WAVCFG13							(0x16)
#define AW862XX_REG_CONTCFG1							(0x18)
#define AW862XX_REG_CONTCFG2							(0x19)
#define AW862XX_REG_CONTCFG3							(0x1A)
#define AW862XX_REG_CONTCFG4							(0x1B)
#define AW862XX_REG_CONTCFG5							(0x1C)
#define AW862XX_REG_CONTCFG6							(0x1D)
#define AW862XX_REG_CONTCFG7							(0x1E)
#define AW862XX_REG_CONTCFG8							(0x1F)
#define AW862XX_REG_CONTCFG9							(0x20)
#define AW862XX_REG_CONTCFG10							(0x21)
#define AW862XX_REG_CONTCFG11							(0x22)
#define AW862XX_REG_CONTCFG12							(0x23)
#define AW862XX_REG_CONTCFG13							(0x24)
#define AW862XX_REG_CONTRD14							(0x25)
#define AW862XX_REG_CONTRD15							(0x26)
#define AW862XX_REG_CONTRD16							(0x27)
#define AW862XX_REG_CONTRD17							(0x28)
#define AW862XX_REG_CONTRD18							(0x29)
#define AW862XX_REG_CONTRD19							(0x2A)
#define AW862XX_REG_CONTRD20							(0x2B)
#define AW862XX_REG_CONTRD21							(0x2C)
#define AW862XX_REG_RTPCFG1								(0x2D)
#define AW862XX_REG_RTPCFG2								(0x2E)
#define AW862XX_REG_RTPCFG3								(0x2F)
#define AW862XX_REG_RTPCFG4								(0x30)
#define AW862XX_REG_RTPCFG5								(0x31)
#define AW862XX_REG_RTPDATA								(0x32)
#define AW862XX_REG_TRGCFG1								(0x33)
#define AW862XX_REG_TRGCFG2								(0x34)
#define AW862XX_REG_TRGCFG3								(0x35)
#define AW862XX_REG_TRGCFG4								(0x36)
#define AW862XX_REG_TRGCFG5								(0x37)
#define AW862XX_REG_TRGCFG6								(0x38)
#define AW862XX_REG_TRGCFG7								(0x39)
#define AW862XX_REG_TRGCFG8								(0x3A)
#define AW862XX_REG_GLBCFG2								(0x3C)
#define AW862XX_REG_GLBCFG4								(0x3E)
#define AW862XX_REG_GLBRD5								(0x3F)
#define AW862XX_REG_RAMADDRH							(0x40)
#define AW862XX_REG_RAMADDRL							(0x41)
#define AW862XX_REG_RAMDATA								(0x42)
#define AW862XX_REG_SYSCTRL1							(0x43)
#define AW862XX_REG_SYSCTRL2							(0x44)
#define AW862XX_REG_SYSCTRL3							(0x45)
#define AW862XX_REG_SYSCTRL4							(0x46)
#define AW862XX_REG_SYSCTRL5							(0x47)
#define AW862XX_REG_SYSCTRL6							(0x48)
#define AW862XX_REG_SYSCTRL7							(0x49)
#define AW862XX_REG_PWMCFG1								(0x4C)
#define AW862XX_REG_PWMCFG3								(0x4E)
#define AW862XX_REG_PWMCFG4								(0x4F)
#define AW862XX_REG_DETCFG1								(0x51)
#define AW862XX_REG_DETCFG2								(0x52)
#define AW862XX_REG_DET_RL								(0x53)
#define AW862XX_REG_DET_OS								(0x54)
#define AW862XX_REG_DET_VBAT							(0x55)
#define AW862XX_REG_DET_LO								(0x57)
#define AW862XX_REG_TRIMCFG1							(0x58)
#define AW862XX_REG_TRIMCFG3							(0x5A)
#define AW862XX_REG_EFRD9								(0x64)
#define AW862XX_REG_ANACFG8								(0x77)

/********************************************
 * AW862X Register List
 *******************************************/
#define AW862X_REG_ID									(0x00)
#define AW862X_REG_SYSST								(0x01)
#define AW862X_REG_SYSINT								(0x02)
#define AW862X_REG_SYSINTM								(0x03)
#define AW862X_REG_SYSCTRL								(0x04)
#define AW862X_REG_GO									(0x05)
#define AW862X_REG_RTP_DATA								(0x06)
#define AW862X_REG_WAVSEQ1								(0x07)
#define AW862X_REG_WAVSEQ2								(0x08)
#define AW862X_REG_WAVSEQ3								(0x09)
#define AW862X_REG_WAVSEQ4								(0x0a)
#define AW862X_REG_WAVSEQ5								(0x0b)
#define AW862X_REG_WAVSEQ6								(0x0c)
#define AW862X_REG_WAVSEQ7								(0x0d)
#define AW862X_REG_WAVSEQ8								(0x0e)
#define AW862X_REG_WAVLOOP1								(0x0f)
#define AW862X_REG_WAVLOOP2								(0x10)
#define AW862X_REG_WAVLOOP3								(0x11)
#define AW862X_REG_WAVLOOP4								(0x12)
#define AW862X_REG_MANLOOP								(0x13)
#define AW862X_REG_TRG1_SEQP							(0x14)
#define AW862X_REG_TRG1_SEQN							(0x17)
#define AW862X_REG_TRG_CFG1								(0x1b)
#define AW862X_REG_TRG_CFG2								(0x1c)
#define AW862X_REG_DBGCTRL								(0x20)
#define AW862X_REG_BASE_ADDRH							(0x21)
#define AW862X_REG_BASE_ADDRL							(0x22)
#define AW862X_REG_FIFO_AEH								(0x23)
#define AW862X_REG_FIFO_AEL								(0x24)
#define AW862X_REG_FIFO_AFH								(0x25)
#define AW862X_REG_FIFO_AFL								(0x26)
#define AW862X_REG_DATCTRL								(0x2b)
#define AW862X_REG_PWMPRC								(0x2d)
#define AW862X_REG_PWMDBG								(0x2e)
#define AW862X_REG_DBGSTAT								(0x30)
#define AW862X_REG_WAVECTRL								(0x31)
#define AW862X_REG_BRAKE0_CTRL							(0x32)
#define AW862X_REG_BRAKE1_CTRL							(0x33)
#define AW862X_REG_BRAKE2_CTRL							(0x34)
#define AW862X_REG_BRAKE_NUM							(0x35)
#define AW862X_REG_ANACTRL								(0x38)
#define AW862X_REG_SW_BRAKE								(0x39)
#define AW862X_REG_DATDBG								(0x3b)
#define AW862X_REG_PRLVL								(0x3e)
#define AW862X_REG_PRTIME								(0x3f)
#define AW862X_REG_RAMADDRH								(0x40)
#define AW862X_REG_RAMADDRL								(0x41)
#define AW862X_REG_RAMDATA								(0x42)
#define AW862X_REG_BRA_MAX_NUM							(0x44)
#define AW862X_REG_GLB_STATE							(0x47)
#define AW862X_REG_CONT_CTRL							(0x48)
#define AW862X_REG_F_PRE_H								(0x49)
#define AW862X_REG_F_PRE_L								(0x4a)
#define AW862X_REG_TD_H									(0x4b)
#define AW862X_REG_TD_L									(0x4c)
#define AW862X_REG_TSET									(0x4d)
#define AW862X_REG_THRS_BRA_END							(0x4f)
#define AW862X_REG_EF_RDATAH							(0x55)
#define AW862X_REG_TRIM_LRA								(0x5b)
#define AW862X_REG_R_SPARE								(0x5d)
#define AW862X_REG_D2SCFG								(0x5e)
#define AW862X_REG_DETCTRL								(0x5f)
#define AW862X_REG_RLDET								(0x60)
#define AW862X_REG_OSDET								(0x61)
#define AW862X_REG_VBATDET								(0x62)
#define AW862X_REG_ADCTEST								(0x66)
#define AW862X_REG_F_LRA_F0_H							(0x68)
#define AW862X_REG_F_LRA_F0_L							(0x69)
#define AW862X_REG_F_LRA_CONT_H							(0x6a)
#define AW862X_REG_F_LRA_CONT_L							(0x6b)
#define AW862X_REG_WAIT_VOL_MP							(0x6e)
#define AW862X_REG_WAIT_VOL_MN							(0x6f)
#define AW862X_REG_ZC_THRSH_H							(0x72)
#define AW862X_REG_ZC_THRSH_L							(0x73)
#define AW862X_REG_BEMF_VTHH_H							(0x74)
#define AW862X_REG_BEMF_VTHH_L							(0x75)
#define AW862X_REG_BEMF_VTHL_H							(0x76)
#define AW862X_REG_BEMF_VTHL_L							(0x77)
#define AW862X_REG_BEMF_NUM								(0x78)
#define AW862X_REG_DRV_TIME								(0x79)
#define AW862X_REG_TIME_NZC								(0x7a)
#define AW862X_REG_DRV_LVL								(0x7b)
#define AW862X_REG_DRV_LVL_OV							(0x7c)
#define AW862X_REG_NUM_F0_1								(0x7d)
#define AW862X_REG_NUM_F0_2								(0x7e)
#define AW862X_REG_NUM_F0_3								(0x7f)

/********************************************
 * AW8623X Register List
 *******************************************/
#define AW8623X_REG_RSTCFG								(0x00)
#define AW8623X_REG_SYSST								(0x01)
#define AW8623X_REG_SYSINT								(0x02)
#define AW8623X_REG_SYSINTM								(0x03)
#define AW8623X_REG_SYSST2								(0x04)
#define AW8623X_REG_SYSER								(0x05)
#define AW8623X_REG_PLAYCFG2							(0x07)
#define AW8623X_REG_PLAYCFG3							(0x08)
#define AW8623X_REG_PLAYCFG4							(0x09)
#define AW8623X_REG_WAVCFG1								(0x0A)
#define AW8623X_REG_WAVCFG2								(0x0B)
#define AW8623X_REG_WAVCFG3								(0x0C)
#define AW8623X_REG_WAVCFG4								(0x0D)
#define AW8623X_REG_WAVCFG5								(0x0E)
#define AW8623X_REG_WAVCFG6								(0x0F)
#define AW8623X_REG_WAVCFG7								(0x10)
#define AW8623X_REG_WAVCFG8								(0x11)
#define AW8623X_REG_WAVCFG9								(0x12)
#define AW8623X_REG_WAVCFG10							(0x13)
#define AW8623X_REG_WAVCFG11							(0x14)
#define AW8623X_REG_WAVCFG12							(0x15)
#define AW8623X_REG_WAVCFG13							(0x16)
#define AW8623X_REG_CONTCFG1							(0x17)
#define AW8623X_REG_CONTCFG2							(0x18)
#define AW8623X_REG_CONTCFG3							(0x19)
#define AW8623X_REG_CONTCFG4							(0x1A)
#define AW8623X_REG_CONTCFG5							(0x1B)
#define AW8623X_REG_CONTCFG6							(0x1C)
#define AW8623X_REG_CONTCFG7							(0x1D)
#define AW8623X_REG_CONTCFG8							(0x1E)
#define AW8623X_REG_CONTCFG9							(0x1F)
#define AW8623X_REG_CONTCFG10							(0x20)
#define AW8623X_REG_CONTCFG11							(0x21)
#define AW8623X_REG_CONTCFG12							(0x22)
#define AW8623X_REG_CONTCFG13							(0x23)
#define AW8623X_REG_CONTRD14							(0x24)
#define AW8623X_REG_CONTRD15							(0x25)
#define AW8623X_REG_CONTRD16							(0x26)
#define AW8623X_REG_CONTRD17							(0x27)
#define AW8623X_REG_CONTRD18							(0x28)
#define AW8623X_REG_CONTRD19							(0x29)
#define AW8623X_REG_CONTRD20							(0x2A)
#define AW8623X_REG_CONTRD21							(0x2B)
#define AW8623X_REG_RTPCFG1								(0x2C)
#define AW8623X_REG_RTPCFG2								(0x2D)
#define AW8623X_REG_RTPCFG3								(0x2E)
#define AW8623X_REG_RTPCFG4								(0x2F)
#define AW8623X_REG_RTPCFG5								(0x30)
#define AW8623X_REG_RTPCFG6								(0x31)
#define AW8623X_REG_RTPDATA								(0x32)
#define AW8623X_REG_TRGCFG1								(0x33)
#define AW8623X_REG_TRGCFG2								(0x34)
#define AW8623X_REG_TRGCFG3								(0x35)
#define AW8623X_REG_TRGCFG4								(0x36)
#define AW8623X_REG_TRGCFG5								(0x37)
#define AW8623X_REG_TRGCFG6								(0x38)
#define AW8623X_REG_TRGCFG7								(0x39)
#define AW8623X_REG_TRGCFG8								(0x3A)
#define AW8623X_REG_GLBCFG2								(0x3C)
#define AW8623X_REG_GLBCFG4								(0x3E)
#define AW8623X_REG_GLBRD5								(0x3F)
#define AW8623X_REG_RAMADDRH							(0x40)
#define AW8623X_REG_RAMADDRL							(0x41)
#define AW8623X_REG_RAMDATA								(0x42)
#define AW8623X_REG_TRGCFG9								(0x43)
#define AW8623X_REG_I2SCFG1								(0x44)
#define AW8623X_REG_SYSCTRL1							(0x45)
#define AW8623X_REG_SYSCTRL2							(0x46)
#define AW8623X_REG_SYSCTRL3							(0x47)
#define AW8623X_REG_SYSCTRL4							(0x48)
#define AW8623X_REG_SYSCTRL5							(0x49)
#define AW8623X_REG_PWMCFG1								(0x4A)
#define AW8623X_REG_PWMCFG3								(0x4C)
#define AW8623X_REG_PWMCFG4								(0x4D)
#define AW8623X_REG_VBATCTRL							(0x4E)
#define AW8623X_REG_DETCFG1								(0x4F)
#define AW8623X_REG_DETCFG2								(0x50)
#define AW8623X_REG_DETRD1								(0x51)
#define AW8623X_REG_DETRD2								(0x52)
#define AW8623X_REG_DETRD3								(0x53)
#define AW8623X_REG_IDH									(0x57)
#define AW8623X_REG_IDL									(0x58)
#define AW8623X_REG_TRIMCFG1							(0x59)
#define AW8623X_REG_TRIMCFG2							(0x5A)
#define AW8623X_REG_TRIMCFG3							(0x5B)
#define AW8623X_REG_AUTOSIN1							(0x5C)
#define AW8623X_REG_AUTOSIN2							(0x5D)
#define AW8623X_REG_TRIMCFG5							(0x5E)
#define AW8623X_REG_EFCFG6								(0x64)
#define AW8623X_REG_CHIPIDL								(0x69)
#define AW8623X_REG_EFCFG12								(0x6A)
#define AW8623X_REG_ANACFG3								(0x70)
#define AW8623X_REG_ANACFG4								(0x71)
#define AW8623X_REG_ANACFG5								(0x72)
#define AW8623X_REG_ANACFG10							(0x77)
/********************************************
 * AW8624X Register List
 *******************************************/
#define AW8624X_REG_ID									(0x00)
#define AW8624X_REG_SYSST								(0x01)
#define AW8624X_REG_SYSINT								(0x02)
#define AW8624X_REG_SYSINTM								(0x03)
#define AW8624X_REG_SYSST2								(0x04)
#define AW8624X_REG_SYSER								(0x05)
#define AW8624X_REG_PLAYCFG2							(0x07)
#define AW8624X_REG_PLAYCFG3							(0x08)
#define AW8624X_REG_PLAYCFG4							(0x09)
#define AW8624X_REG_WAVCFG1								(0x0A)
#define AW8624X_REG_WAVCFG2								(0x0B)
#define AW8624X_REG_WAVCFG3								(0x0C)
#define AW8624X_REG_WAVCFG4								(0x0D)
#define AW8624X_REG_WAVCFG5								(0x0E)
#define AW8624X_REG_WAVCFG6								(0x0F)
#define AW8624X_REG_WAVCFG7								(0x10)
#define AW8624X_REG_WAVCFG8								(0x11)
#define AW8624X_REG_WAVCFG9								(0x12)
#define AW8624X_REG_WAVCFG10							(0x13)
#define AW8624X_REG_WAVCFG11							(0x14)
#define AW8624X_REG_WAVCFG12							(0x15)
#define AW8624X_REG_WAVCFG13							(0x16)
#define AW8624X_REG_CONTCFG1							(0x18)
#define AW8624X_REG_CONTCFG2							(0x19)
#define AW8624X_REG_CONTCFG3							(0x1A)
#define AW8624X_REG_CONTCFG4							(0x1B)
#define AW8624X_REG_CONTCFG5							(0x1C)
#define AW8624X_REG_CONTCFG6							(0x1D)
#define AW8624X_REG_CONTCFG7							(0x1E)
#define AW8624X_REG_CONTCFG8							(0x1F)
#define AW8624X_REG_CONTCFG9							(0x20)
#define AW8624X_REG_CONTCFG10							(0x21)
#define AW8624X_REG_CONTCFG11							(0x22)
#define AW8624X_REG_CONTCFG12							(0x23)
#define AW8624X_REG_CONTCFG13							(0x24)
#define AW8624X_REG_CONTRD14							(0x25)
#define AW8624X_REG_CONTRD15							(0x26)
#define AW8624X_REG_CONTRD16							(0x27)
#define AW8624X_REG_CONTRD17							(0x28)
#define AW8624X_REG_CONTRD18							(0x29)
#define AW8624X_REG_CONTRD19							(0x2A)
#define AW8624X_REG_CONTRD20							(0x2B)
#define AW8624X_REG_CONTRD21							(0x2C)
#define AW8624X_REG_RTPCFG1								(0x2D)
#define AW8624X_REG_RTPCFG2								(0x2E)
#define AW8624X_REG_RTPCFG3								(0x2F)
#define AW8624X_REG_RTPCFG4								(0x30)
#define AW8624X_REG_RTPCFG5								(0x31)
#define AW8624X_REG_RTPDATA								(0x32)
#define AW8624X_REG_TRGCFG1								(0x33)
#define AW8624X_REG_TRGCFG2								(0x34)
#define AW8624X_REG_TRGCFG3								(0x35)
#define AW8624X_REG_TRGCFG4								(0x36)
#define AW8624X_REG_TRGCFG5								(0x37)
#define AW8624X_REG_TRGCFG6								(0x38)
#define AW8624X_REG_TRGCFG7								(0x39)
#define AW8624X_REG_TRGCFG8								(0x3A)
#define AW8624X_REG_GLBCFG2								(0x3C)
#define AW8624X_REG_GLBCFG4								(0x3E)
#define AW8624X_REG_GLBRD5								(0x3F)
#define AW8624X_REG_RAMADDRH							(0x40)
#define AW8624X_REG_RAMADDRL							(0x41)
#define AW8624X_REG_RAMDATA								(0x42)
#define AW8624X_REG_SYSCTRL1							(0x43)
#define AW8624X_REG_SYSCTRL2							(0x44)
#define AW8624X_REG_SYSCTRL3							(0x45)
#define AW8624X_REG_SYSCTRL4							(0x46)
#define AW8624X_REG_SYSCTRL5							(0x49)
#define AW8624X_REG_SYSCTRL6							(0x4A)
#define AW8624X_REG_PWMCFG1								(0x4C)
#define AW8624X_REG_PWMCFG3								(0x4E)
#define AW8624X_REG_PWMCFG4								(0x4F)
#define AW8624X_REG_DET_LO								(0x50)
#define AW8624X_REG_DETCFG1								(0x51)
#define AW8624X_REG_DETCFG2								(0x52)
#define AW8624X_REG_DET_RL								(0x53)
#define AW8624X_REG_DET_OS								(0x54)
#define AW8624X_REG_DET_VBAT							(0x55)
#define AW8624X_REG_CHIPIDH								(0x57)
#define AW8624X_REG_CHIPIDL								(0x58)
#define AW8624X_REG_TRIMCFG1							(0x59)
#define AW8624X_REG_TRIMCFG3							(0x5A)
#define AW8624X_REG_EFRD9								(0x64)
#define AW8624X_REG_ANACFG8								(0x77)
/******************************************************
 * Common Register Detail
 *****************************************************/
#define AW_BIT_RESET									(0xAA)
/* GLB_STATE */
#define AW_BIT_GLBRD_STATE_MASK							(15<<0)
#define AW_BIT_STATE_STANDBY							(0<<0)
#define AW_BIT_STATE_WAKEUP								(1<<0)
#define AW_BIT_STATE_STARTUP							(2<<0)
#define AW_BIT_STATE_WAIT								(3<<0)
#define AW_BIT_STATE_CONT_GO							(6<<0)
#define AW_BIT_STATE_RAM_GO								(7<<0)
#define AW_BIT_STATE_RTP_GO								(8<<0)
#define AW_BIT_STATE_TRIG_GO							(9<<0)
#define AW_BIT_STATE_BRAKE								(11<<0)
#define AW_BIT_STATE_END								(12<<0)
/******************************************************
 * AW862XX Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW862XX_BIT_SYSST_UVLS							(1<<5)
#define AW862XX_BIT_SYSST_FF_AES						(1<<4)
#define AW862XX_BIT_SYSST_FF_AFS						(1<<3)
#define AW862XX_BIT_SYSST_OCDS							(1<<2)
#define AW862XX_BIT_SYSST_OTS							(1<<1)
#define AW862XX_BIT_SYSST_DONES							(1<<0)

/* SYSINT: reg 0x02 RC */
#define AW862XX_BIT_SYSINT_UVLI							(1<<5)
#define AW862XX_BIT_SYSINT_FF_AEI						(1<<4)
#define AW862XX_BIT_SYSINT_FF_AFI						(1<<3)
#define AW862XX_BIT_SYSINT_OCDI							(1<<2)
#define AW862XX_BIT_SYSINT_OTI							(1<<1)
#define AW862XX_BIT_SYSINT_DONEI						(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW862XX_BIT_SYSINTM_UVLM_MASK					(~(1<<5))
#define AW862XX_BIT_SYSINTM_UVLM_OFF					(1<<5)
#define AW862XX_BIT_SYSINTM_UVLM_ON						(0<<5)
#define AW862XX_BIT_SYSINTM_FF_AEM_MASK					(~(1<<4))
#define AW862XX_BIT_SYSINTM_FF_AEM_OFF					(1<<4)
#define AW862XX_BIT_SYSINTM_FF_AEM_ON					(0<<4)
#define AW862XX_BIT_SYSINTM_FF_AFM_MASK					(~(1<<3))
#define AW862XX_BIT_SYSINTM_FF_AFM_OFF					(1<<3)
#define AW862XX_BIT_SYSINTM_FF_AFM_ON					(0<<3)
#define AW862XX_BIT_SYSINTM_OCDM_MASK					(~(1<<2))
#define AW862XX_BIT_SYSINTM_OCDM_OFF					(1<<2)
#define AW862XX_BIT_SYSINTM_OCDM_ON						(0<<2)
#define AW862XX_BIT_SYSINTM_OTM_MASK					(~(1<<1))
#define AW862XX_BIT_SYSINTM_OTM_OFF						(1<<1)
#define AW862XX_BIT_SYSINTM_OTM_ON						(0<<1)
#define AW862XX_BIT_SYSINTM_DONEM_MASK					(~(1<<0))
#define AW862XX_BIT_SYSINTM_DONEM_OFF					(1<<0)
#define AW862XX_BIT_SYSINTM_DONEM_ON					(0<<0)

/* SYSST2: reg 0x04 RO */
#define AW862XX_BIT_SYSST2_RAM_ADDR_ER					(1<<7)
#define AW862XX_BIT_SYSST2_TRG_ADDR_ER					(1<<6)
#define AW862XX_BIT_SYSST2_VBG_OK						(1<<3)
#define AW862XX_BIT_SYSST2_LDO_OK						(1<<2)
#define AW862XX_BIT_SYSST2_FF_FULL						(1<<1)
#define AW862XX_BIT_SYSST2_FF_EMPTY						(1<<0)

/* SYSER: reg 0x05 RC */
#define AW862XX_BIT_SYSER_I2S_ERR						(1<<7)
#define AW862XX_BIT_SYSER_TRIG1_EVENT					(1<<6)
#define AW862XX_BIT_SYSER_TRIG2_EVENT					(1<<5)
#define AW862XX_BIT_SYSER_TRIG3_EVENT					(1<<4)
#define AW862XX_BIT_SYSER_OV							(1<<3)
#define AW862XX_BIT_SYSER_ADDR_ER						(1<<2)
#define AW862XX_BIT_SYSER_FF_ER							(1<<1)
#define AW862XX_BIT_SYSER_PLL_REF_ER					(1<<0)

/* PLAYCFG3: reg 0x08 RW */
#define AW862XX_BIT_PLAYCFG3_STOP_MODE_MASK				(~(1<<5))
#define AW862XX_BIT_PLAYCFG3_STOP_MODE_NOW				(1<<5)
#define AW862XX_BIT_PLAYCFG3_STOP_MODE_LATER			(0<<5)
#define AW862XX_BIT_PLAYCFG3_BRK_EN_MASK				(~(1<<2))
#define AW862XX_BIT_PLAYCFG3_BRK						(1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_ENABLE					(1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_DISABLE				(0<<2)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK				(~(3<<0))
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP				(3<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT				(2<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RTP				(1<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM				(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_BIT_PLAYCFG4_STOP_MASK					(~(1<<1))
#define AW862XX_BIT_PLAYCFG4_STOP_ON					(1<<1)
#define AW862XX_BIT_PLAYCFG4_STOP_OFF					(0<<1)
#define AW862XX_BIT_PLAYCFG4_GO_MASK					(~(1<<0))
#define AW862XX_BIT_PLAYCFG4_GO_ON						(1<<0)
#define AW862XX_BIT_PLAYCFG4_GO_OFF						(0<<0)

/* WAVCFG1-8: reg 0x0A - reg 0x11 RW */
#define AW862XX_BIT_WAVCFG_SEQWAIT_MASK					(~(1<<7))
#define AW862XX_BIT_WAVCFG_SEQWAIT_TIME					(1<<7)
#define AW862XX_BIT_WAVCFG_SEQWAIT_NUMBER				(0<<7)
#define AW862XX_BIT_WAVCFG_SEQ							(0x7F)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW862XX_BIT_WAVLOOP_SEQ_ODD_MASK				(~(0x0F<<4))
#define AW862XX_BIT_WAVLOOP_SEQ_ODD_INIFINITELY				(0x0F<<4)
#define AW862XX_BIT_WAVLOOP_SEQ_EVEN_MASK				(~(0x0F<<0))
#define AW862XX_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY			(0x0F<<0)
#define AW862XX_BIT_WAVLOOP_INIFINITELY					(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW862XX_BIT_WAVCFG9_SEQ1LOOP_MASK				(~(0x0F<<4))
#define AW862XX_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY			(0x0F<<4)
#define AW862XX_BIT_WAVCFG9_SEQ2LOOP_MASK				(~(0x0F<<0))
#define AW862XX_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY			(0x0F<<0)

/* WAVCFG10: reg 0x13 RW */
#define AW862XX_BIT_WAVCFG10_SEQ3LOOP_MASK				(~(0x0F<<4))
#define AW862XX_BIT_WAVCFG10_SEQ3LOOP_INIFINITELY		(0x0F<<4)
#define AW862XX_BIT_WAVCFG10_SEQ4LOOP_MASK				(~(0x0F<<0))
#define AW862XX_BIT_WAVCFG10_SEQ4LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG11: reg 0x14 RW */
#define AW862XX_BIT_WAVCFG11_SEQ5LOOP_MASK				(~(0x0F<<4))
#define AW862XX_BIT_WAVCFG11_SEQ5LOOP_INIFINITELY		(0x0F<<4)
#define AW862XX_BIT_WAVCFG11_SEQ6LOOP_MASK				(~(0x0F<<0))
#define AW862XX_BIT_WAVCFG11_SEQ6LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG12: reg 0x15 RW */
#define AW862XX_BIT_WAVCFG12_SEQ7LOOP_MASK				(~(0x0F<<4))
#define AW862XX_BIT_WAVCFG12_SEQ7LOOP_INIFINITELY		(0x0F<<4)
#define AW862XX_BIT_WAVCFG12_SEQ8LOOP_MASK				(~(0x0F<<0))
#define AW862XX_BIT_WAVCFG12_SEQ8LOOP_INIFINITELY		(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x18 RW */
#define AW862XX_BIT_CONTCFG1_EDGE_FRE_MASK				(~(0x0F<<4))
#define AW862XX_BIT_CONTCFG1_EN_F0_DET_MASK				(~(1<<3))
#define AW862XX_BIT_CONTCFG1_F0_DET_ENABLE				(1<<3)
#define AW862XX_BIT_CONTCFG1_F0_DET_DISABLE				(0<<3)
#define AW862XX_BIT_CONTCFG1_SIN_MODE_MASK				(~(1<<0))
#define AW862XX_BIT_CONTCFG1_SIN_MODE_COS				(1<<0)
#define AW862XX_BIT_CONTCFG1_SIN_MODE_SINE				(0<<0)
/* CONTCFG3: reg 0x1A RW */

/* CONTCFG5: reg 0x1C RW */
#define AW862XX_BIT_CONTCFG5_BRK_GAIN_MASK				(~(0x0F<<0))

/* CONTCFG6: reg 0x1D RW */
#define AW862XX_BIT_CONTCFG6_TRACK_EN_MASK				(~(1<<7))
#define AW862XX_BIT_CONTCFG6_TRACK_ENABLE				(1<<7)
#define AW862XX_BIT_CONTCFG6_TRACK_DISABLE				(0<<7)
#define AW862XX_BIT_CONTCFG6_DRV1_LVL_MASK				(~(0x7F<<0))

/* CONTCFG7: reg 0x1E RW */
#define AW862XX_BIT_CONTCFG7_DRV2_LVL_MASK				(~(0x7F<<0))

/* CONTCFG13: reg 0x24 RW */
#define AW862XX_BIT_CONTCFG13_TSET_MASK					(~(0x0F<<4))
#define AW862XX_BIT_CONTCFG13_BEME_SET_MASK				(~(0x0F<<0))

/***************** RTP *****************/
/* RTPCFG1: reg 0x2D RW */
#define AW862XX_BIT_RTPCFG1_ADDRH_MASK					(~(0x0F<<0))
#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_2K_MASK			(~(1<<5))
#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_2K_EN				(1<<5)
#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_2K_DIS			(0<<5)

#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_1K_MASK			(~(1<<4))
#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_1K_EN				(1<<4)
#define AW862XX_BIT_RTPCFG1_SRAM_SIZE_1K_DIS			(0<<4)

/* RTPCFG3: reg 0x2F RW */
#define AW862XX_BIT_RTPCFG3_FIFO_AEH_MASK				(~(0x0F<<4))
#define AW862XX_BIT_RTPCFG3_FIFO_AFH_MASK				(~(0x0F<<0))
#define AW862XX_BIT_RTPCFG3_FIFO_AEH					(0x0F<<4)
#define AW862XX_BIT_RTPCFG3_FIFO_AFH					(0x0F<<0)

/***************** TRIGGER *****************/
#define AW862XX_BIT_TRG_ENABLE_MASK						(~(1<<7))
#define AW862XX_BIT_TRG_ENABLE							(1<<7)
#define AW862XX_BIT_TRG_DISABLE							(0<<7)
#define AW862XX_BIT_TRG_SEQ_MASK						(~(0x7F<<0))

/* TRGCFG1: reg 0x33 RW */
#define AW862XX_BIT_TRGCFG1_TRG1_POS_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG1_TRG1_POS_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG1_TRG1_POS_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG1_TRG1SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG2: reg 0x34 RW */
#define AW862XX_BIT_TRGCFG2_TRG2_POS_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG2_TRG2_POS_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG2_TRG2_POS_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG2_TRG2SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG3: reg 0x35 RW */
#define AW862XX_BIT_TRGCFG3_TRG3_POS_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG3_TRG3_POS_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG3_TRG3_POS_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG3_TRG3SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG4: reg 0x36 RW */
#define AW862XX_BIT_TRGCFG4_TRG1_NEG_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG4_TRG1_NEG_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG4_TRG1_NEG_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG4_TRG1SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG5: reg 0x37 RW */
#define AW862XX_BIT_TRGCFG5_TRG2_NEG_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG5_TRG2_NEG_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG5_TRG2_NEG_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG5_TRG2SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG6: reg 0x38 RW */
#define AW862XX_BIT_TRGCFG6_TRG3_NEG_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG6_TRG3_NEG_ENABLE				(1<<7)
#define AW862XX_BIT_TRGCFG6_TRG3_NEG_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG6_TRG3SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG7: reg 0x39 RW */
#define AW862XX_BIT_TRGCFG7_TRG1_POR_LEV_BRK_MASK		(~(7<<5))
#define AW862XX_BIT_TRGCFG7_TRG2_POR_LEV_BRK_MASK		(~(7<<1))
#define AW862XX_BIT_TRGCFG7_TRG1_POLAR_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG7_TRG1_POLAR_NEG				(1<<7)
#define AW862XX_BIT_TRGCFG7_TRG1_POLAR_POS				(0<<7)
#define AW862XX_BIT_TRGCFG7_TRG1_MODE_MASK				(~(1<<6))
#define AW862XX_BIT_TRGCFG7_TRG1_MODE_LEVEL				(1<<6)
#define AW862XX_BIT_TRGCFG7_TRG1_MODE_EDGE				(0<<6)
#define AW862XX_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK			(~(1<<5))
#define AW862XX_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE		(1<<5)
#define AW862XX_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE		(0<<5)
#define AW862XX_BIT_TRGCFG7_TRG2_POLAR_MASK				(~(1<<3))
#define AW862XX_BIT_TRGCFG7_TRG2_POLAR_NEG				(1<<3)
#define AW862XX_BIT_TRGCFG7_TRG2_POLAR_POS				(0<<3)
#define AW862XX_BIT_TRGCFG7_TRG2_MODE_MASK				(~(1<<2))
#define AW862XX_BIT_TRGCFG7_TRG2_MODE_LEVEL				(1<<2)
#define AW862XX_BIT_TRGCFG7_TRG2_MODE_EDGE				(0<<2)
#define AW862XX_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK			(~(1<<1))
#define AW862XX_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE		(1<<1)
#define AW862XX_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE		(0<<1)

/* TRGCFG8: reg 0x3A RW */
#define AW862XX_BIT_TRGCFG8_TRG3_POR_LEV_BRK_MASK		(~(7<<5))
#define AW862XX_BIT_TRGCFG8_TRG3_POLAR_MASK				(~(1<<7))
#define AW862XX_BIT_TRGCFG8_TRG3_POLAR_NEG				(1<<7)
#define AW862XX_BIT_TRGCFG8_TRG3_POLAR_POS				(0<<7)
#define AW862XX_BIT_TRGCFG8_TRG3_MODE_MASK				(~(1<<6))
#define AW862XX_BIT_TRGCFG8_TRG3_MODE_LEVEL				(1<<6)
#define AW862XX_BIT_TRGCFG8_TRG3_MODE_EDGE				(0<<6)
#define AW862XX_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK			(~(1<<5))
#define AW862XX_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE		(1<<5)
#define AW862XX_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE		(0<<5)
#define AW862XX_BIT_TRGCFG8_TRG_TRIG1_MODE_MASK			(~(3<<3))
#define AW862XX_BIT_TRGCFG8_PWM_LRA						(0<<3)
#define AW862XX_BIT_TRGCFG8_PWM_ERA						(1<<3)
#define AW862XX_BIT_TRGCFG8_TRIG1						(2<<3)
#define AW862XX_BIT_TRGCFG8_DISABLE						(3<<3)
#define AW862XX_BIT_TRGCFG8_TRG1_STOP_MASK				(~(1<<2))
#define AW862XX_BIT_TRGCFG8_TRG1_STOP					(1<<2)
#define AW862XX_BIT_TRGCFG8_TRG2_STOP_MASK				(~(1<<1))
#define AW862XX_BIT_TRGCFG8_TRG2_STOP					(1<<1)
#define AW862XX_BIT_TRGCFG8_TRG3_STOP_MASK				(~(1<<0))
#define AW862XX_BIT_TRGCFG8_TRG3_STOP					(1<<0)

/* GLBCFG2: reg 0x3C RW */
/* START_DLY */
#define AW862XX_BIT_GLBCFG2_START_DLY_250US				(0x0C)

/* GLBCFG4: reg 0x3E RW */
#define AW862XX_BIT_GLBCFG4_GO_PRIO_MASK				(~(3<<6))
#define AW862XX_BIT_GLBCFG4_TRG3_PRIO_MASK				(~(3<<4))
#define AW862XX_BIT_GLBCFG4_TRG2_PRIO_MASK				(~(3<<2))
#define AW862XX_BIT_GLBCFG4_TRG1_PRIO_MASK				(~(3<<0))

/* GLBRD5: reg 0x3F R0 */
/* GLB_STATE */
#define AW862XX_BIT_GLBRD5_STATE						(15<<0)
#define AW862XX_BIT_GLBRD5_STATE_STANDBY				(0<<0)
#define AW862XX_BIT_GLBRD5_STATE_WAKEUP					(1<<0)
#define AW862XX_BIT_GLBRD5_STATE_STARTUP				(2<<0)
#define AW862XX_BIT_GLBRD5_STATE_WAIT					(3<<0)
#define AW862XX_BIT_GLBRD5_STATE_CONT_GO				(6<<0)
#define AW862XX_BIT_GLBRD5_STATE_RAM_GO					(7<<0)
#define AW862XX_BIT_GLBRD5_STATE_RTP_GO					(8<<0)
#define AW862XX_BIT_GLBRD5_STATE_TRIG_GO				(9<<0)
#define AW862XX_BIT_GLBRD5_STATE_I2S_GO					(10<<0)
#define AW862XX_BIT_GLBRD5_STATE_BRAKE					(11<<0)
#define AW862XX_BIT_GLBRD5_STATE_END					(12<<0)
/* RAMADDRH: reg 0x40 RWS */
#define AW862XX_BIT_RAMADDRH_MASK						(~(63<<0))

/***************** SYSCTRL *****************/
/* SYSCTRL1: reg 0x43 RW */
#define AW862XX_BIT_SYSCTRL1_VBAT_MODE_MASK				(~(1<<7))
#define AW862XX_BIT_SYSCTRL1_VBAT_MODE_HW				(1<<7)
#define AW862XX_BIT_SYSCTRL1_VBAT_MODE_SW				(0<<7)
#define AW862XX_BIT_SYSCTRL1_PERP_MASK					(~(1<<6))
#define AW862XX_BIT_SYSCTRL1_PERP_ON					(1<<6)
#define AW862XX_BIT_SYSCTRL1_PERP_OFF					(0<<6)
#define AW862XX_BIT_SYSCTRL1_CLK_SEL_MASK				(~(3<<4))
#define AW862XX_BIT_SYSCTRL1_CLK_SEL_OSC				(1<<4)
#define AW862XX_BIT_SYSCTRL1_CLK_SEL_AUTO				(0<<4)
#define AW862XX_BIT_SYSCTRL1_RAMINIT_MASK				(~(1<<3))
#define AW862XX_BIT_SYSCTRL1_RAMINIT_ON					(1<<3)
#define AW862XX_BIT_SYSCTRL1_RAMINIT_OFF				(0<<3)
#define AW862XX_BIT_SYSCTRL1_EN_FIR_MASK				(~(1<<2))
#define AW862XX_BIT_SYSCTRL1_FIR_ENABLE					(0<<2)
#define AW862XX_BIT_SYSCTRL1_WAKE_MODE_MASK				(~(1<<1))
#define AW862XX_BIT_SYSCTRL1_WAKE_MODE_WAKEUP			(1<<1)
#define AW862XX_BIT_SYSCTRL1_WAKE_MODE_BST				(0<<1)
#define AW862XX_BIT_SYSCTRL1_RTP_CLK_MASK				(~(1<<0))
#define AW862XX_BIT_SYSCTRL1_RTP_PLL					(1<<0)
#define AW862XX_BIT_SYSCTRL1_RTP_OSC					(0<<0)

/* SYSCTRL2: reg 0x44 RW */
#define AW862XX_BIT_SYSCTRL2_WAKE_MASK					(~(1<<7))
#define AW862XX_BIT_SYSCTRL2_WAKE_ON					(1<<7)
#define AW862XX_BIT_SYSCTRL2_WAKE_OFF					(0<<7)
#define AW862XX_BIT_SYSCTRL2_STANDBY_MASK				(~(1<<6))
#define AW862XX_BIT_SYSCTRL2_STANDBY_ON					(1<<6)
#define AW862XX_BIT_SYSCTRL2_STANDBY_OFF				(0<<6)
#define AW862XX_BIT_SYSCTRL2_RTP_DLY_MASK				(~(3<<4))
#define AW862XX_BIT_SYSCTRL2_INTN_PIN_MASK				(~(1<<3))
#define AW862XX_BIT_SYSCTRL2_INTN						(1<<3)
#define AW862XX_BIT_SYSCTRL2_TRIG1						(0<<3)
#define AW862XX_BIT_SYSCTRL2_WCK_PIN_MASK				(~(1<<2))
#define AW862XX_BIT_SYSCTRL2_ENABLE_TRIG2				(1<<2)
#define AW862XX_BIT_SYSCTRL2_DISENABLE_TRIG2			(0<<2)
#define AW862XX_BIT_SYSCTRL2_WAVDAT_MODE_MASK			(~(3<<0))
#define AW862XX_BIT_SYSCTRL2_RATE						(3<<0)
#define AW862XX_BIT_SYSCTRL2_RATE_12K					(2<<0)
#define AW862XX_BIT_SYSCTRL2_RATE_24K					(0<<0)
#define AW862XX_BIT_SYSCTRL2_RATE_48K					(1<<0)

/* SYSCTRL7: reg 0x49 RW */
#define AW862XX_BIT_SYSCTRL7_GAIN_BYPASS_MASK			(~(1<<6))
#define AW862XX_BIT_SYSCTRL7_GAIN_CHANGEABLE			(1<<6)
#define AW862XX_BIT_SYSCTRL7_GAIN_FIXED					(0<<6)
#define AW862XX_BIT_SYSCTRL7_GAIN						(0x07)

#define AW862XX_BIT_SYSCTRL7_INT_EDGE_MODE_MASK			(~(1<<5))
#define AW862XX_BIT_SYSCTRL7_INT_EDGE_MODE_POS			(0<<5)
#define AW862XX_BIT_SYSCTRL7_INT_EDGE_MODE_BOTH			(1<<5)
#define AW862XX_BIT_SYSCTRL7_INT_MODE_MASK				(~(1<<4))
#define AW862XX_BIT_SYSCTRL7_INT_MODE_EDGE				(1<<4)
#define AW862XX_BIT_SYSCTRL7_INT_MODE_LEVEL				(0<<4)

#define AW862XX_BIT_SYSCTRL7_INTP_MASK					(~(1<<3))
#define AW862XX_BIT_SYSCTRL7_INTP_HIGH					(1<<3)
#define AW862XX_BIT_SYSCTRL7_INTP_LOW					(0<<3)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_MASK				(~(7<<0))
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_1					(0<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_2					(1<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_4					(2<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_5					(3<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_8					(4<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_10				(5<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_20				(6<<0)
#define AW862XX_BIT_SYSCTRL7_D2S_GAIN_40				(7<<0)

/* PWMCFG1: reg 0x4C RW */
#define AW862XX_BIT_PWMCFG1_PRC_EN_MASK					(~(1<<7))
#define AW862XX_BIT_PWMCFG1_PRC_ENABLE					(1<<7)
#define AW862XX_BIT_PWMCFG1_PRC_DISABLE					(0<<7)
#define AW862XX_BIT_PWMCFG1_PRCTIME_MASK				(~(0x7F<<0))
#define AW862XX_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE			(0x3F)

/* PWMCFG4: reg 0x4F RW */
/* PRTIME */
#define AW862XX_PWMCFG4_PRTIME_DEFAULT_VALUE			(0x32)

/* PWMCFG2: reg 0x4D RW */
#define AW862XX_BIT_PWMCFG2_REF_SEL_MASK				(~(1<<5))
#define AW862XX_BIT_PWMCFG2_REF_SEL_TRIANGLE			(1<<5)
#define AW862XX_BIT_PWMCFG2_REF_SEL_SAWTOOTH			(0<<5)
#define AW862XX_BIT_PWMCFG2_PD_HWM_MASK					(~(1<<4))
#define AW862XX_BIT_PWMCFG2_PD_HWM_ON					(1<<4)
#define AW862XX_BIT_PWMCFG2_PWMOE_MASK					(~(1<<3))
#define AW862XX_BIT_PWMCFG2_PWMOE_ON					(1<<3)
#define AW862XX_BIT_PWMCFG2_PWMFRC_MASK					(~(7<<0))

/* PWMCFG3: reg 0x4E RW */
#define AW862XX_BIT_PWMCFG3_PR_EN_MASK					(~(1<<7))
#define AW862XX_BIT_PWMCFG3_PR_ENABLE					(1<<7)
#define AW862XX_BIT_PWMCFG3_PR_DISABLE					(0<<7)
#define AW862XX_BIT_PWMCFG3_PRLVL_MASK					(~(0x7F<<0))

/* DETCFG1: reg 0x51 RW */
#define AW862XX_BIT_DETCFG1_FTS_GO_MASK					(~(1<<7))
#define AW862XX_BIT_DETCFG1_FTS_GO_ENABLE				(1<<7)
#define AW862XX_BIT_DETCFG1_TEST_GO_MASK				(~(1<<6))
#define AW862XX_BIT_DETCFG1_TEST_GO_ENABLE				(1<<6)
#define AW862XX_BIT_DETCFG1_ADO_SLOT_MODE_MASK			(~(1<<5))
#define AW862XX_BIT_DETCFG1_ADO_SLOT_ADC_32				(1<<5)
#define AW862XX_BIT_DETCFG1_ADO_SLOT_ADC_256			(0<<5)
#define AW862XX_BIT_DETCFG1_RL_OS_MASK					(~(1<<4))
#define AW862XX_BIT_DETCFG1_RL							(1<<4)
#define AW862XX_BIT_DETCFG1_OS							(0<<4)
#define AW862XX_BIT_DETCFG1_PRCT_MODE_MASK				(~(1<<3))
#define AW862XX_BIT_DETCFG1_PRCT_MODE_INVALID			(1<<3)
#define AW862XX_BIT_DETCFG1_PRCT_MODE_VALID				(0<<3)
#define AW862XX_BIT_DETCFG1_CLK_ADC_MASK				(~(7<<0))
#define AW862XX_BIT_DETCFG1_CLK_ADC_12M					(0<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_6M					(1<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_3M					(2<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_1M5					(3<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_M75					(4<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_M37					(5<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_M18					(6<<0)
#define AW862XX_BIT_DETCFG1_CLK_ADC_M09					(7<<0)

/* DETCFG2: reg 0x52 RW */
#define AW862XX_BIT_DETCFG2_VBAT_GO_MASK				(~(1<<1))
#define AW862XX_BIT_DETCFG2_VABT_GO_ON					(1<<1)
#define AW862XX_BIT_DETCFG2_DIAG_GO_MASK				(~(1<<0))
#define AW862XX_BIT_DETCFG2_DIAG_GO_ON					(1<<0)

/* DET_LO: reg 0x57 RW */
#define AW862XX_BIT_DET_LO_TEST_MASK					(~(3<<6))
#define AW862XX_BIT_DET_LO_VBAT_MASK					(~(3<<4))
#define AW862XX_BIT_DET_LO_VBAT							(3<<4)
#define AW862XX_BIT_DET_LO_OS_MASK						(~(3<<2))
#define AW862XX_BIT_DET_LO_RL_MASK						(~(3<<0))
#define AW862XX_BIT_DET_LO_RL							(3<<0)

/* TRIMCFG1: reg:0x58 RW */
#define AW862XX_BIT_TRIMCFG1_RL_TRIM_SRC_MASK			(~(1<<6))
#define AW862XX_BIT_TRIMCFG1_RL_TRIM_SRC_REG			(1<<6)
#define AW862XX_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE			(0<<6)
#define AW862XX_BIT_TRIMCFG1_TRIM_RL_MASK				(~(63<<0))

/* TRIMCFG3: reg:0x5A RW */
#define AW862XX_BIT_TRIMCFG3_OSC_TRIM_SRC_MASK			(~(1<<7))
#define AW862XX_BIT_TRIMCFG3_OSC_TRIM_SRC_REG			(1<<7)
#define AW862XX_BIT_TRIMCFG3_OSC_TRIM_SRC_EFUSE			(0<<7)
#define AW862XX_BIT_TRIMCFG3_TRIM_LRA_MASK				(~(63<<0))

/* TRIMCFG4: reg:0x5B RW */
/* TRIM_OSC */

/* ANACFG8: reg:0x77 RW */
#define AW862XX_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK			(~(3<<6))
#define AW862XX_BIT_ANACFG8_TRTF_CTRL_HDRV				(3<<6)

/******************************************************
 * AW862X Register Detail
 *****************************************************/
 /* SYSST  0x01 */
#define AW862X_BIT_SYSST_OVS							(1<<6)
#define AW862X_BIT_SYSST_UVLS							(1<<5)
#define AW862X_BIT_SYSST_FF_AES							(1<<4)
#define AW862X_BIT_SYSST_FF_AFS							(1<<3)
#define AW862X_BIT_SYSST_OCDS							(1<<2)
#define AW862X_BIT_SYSST_OTS							(1<<1)
#define AW862X_BIT_SYSST_DONES							(1<<0)

 /* SYSINT  0x02 */
#define AW862X_BIT_SYSINT_OVI							(1<<6)
#define AW862X_BIT_SYSINT_UVLI							(1<<5)
#define AW862X_BIT_SYSINT_FF_AEI						(1<<4)
#define AW862X_BIT_SYSINT_FF_AFI						(1<<3)
#define AW862X_BIT_SYSINT_OCDI							(1<<2)
#define AW862X_BIT_SYSINT_OTI							(1<<1)
#define AW862X_BIT_SYSINT_DONEI							(1<<0)

 /* SYSINTM 0x03 */
#define AW862X_BIT_SYSINTM_OV_MASK						(~(1<<6))
#define AW862X_BIT_SYSINTM_OV_OFF						(1<<6)
#define AW862X_BIT_SYSINTM_OV_EN						(0<<6)
#define AW862X_BIT_SYSINTM_UVLO_MASK					(~(1<<5))
#define AW862X_BIT_SYSINTM_UVLO_OFF						(1<<5)
#define AW862X_BIT_SYSINTM_UVLO_EN						(0<<5)
#define AW862X_BIT_SYSINTM_FF_AE_MASK					(~(1<<4))
#define AW862X_BIT_SYSINTM_FF_AE_OFF					(1<<4)
#define AW862X_BIT_SYSINTM_FF_AE_EN						(0<<4)
#define AW862X_BIT_SYSINTM_FF_AF_MASK					(~(1<<3))
#define AW862X_BIT_SYSINTM_FF_AF_OFF					(1<<3)
#define AW862X_BIT_SYSINTM_FF_AF_EN						(0<<3)
#define AW862X_BIT_SYSINTM_OCD_MASK						(~(1<<2))
#define AW862X_BIT_SYSINTM_OCD_OFF						(1<<2)
#define AW862X_BIT_SYSINTM_OCD_EN						(0<<2)
#define AW862X_BIT_SYSINTM_OT_MASK						(~(1<<1))
#define AW862X_BIT_SYSINTM_OT_OFF						(1<<1)
#define AW862X_BIT_SYSINTM_OT_EN						(0<<1)
#define AW862X_BIT_SYSINTM_DONE_MASK					(~(1<<0))
#define AW862X_BIT_SYSINTM_DONE_OFF						(1<<0)
#define AW862X_BIT_SYSINTM_DONE_EN						(0<<0)

 /* SYSCTRL 0x04 */
#define AW862X_BIT_SYSCTRL_WAVDAT_MODE_MASK				(~(3<<6))
#define AW862X_BIT_SYSCTRL_WAVDAT_MODE_4X				(3<<6)
#define AW862X_BIT_SYSCTRL_WAVDAT_MODE_2X				(0<<6)
#define AW862X_BIT_SYSCTRL_WAVDAT_MODE_1X				(1<<6)
#define AW862X_BIT_SYSCTRL_RAMINIT_MASK					(~(1<<5))
#define AW862X_BIT_SYSCTRL_RAMINIT_EN					(1<<5)
#define AW862X_BIT_SYSCTRL_RAMINIT_OFF					(0<<5)
#define AW862X_BIT_SYSCTRL_PLAY_MODE_MASK				(~(3<<2))
#define AW862X_BIT_SYSCTRL_PLAY_MODE_CONT				(2<<2)
#define AW862X_BIT_SYSCTRL_PLAY_MODE_RTP				(1<<2)
#define AW862X_BIT_SYSCTRL_PLAY_MODE_RAM				(0<<2)
#define AW862X_BIT_SYSCTRL_WORK_MODE_MASK				(~(1<<0))
#define AW862X_BIT_SYSCTRL_STANDBY						(1<<0)
#define AW862X_BIT_SYSCTRL_ACTIVE						(0<<0)

 /* GO 0x05 */
#define AW862X_BIT_GO_MASK								(~(1<<0))
#define AW862X_BIT_GO_ENABLE							(1<<0)
#define AW862X_BIT_GO_DISABLE							(0<<0)

 /* WAVSEQ1 0x07 */
#define AW862X_BIT_WAVSEQ1_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ1_WAV_FRM_SEQ1_MASK			(~(127<<0))

 /* WAVSEQ2 0x08 */
#define AW862X_BIT_WAVSEQ2_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ2_WAV_FRM_SEQ2_MASK			(~(127<<0))

 /* WAVSEQ3 0x09 */
#define AW862X_BIT_WAVSEQ3_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ3_WAV_FRM_SEQ3_MASK			(~(127<<0))

 /* WAVSEQ4 0x0A */
#define AW862X_BIT_WAVSEQ4_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ4_WAV_FRM_SEQ4_MASK			(~(127<<0))

 /* WAVSEQ5 0X0B */
#define AW862X_BIT_WAVSEQ5_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ5_WAV_FRM_SEQ5_MASK			(~(127<<0))

 /* WAVSEQ6 0X0C */
#define AW862X_BIT_WAVSEQ6_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ6_WAV_FRM_SEQ6_MASK			(~(127<<0))

 /* WAVSEQ7 */
#define AW862X_BIT_WAVSEQ7_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ7_WAV_FRM_SEQ7_MASK			(~(127<<0))

 /* WAVSEQ8 */
#define AW862X_BIT_WAVSEQ8_WAIT							(1<<7)
#define AW862X_BIT_WAVSEQ8_WAV_FRM_SEQ8_MASK			(~(127<<0))

 /* WAVLOOP */
#define AW862X_BIT_WAVLOOP_SEQN_MASK					(~(15<<4))
#define AW862X_BIT_WAVLOOP_SEQNP1_MASK					(~(15<<0))
#define AW862X_BIT_WAVLOOP_INIFINITELY					(15<<0)

 /* WAVLOOP1 */
#define AW862X_BIT_WAVLOOP1_SEQ1_MASK					(~(15<<4))
#define AW862X_BIT_WAVLOOP1_SEQ2_MASK					(~(15<<0))

 /* WAVLOOP2 */
#define AW862X_BIT_WAVLOOP2_SEQ3_MASK					(~(15<<4))
#define AW862X_BIT_WAVLOOP2_SEQ4_MASK					(~(15<<0))

 /* WAVLOOP3 */
#define AW862X_BIT_WAVLOOP3_SEQ5_MASK					(~(15<<4))
#define AW862X_BIT_WAVLOOP3_SEQ6_MASK					(~(15<<0))

 /* WAVLOOP4 */
#define AW862X_BIT_WAVLOOP4_SEQ7_MASK					(~(15<<4))
#define AW862X_BIT_WAVLOOP4_SEQ8_MASK					(~(15<<0))

 /* TRGCFG1 */
#define AW862X_BIT_TRGCFG1_TRG1_POLAR_MASK				(~(1<<1))
#define AW862X_BIT_TRGCFG1_TRG1_POLAR_NEG				(1<<1)
#define AW862X_BIT_TRGCFG1_TRG1_POLAR_POS				(0<<1)
#define AW862X_BIT_TRGCFG1_TRG1_EDGE_MASK				(~(1<<0))
#define AW862X_BIT_TRGCFG1_TRG1_EDGE_POS				(1<<0)
#define AW862X_BIT_TRGCFG1_TRG1_EDGE_POS_NEG			(0<<0)

 /* TRGCFG2 */
#define AW862X_BIT_TRGCFG2_TRG1_ENABLE_MASK				(~(1<<0))
#define AW862X_BIT_TRGCFG2_TRG1_ENABLE					(1<<0)
#define AW862X_BIT_TRGCFG2_TRG1_DISABLE					(0<<0)

 /*DBGCTRL 0X20 */
#define AW862X_BIT_DBGCTRL_INTN_TRG_SEL_MASK			(~(1<<5))
#define AW862X_BIT_DBGCTRL_INTN_SEL_ENABLE				(1<<5)
#define AW862X_BIT_DBGCTRL_TRG_SEL_ENABLE				(0<<5)
#define AW862X_BIT_DBGCTRL_INT_MODE_MASK				(~(3<<2))
#define AW862X_BIT_DBGCTRL_INTN_LEVEL_MODE				(0<<2)
#define AW862X_BIT_DBGCTRL_INT_MODE_EDGE				(1<<2)
#define AW862X_BIT_DBGCTRL_INTN_POSEDGE_MODE			(2<<2)
#define AW862X_BIT_DBGCTRL_INTN_BOTH_EDGE_MODE			(3<<2)

 /* DATCTRL */
#define AW862X_BIT_DATCTRL_FC_MASK						(~(3<<6))
#define AW862X_BIT_DATCTRL_FC_1000HZ					(3<<6)
#define AW862X_BIT_DATCTRL_FC_800HZ						(3<<6)
#define AW862X_BIT_DATCTRL_FC_600HZ						(1<<6)
#define AW862X_BIT_DATCTRL_FC_400HZ						(0<<6)
#define AW862X_BIT_DATCTRL_LPF_ENABLE_MASK				(~(1<<5))
#define AW862X_BIT_DATCTRL_LPF_ENABLE					(1<<5)
#define AW862X_BIT_DATCTRL_LPF_DISABLE					(0<<5)

 /*PWMPRC 0X2D */
#define AW862X_BIT_PWMPRC_PRC_EN_MASK					(~(1<<7))
#define AW862X_BIT_PWMPRC_PRC_ENABLE					(1<<7)
#define AW862X_BIT_PWMPRC_PRC_DISABLE					(0<<7)
#define AW862X_BIT_PWMPRC_PRCTIME_MASK					(~(0x7f<<0))

 /* PWMDBG */
#define AW862X_BIT_PWMDBG_PWM_MODE_MASK					(~(3<<5))
#define AW862X_BIT_PWMDBG_PWM_12K						(3<<5)
#define AW862X_BIT_PWMDBG_PWM_24K						(2<<5)
#define AW862X_BIT_PWMDBG_PWM_48K						(0<<5)

/* GLB_STATE 0x47*/
#define AW862X_BIT_GLBRD5_STATE_MASK					(~(15<<0))
#define AW862X_BIT_GLBRD5_STATE							(15<<0)
#define AW862X_BIT_GLBRD5_STATE_STANDBY					(0<<0)
#define AW862X_BIT_GLBRD5_STATE_WAKEUP					(1<<0)
#define AW862X_BIT_GLBRD5_STATE_STARTUP					(2<<0)
#define AW862X_BIT_GLBRD5_STATE_WAIT					(3<<0)
#define AW862X_BIT_GLBRD5_STATE_CONT_GO					(6<<0)
#define AW862X_BIT_GLBRD5_STATE_RAM_GO					(7<<0)
#define AW862X_BIT_GLBRD5_STATE_RTP_GO					(8<<0)
#define AW862X_BIT_GLBRD5_STATE_TRIG_GO					(9<<0)
#define AW862X_BIT_GLBRD5_STATE_I2S_GO					(10<<0)
#define AW862X_BIT_GLBRD5_STATE_BRAKE					(11<<0)
#define AW862X_BIT_GLBRD5_STATE_END						(12<<0)

 /* WAVECTRL */
#define AW862X_BIT_WAVECTRL_NUM_OV_DRIVER_MASK			(~(0xF<<4))
#define AW862X_BIT_WAVECTRL_NUM_OV_DRIVER				(0<<4)

 /* CONT_CTRL */
#define AW862X_BIT_CONT_CTRL_ZC_DETEC_MASK				(~(1<<7))
#define AW862X_BIT_CONT_CTRL_ZC_DETEC_ENABLE			(1<<7)
#define AW862X_BIT_CONT_CTRL_ZC_DETEC_DISABLE			(0<<7)
#define AW862X_BIT_CONT_CTRL_WAIT_PERIOD_MASK			(~(3<<5))
#define AW862X_BIT_CONT_CTRL_WAIT_8PERIOD				(3<<5)
#define AW862X_BIT_CONT_CTRL_WAIT_4PERIOD				(2<<5)
#define AW862X_BIT_CONT_CTRL_WAIT_2PERIOD				(1<<5)
#define AW862X_BIT_CONT_CTRL_WAIT_1PERIOD				(0<<5)
#define AW862X_BIT_CONT_CTRL_MODE_MASK					(~(1<<4))
#define AW862X_BIT_CONT_CTRL_BY_DRV_TIME				(1<<4)
#define AW862X_BIT_CONT_CTRL_BY_GO_SIGNAL				(0<<4)
#define AW862X_BIT_CONT_CTRL_EN_CLOSE_MASK				(~(1<<3))
#define AW862X_BIT_CONT_CTRL_CLOSE_PLAYBACK				(1<<3)
#define AW862X_BIT_CONT_CTRL_OPEN_PLAYBACK				(0<<3)
#define AW862X_BIT_CONT_CTRL_F0_DETECT_MASK				(~(1<<2))
#define AW862X_BIT_CONT_CTRL_F0_DETECT_ENABLE			(1<<2)
#define AW862X_BIT_CONT_CTRL_F0_DETECT_DISABLE			(0<<2)
#define AW862X_BIT_CONT_CTRL_O2C_MASK					(~(1<<1))
#define AW862X_BIT_CONT_CTRL_O2C_ENABLE					(1<<1)
#define AW862X_BIT_CONT_CTRL_O2C_DISABLE				(0<<1)
#define AW862X_BIT_CONT_CTRL_AUTO_BRK_MASK				(~(1<<0))
#define AW862X_BIT_CONT_CTRL_AUTO_BRK_ENABLE			(1<<0)
#define AW862X_BIT_CONT_CTRL_AUTO_BRK_DISABLE			(0<<0)

#define AW862X_BIT_D2SCFG_CLK_ADC_MASK					(~(7<<5))
#define AW862X_BIT_D2SCFG_CLK_ASC_1P5MHZ				(3<<5)

#define AW862X_BIT_D2SCFG_GAIN_MASK						(~(7<<0))
#define AW862X_BIT_D2SCFG_GAIN_40						(7<<0)
 /* DETCTRL */
#define AW862X_BIT_DETCTRL_RL_OS_MASK					(~(1<<6))
#define AW862X_BIT_DETCTRL_RL_DETECT					(1<<6)
#define AW862X_BIT_DETCTRL_OS_DETECT					(0<<6)
#define AW862X_BIT_DETCTRL_PROTECT_MASK					(~(1<<5))
#define AW862X_BIT_DETCTRL_PROTECT_NO_ACTION			(1<<5)
#define AW862X_BIT_DETCTRL_PROTECT_SHUTDOWN				(0<<5)
#define AW862X_BIT_DETCTRL_VBAT_GO_MASK					(~(1<<1))
#define AW862X_BIT_DETCTRL_VABT_GO_ENABLE				(1<<1)
#define AW862X_BIT_DETCTRL_VBAT_GO_DISABLE				(0<<1)
#define AW862X_BIT_DETCTRL_DIAG_GO_MASK					(~(1<<0))
#define AW862X_BIT_DETCTRL_DIAG_GO_ENABLE				(1<<0)
#define AW862X_BIT_DETCTRL_DIAG_GO_DISABLE				(0<<0)


#define AW862X_BIT_RAMADDRH_MASK						(~(63<<0))

 /* VBAT MODE */
#define AW862X_BIT_DETCTRL_VBAT_MODE_MASK				(~(1<<6))
#define AW862X_BIT_DETCTRL_VBAT_HW_COMP					(1<<6)
#define AW862X_BIT_DETCTRL_VBAT_SW_COMP					(0<<6)


 /* ANACTRL */
#define AW862X_BIT_ANACTRL_LRA_SRC_MASK					(~(1<<5))
#define AW862X_BIT_ANACTRL_LRA_SRC_REG					(1<<5)
#define AW862X_BIT_ANACTRL_LRA_SRC_EFUSE				(0<<5)
#define AW862X_BIT_ANACTRL_EN_IO_PD1_MASK				(~(1<<0))
#define AW862X_BIT_ANACTRL_EN_IO_PD1_HIGH				(1<<0)
#define AW862X_BIT_ANACTRL_EN_IO_PD1_LOW				(0<<0)

/* PRLVL */
#define AW862X_BIT_PRLVL_PR_EN_MASK						(~(1<<7))
#define AW862X_BIT_PRLVL_PR_ENABLE						(1<<7)
#define AW862X_BIT_PRLVL_PR_DISABLE						(0<<7)
#define AW862X_BIT_PRLVL_PRLVL_MASK						(~(0x7f<<0))
#define AW862X_BIT_PRLVL_PRLVL_DEFAULT_VALUE			(0x3F)

/* PRTIME */
#define AW862X_REG_PRTIME_DEFAULT_VALUE					(0x12)
#define AW862X_BIT_PRTIME_PRTIME_MASK					(~(0xff<<0))

#define AW862X_BIT_BEMF_NUM_BRK_MASK					(~(0xf<<0))

/* TD_H 0x4b TD_brake */
#define AW862X_BIT_R_SPARE_MASK							(~(1<<7))
#define AW862X_BIT_R_SPARE_ENABLE						(1<<7)

/* TIME_NZC */
#define AW862X_BIT_TIME_NZC_DEF_VAL						(0x1f)

/* TRIM_LRA */
#define AW862X_BIT_TRIM_LRA								(0x3f<<0)
/******************************************************
 * AW8623X Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW8623X_BIT_SYSST_UVLS							(1<<5)
#define AW8623X_BIT_SYSST_FF_AES						(1<<4)
#define AW8623X_BIT_SYSST_FF_AFS						(1<<3)
#define AW8623X_BIT_SYSST_OCDS							(1<<2)
#define AW8623X_BIT_SYSST_OTS							(1<<1)
#define AW8623X_BIT_SYSST_DONES							(1<<0)

/* SYSINT: reg 0x02 RC */
#define AW8623X_BIT_SYSINT_UVLI							(1<<5)
#define AW8623X_BIT_SYSINT_FF_AEI						(1<<4)
#define AW8623X_BIT_SYSINT_FF_AFI						(1<<3)
#define AW8623X_BIT_SYSINT_OCDI							(1<<2)
#define AW8623X_BIT_SYSINT_OTI							(1<<1)
#define AW8623X_BIT_SYSINT_DONEI						(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW8623X_BIT_SYSINTM_UVLM_MASK					(~(1<<5))
#define AW8623X_BIT_SYSINTM_UVLM_OFF					(1<<5)
#define AW8623X_BIT_SYSINTM_UVLM_ON						(0<<5)
#define AW8623X_BIT_SYSINTM_FF_AEM_MASK					(~(1<<4))
#define AW8623X_BIT_SYSINTM_FF_AEM_OFF					(1<<4)
#define AW8623X_BIT_SYSINTM_FF_AEM_ON					(0<<4)
#define AW8623X_BIT_SYSINTM_FF_AFM_MASK					(~(1<<3))
#define AW8623X_BIT_SYSINTM_FF_AFM_OFF					(1<<3)
#define AW8623X_BIT_SYSINTM_FF_AFM_ON					(0<<3)
#define AW8623X_BIT_SYSINTM_OCDM_MASK					(~(1<<2))
#define AW8623X_BIT_SYSINTM_OCDM_OFF					(1<<2)
#define AW8623X_BIT_SYSINTM_OCDM_ON						(0<<2)
#define AW8623X_BIT_SYSINTM_OTM_MASK					(~(1<<1))
#define AW8623X_BIT_SYSINTM_OTM_OFF						(1<<1)
#define AW8623X_BIT_SYSINTM_OTM_ON						(0<<1)
#define AW8623X_BIT_SYSINTM_DONEM_MASK					(~(1<<0))
#define AW8623X_BIT_SYSINTM_DONEM_OFF					(1<<0)
#define AW8623X_BIT_SYSINTM_DONEM_ON					(0<<0)

/* SYSST2: reg 0x04 RO */
#define AW8623X_BIT_SYSST2_WCK_OK						(1<<5)
#define AW8623X_BIT_SYSST2_BCK_OK						(1<<4)
#define AW8623X_BIT_SYSST2_LDO_OK						(1<<3)
#define AW8623X_BIT_SYSST2_VBG_OK						(1<<2)
#define AW8623X_BIT_SYSST2_FF_FULL						(1<<1)
#define AW8623X_BIT_SYSST2_FF_EMPTY						(1<<0)

/* SYSER: reg 0x05 RC */
#define AW8623X_BIT_SYSER_TRIG1_EVENT					(1<<5)
#define AW8623X_BIT_SYSER_TRIG2_EVENT					(1<<4)
#define AW8623X_BIT_SYSER_TRIG3_EVENT					(1<<3)
#define AW8623X_BIT_SYSER_OV							(1<<2)
#define AW8623X_BIT_SYSER_ADDR_ER						(1<<1)
#define AW8623X_BIT_SYSER_FF_ER							(1<<0)

/* PLAYCFG3: reg 0x08 RW */
#define AW8623X_BIT_PLAYCFG3_ONEWIRE_COMP_MASK			(~(1<<4))
#define AW8623X_BIT_PLAYCFG3_ONEWIRE_1908_MODE			(1<<4)
#define AW8623X_BIT_PLAYCFG3_ONEWIRE_2102_MODE			(0<<4)
#define AW8623X_BIT_PLAYCFG3_STOP_MODE_MASK				(~(1<<3))
#define AW8623X_BIT_PLAYCFG3_STOP_MODE_NOW				(1<<3)
#define AW8623X_BIT_PLAYCFG3_STOP_MODE_LATER			(0<<3)
#define AW8623X_BIT_PLAYCFG3_BRK_EN_MASK				(~(1<<2))
#define AW8623X_BIT_PLAYCFG3_BRK						(1<<2)
#define AW8623X_BIT_PLAYCFG3_BRK_ENABLE					(1<<2)
#define AW8623X_BIT_PLAYCFG3_BRK_DISABLE				(0<<2)
#define AW8623X_BIT_PLAYCFG3_PLAY_MODE_MASK				(~(3<<0))
#define AW8623X_BIT_PLAYCFG3_PLAY_MODE_STOP				(3<<0)
#define AW8623X_BIT_PLAYCFG3_PLAY_MODE_CONT				(2<<0)
#define AW8623X_BIT_PLAYCFG3_PLAY_MODE_RTP				(1<<0)
#define AW8623X_BIT_PLAYCFG3_PLAY_MODE_RAM				(0<<0)

/* PLAYCFG4: reg 0x09 WC */
#define AW8623X_BIT_PLAYCFG4_STOP_MASK					(~(1<<1))
#define AW8623X_BIT_PLAYCFG4_STOP_ON					(1<<1)
#define AW8623X_BIT_PLAYCFG4_STOP_OFF					(0<<1)
#define AW8623X_BIT_PLAYCFG4_GO_MASK					(~(1<<0))
#define AW8623X_BIT_PLAYCFG4_GO_ON						(1<<0)
#define AW8623X_BIT_PLAYCFG4_GO_OFF						(0<<0)

/* WAVCFG1-8: reg 0x0A - reg 0x11 RW */
#define AW8623X_BIT_WAVCFG_SEQWAIT_MASK					(~(1<<7))
#define AW8623X_BIT_WAVCFG_SEQWAIT_TIME					(1<<7)
#define AW8623X_BIT_WAVCFG_SEQWAIT_NUMBER				(0<<7)
#define AW8623X_BIT_WAVCFG_SEQ							(0x7F)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW8623X_BIT_WAVLOOP_SEQ_ODD_MASK				(~(0x0F<<4))
#define AW8623X_BIT_WAVLOOP_SEQ_ODD_INIFINITELY			(0x0F<<4)
#define AW8623X_BIT_WAVLOOP_SEQ_EVEN_MASK				(~(0x0F<<0))
#define AW8623X_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY		(0x0F<<0)
#define AW8623X_BIT_WAVLOOP_INIFINITELY					(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW8623X_BIT_WAVCFG9_SEQ1LOOP_MASK				(~(0x0F<<4))
#define AW8623X_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY		(0x0F<<4)
#define AW8623X_BIT_WAVCFG9_SEQ2LOOP_MASK				(~(0x0F<<0))
#define AW8623X_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG10: reg 0x13 RW */
#define AW8623X_BIT_WAVCFG10_SEQ3LOOP_MASK				(~(0x0F<<4))
#define AW8623X_BIT_WAVCFG10_SEQ3LOOP_INIFINITELY		(0x0F<<4)
#define AW8623X_BIT_WAVCFG10_SEQ4LOOP_MASK				(~(0x0F<<0))
#define AW8623X_BIT_WAVCFG10_SEQ4LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG11: reg 0x14 RW */
#define AW8623X_BIT_WAVCFG11_SEQ5LOOP_MASK				(~(0x0F<<4))
#define AW8623X_BIT_WAVCFG11_SEQ5LOOP_INIFINITELY		(0x0F<<4)
#define AW8623X_BIT_WAVCFG11_SEQ6LOOP_MASK				(~(0x0F<<0))
#define AW8623X_BIT_WAVCFG11_SEQ6LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG12: reg 0x15 RW */
#define AW8623X_BIT_WAVCFG12_SEQ7LOOP_MASK				(~(0x0F<<4))
#define AW8623X_BIT_WAVCFG12_SEQ7LOOP_INIFINITELY		(0x0F<<4)
#define AW8623X_BIT_WAVCFG12_SEQ8LOOP_MASK				(~(0x0F<<0))
#define AW8623X_BIT_WAVCFG12_SEQ8LOOP_INIFINITELY		(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x17 RW */
#define AW8623X_BIT_CONTCFG1_EDGE_FRE_MASK				(~(0x0F<<0))
#define AW8623X_BIT_CONTCFG1_EN_F0_DET_MASK				(~(1<<5))
#define AW8623X_BIT_CONTCFG1_F0_DET_ENABLE				(1<<5)
#define AW8623X_BIT_CONTCFG1_F0_DET_DISABLE				(0<<5)
#define AW8623X_BIT_CONTCFG1_SIN_MODE_MASK				(~(1<<4))
#define AW8623X_BIT_CONTCFG1_SIN_MODE_COS				(1<<0)
#define AW8623X_BIT_CONTCFG1_SIN_MODE_SINE				(0<<0)

/* CONTCFG5: reg 0x1B RW */
#define AW8623X_BIT_CONTCFG5_BRK_GAIN_MASK				(~(0x0F<<0))

/* CONTCFG6: reg 0x1C RW */
#define AW8623X_BIT_CONTCFG6_TRACK_EN_MASK				(~(1<<7))
#define AW8623X_BIT_CONTCFG6_TRACK_ENABLE				(1<<7)
#define AW8623X_BIT_CONTCFG6_TRACK_DISABLE				(0<<7)
#define AW8623X_BIT_CONTCFG6_DRV1_LVL_MASK				(~(0x7F<<0))

/* CONTCFG7: reg 0x1D RW */
#define AW8623X_BIT_CONTCFG7_WAITHZ_EN_MASK				(~(1<<7))
#define AW8623X_BIT_CONTCFG7_WAITHZ_ENABLE				(1<<7)
#define AW8623X_BIT_CONTCFG7_WAITHZ_DISABLE				(0<<7)
#define AW8623X_BIT_CONTCFG7_DRV2_LVL_MASK				(~(0x7F<<0))

/* CONTCFG13: reg 0x23 RW */
#define AW8623X_BIT_CONTCFG13_TSET_MASK					(~(0x0F<<4))
#define AW8623X_BIT_CONTCFG13_BEME_SET_MASK				(~(0x0F<<0))

/***************** RTP *****************/
/* RTPCFG1: reg 0x2C RW */
#define AW8623X_BIT_RTPCFG1_ADDRH_MASK			 		(~(0x0F<<0))

#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK			(~(1<<5))
#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_2K_EN				(1<<5)
#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_2K_DIS			(0<<5)

#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK			(~(1<<4))
#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_1K_EN				(1<<4)
#define AW8623X_BIT_RTPCFG1_SRAM_SIZE_1K_DIS			(0<<4)

/* RTPCFG3: reg 0x2E RW */
#define AW8623X_BIT_RTPCFG3_FIFO_AEH_MASK				(~(0x0F<<4))
#define AW8623X_BIT_RTPCFG3_FIFO_AFH_MASK				(~(0x0F<<0))
#define AW8623X_BIT_RTPCFG3_FIFO_AEH					(0x0F<<4)
#define AW8623X_BIT_RTPCFG3_FIFO_AFH					(0x0F<<0)

/* RTPCFG6: reg 0x31 RW */
#define AW8623X_BIT_RTPCFG6_EN_AUTO_SIN_96K_MASK		(~(1<<3))
#define AW8623X_BIT_RTPCFG6_EN_AUTO_SIN_96K_ENABLE		(1<<3)
#define AW8623X_BIT_RTPCFG6_EN_AUTO_SIN_96K_DISABLE		(0<<3)
#define AW8623X_BIT_RTPCFG6_EN_RTP_AUTO_SIN_MASK		(~(1<<2))
#define AW8623X_BIT_RTPCFG6_EN_RTP_AUTO_SIN_ENABLE		(1<<2)
#define AW8623X_BIT_RTPCFG6_EN_RTP_AUTO_SIN_DISABLE		(0<<2)
#define AW8623X_BIT_RTPCFG6_EN_TWORTP_MASK				(~(1<<1))
#define AW8623X_BIT_RTPCFG6_EN_TWORTP_ENABLE			(1<<1)
#define AW8623X_BIT_RTPCFG6_EN_TWORTP_DISABLE			(0<<1)
#define AW8623X_BIT_RTPCFG6_WCK_PIN_MASK				(~(1<<0))
#define AW8623X_BIT_RTPCFG6_WCK_PIN_ENABLE				(1<<0)
#define AW8623X_BIT_RTPCFG6_WCK_PIN_DISABLE				(0<<0)

/***************** TRIGGER *****************/
#define AW8623X_BIT_TRG_ENABLE_MASK						(~(1<<7))
#define AW8623X_BIT_TRG_ENABLE							(1<<7)
#define AW8623X_BIT_TRG_DISABLE							(0<<7)
#define AW8623X_BIT_TRG_SEQ_MASK						(~(0x7F<<0))

/* TRGCFG1: reg 0x33 RW */
#define AW8623X_BIT_TRGCFG1_TRG1_POS_MASK				(~(1<<7))
#define AW8623X_BIT_TRGCFG1_TRG1_POS_ENABLE				(1<<7)
#define AW8623X_BIT_TRGCFG1_TRG1_POS_DISABLE			(0<<7)
#define AW8623X_BIT_TRGCFG1_TRG1SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG2: reg 0x34 RW */
#define AW8623X_BIT_TRGCFG2_TRG2_POS_MASK				(~(1<<7))
#define AW8623X_BIT_TRGCFG2_TRG2_POS_ENABLE				(1<<7)
#define AW8623X_BIT_TRGCFG2_TRG2_POS_DISABLE			(0<<7)
#define AW8623X_BIT_TRGCFG2_TRG2SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG3: r3g 0x35 RW */
#define AW8623X_BIT_TRGCFG3_TRG3_POS_MASK				(~(1<<7))
#define AW8623X_BIT_TRGCFG3_TRG3_POS_ENABLE				(1<<7)
#define AW8623X_BIT_TRGCFG3_TRG3_POS_DISABLE			(0<<7)
#define AW8623X_BIT_TRGCFG3_TRG3SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG4: r3g 0x36 RW */
#define AW8623X_BIT_TRGCFG4_TRG1_NEG_MASK				(~(1<<7))
#define AW8623X_BIT_TRGCFG4_TRG1_NEG_ENABLE				(1<<7)
#define AW8623X_BIT_TRGCFG4_TRG1_NEG_DISABLE			(0<<7)
#define AW8623X_BIT_TRGCFG4_TRG1SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG5: r3g 0x37 RW */
#define AW8623X_BIT_TRGCFG5_TRG2_NEG_MASK				(~(1<<7))
#define AW8623X_BIT_TRGCFG5_TRG2_NEG_ENABLE				(1<<7)
#define AW8623X_BIT_TRGCFG5_TRG2_NEG_DISABLE			(0<<7)
#define AW8623X_BIT_TRGCFG5_TRG2SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG6: r3g 0x38 RW */
#define AW8623X_BIT_TRGCFG6_TRG3_NEG_MASK				~(1<<7))
#define AW8623X_BIT_TRGCFG6_TRG3_NEG_ENABLE				1<<7)
#define AW8623X_BIT_TRGCFG6_TRG3_NEG_DISABLE			0<<7)
#define AW8623X_BIT_TRGCFG6_TRG3SEQ_N_MASK				~(0x7F<<0))

/* TRGCFG7: reg 0x39 RW */
#define AW8623X_BIT_TRGCFG7_TRG1_POR_LEV_BRK_MASK		(~(7<<3))
#define AW8623X_BIT_TRGCFG7_TRG2_POR_LEV_BRK_MASK		(~(7<<0))
#define AW8623X_BIT_TRGCFG7_TRG1_POLAR_MASK				(~(1<<5))
#define AW8623X_BIT_TRGCFG7_TRG1_POLAR_NEG				(1<<5)
#define AW8623X_BIT_TRGCFG7_TRG1_POLAR_POS				(0<<5)
#define AW8623X_BIT_TRGCFG7_TRG1_MODE_MASK				(~(1<<4))
#define AW8623X_BIT_TRGCFG7_TRG1_MODE_LEVEL				(1<<4)
#define AW8623X_BIT_TRGCFG7_TRG1_MODE_EDGE				(0<<4)
#define AW8623X_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK			(~(1<<3))
#define AW8623X_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE		(1<<3)
#define AW8623X_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE		(0<<3)
#define AW8623X_BIT_TRGCFG7_TRG2_POLAR_MASK				(~(1<<2))
#define AW8623X_BIT_TRGCFG7_TRG2_POLAR_NEG				(1<<2)
#define AW8623X_BIT_TRGCFG7_TRG2_POLAR_POS				(0<<2)
#define AW8623X_BIT_TRGCFG7_TRG2_MODE_MASK				(~(1<<1))
#define AW8623X_BIT_TRGCFG7_TRG2_MODE_LEVEL				(1<<1)
#define AW8623X_BIT_TRGCFG7_TRG2_MODE_EDGE				(0<<1)
#define AW8623X_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK			(~(1<<0))
#define AW8623X_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE		(1<<0)
#define AW8623X_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE		(0<<0)

/* TRGCFG8: reg 0x3A RW */
#define AW8623X_BIT_TRGCFG8_TRG3_POR_LEV_BRK_MASK		(~(7<<3))
#define AW8623X_BIT_TRGCFG8_TRG3_POLAR_MASK				(~(1<<5))
#define AW8623X_BIT_TRGCFG8_TRG3_POLAR_NEG				(1<<5)
#define AW8623X_BIT_TRGCFG8_TRG3_POLAR_POS				(0<<5)
#define AW8623X_BIT_TRGCFG8_TRG3_MODE_MASK				(~(1<<4))
#define AW8623X_BIT_TRGCFG8_TRG3_MODE_LEVEL				(1<<4)
#define AW8623X_BIT_TRGCFG8_TRG3_MODE_EDGE				(0<<4)
#define AW8623X_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK			(~(1<<3))
#define AW8623X_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE		(1<<3)
#define AW8623X_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE		(0<<3)
#define AW8623X_BIT_TRGCFG8_TRG1_STOP_MASK				(~(1<<2))
#define AW8623X_BIT_TRGCFG8_TRG1_STOP					(1<<2)
#define AW8623X_BIT_TRGCFG8_TRG2_STOP_MASK				(~(1<<1))
#define AW8623X_BIT_TRGCFG8_TRG2_STOP					(1<<1)
#define AW8623X_BIT_TRGCFG8_TRG3_STOP_MASK				(~(1<<0))
#define AW8623X_BIT_TRGCFG8_TRG3_STOP					(1<<0)

/* GLBCFG4: reg 0x3E RW */
#define AW8623X_BIT_GLBCFG4_GO_PRIO_MASK				(~(3<<6))
#define AW8623X_BIT_GLBCFG4_TRG3_PRIO_MASK				(~(3<<4))
#define AW8623X_BIT_GLBCFG4_TRG2_PRIO_MASK				(~(3<<2))
#define AW8623X_BIT_GLBCFG4_TRG1_PRIO_MASK				(~(3<<0))

/* GLBRD5: reg 0x3F R0 */
/* GLB_STATE */
#define AW8623X_BIT_GLBRD5_STATE						(15<<0)
#define AW8623X_BIT_GLBRD5_STATE_STANDBY				(0<<0)
#define AW8623X_BIT_GLBRD5_STATE_WAKEUP					(1<<0)
#define AW8623X_BIT_GLBRD5_STATE_STARTUP				(2<<0)
#define AW8623X_BIT_GLBRD5_STATE_WAIT					(3<<0)
#define AW8623X_BIT_GLBRD5_STATE_CONT_GO				(6<<0)
#define AW8623X_BIT_GLBRD5_STATE_RAM_GO					(7<<0)
#define AW8623X_BIT_GLBRD5_STATE_RTP_GO					(8<<0)
#define AW8623X_BIT_GLBRD5_STATE_TRIG_GO				(9<<0)
#define AW8623X_BIT_GLBRD5_STATE_I2S_GO					(10<<0)
#define AW8623X_BIT_GLBRD5_STATE_BRAKE					(11<<0)
#define AW8623X_BIT_GLBRD5_STATE_END					(12<<0)
/* RAMADDRH: reg 0x40 RWS */
#define AW8623X_BIT_RAMADDRH_MASK						(~(63<<0))

/* I2SCFG1: reg 0x44 RW */
#define AW8623X_BIT_I2SCFG1_I2S_EN_MASK					(~(1<<2))
#define AW8623X_BIT_I2SCFG1_I2S_ENABLE					(1<<2)
#define AW8623X_BIT_I2SCFG1_I2S_DISABLE					(0<<2)
#define AW8623X_BIT_I2SCFG1_I2S_MD_MASK					(~(1<<1))
#define AW8623X_BIT_I2SCFG1_I2S_MD_MSB					(1<<1)
#define AW8623X_BIT_I2SCFG1_I2S_MD_PHILIP				(0<<1)
#define AW8623X_BIT_I2SCFG1_I2S_SLOT_CHSEL_MASK			(~(1<<0))
#define AW8623X_BIT_I2SCFG1_I2S_SLOT_CHSEL_R_BEHIND		(1<<0)
#define AW8623X_BIT_I2SCFG1_I2S_SLOT_CHSEL_L_BEYOND		(0<<0)

/***************** SYSCTRL *****************/
/* SYSCTRL1: reg 0x45 RW */
#define AW8623X_BIT_SYSCTRL1_TRIG1_MODE_MASK			(~(7<<4))
#define AW8623X_BIT_SYSCTRL1_TRIG1_NONE					(7<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_AD					(6<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_RCK					(5<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_BCK					(4<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_TRIG					(3<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_ONE_WIRE				(2<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_PWM_ERM				(1<<4)
#define AW8623X_BIT_SYSCTRL1_TRIG1_PWM_LRA				(0<<4)
#define AW8623X_BIT_SYSCTRL1_GAIN_BYPASS_MASK			(~(1<<3))
#define AW8623X_BIT_SYSCTRL1_GAIN_CHANGE_ENABLE			(1<<3)
#define AW8623X_BIT_SYSCTRL1_GAIN_CHANGE_DISABLE		(0<<3)
#define AW8623X_BIT_SYSCTRL1_RAMINIT_MASK				(~(1<<2))
#define AW8623X_BIT_SYSCTRL1_RAMINIT_ON					(1<<2)
#define AW8623X_BIT_SYSCTRL1_RAMINIT_OFF				(0<<2)
#define AW8623X_BIT_SYSCTRL1_EN_FIR_MASK				(~(1<<1))
#define AW8623X_BIT_SYSCTRL1_FIR_ENABLE					(0<<1)
#define AW8623X_BIT_SYSCTRL1_INTN_PIN_MASK				(~(1<<0))
#define AW8623X_BIT_SYSCTRL1_INTN						(1<<0)
#define AW8623X_BIT_SYSCTRL1_TRIG1						(0<<0)

/* SYSCTRL2: reg 0x46 RW */
#define AW8623X_BIT_SYSCTRL2_WAKE_MASK					(~(1<<7))
#define AW8623X_BIT_SYSCTRL2_WAKE_ON					(1<<7)
#define AW8623X_BIT_SYSCTRL2_WAKE_OFF					(0<<7)
#define AW8623X_BIT_SYSCTRL2_STANDBY_MASK				(~(1<<6))
#define AW8623X_BIT_SYSCTRL2_STANDBY_ON					(1<<6)
#define AW8623X_BIT_SYSCTRL2_STANDBY_OFF				(0<<6)
#define AW8623X_BIT_SYSCTRL2_EN_DLL_MASK				(~(1<<5))
#define AW8623X_BIT_SYSCTRL2_EN_DLL						(1<<5)
#define AW8623X_BIT_SYSCTRL2_DISENABLE_DLL				(0<<5)
#define AW8623X_BIT_SYSCTRL2_RCK_FRE_MASK				(~(3<<3))
#define AW8623X_BIT_SYSCTRL2_RCK_FRE_96K				(3<<3)
#define AW8623X_BIT_SYSCTRL2_RCK_FRE_48K				(2<<3)
#define AW8623X_BIT_SYSCTRL2_RCK_FRE_32_768K			(1<<3)
#define AW8623X_BIT_SYSCTRL2_RCK_FRE_24K				(0<<3)
#define AW8623X_BIT_SYSCTRL2_EN_INTN_CLKOUT_MASK		(~(1<<2))
#define AW8623X_BIT_SYSCTRL2_EN_INTN_CLKOUT				(1<<2)
#define AW8623X_BIT_SYSCTRL2_DISENABLE_INTN_CLKOUT		(0<<2)
#define AW8623X_BIT_SYSCTRL2_WAVDAT_MODE_MASK			(~(3<<0))
#define AW8623X_BIT_SYSCTRL2_RATE						(3<<0)
#define AW8623X_BIT_SYSCTRL2_RATE_12K					(2<<0)
#define AW8623X_BIT_SYSCTRL2_RATE_24K					(0<<0)
#define AW8623X_BIT_SYSCTRL2_RATE_48K					(1<<0)

/* SYSCTRL3: reg 0x47 RW */
#define AW8623X_BIT_SYSCTRL3_EN_TRIG1_PD_MASK			(~(1<<6))
#define AW8623X_BIT_SYSCTRL3_EN_TRIG1_PD				(1<<6)
#define AW8623X_BIT_SYSCTRL3_DISENABLE_TRIG1_PD			(0<<6)
#define AW8623X_BIT_SYSCTRL3_RTP_DLY_MASK				(~(3<<4))
#define AW8623X_BIT_SYSCTRL3_RTP_DLY_FIFO_AE			(3<<4)
#define AW8623X_BIT_SYSCTRL3_RTP_DLY_FIFO_8				(2<<4)
#define AW8623X_BIT_SYSCTRL3_RTP_DLY_FIFO_5				(1<<4)
#define AW8623X_BIT_SYSCTRL3_RTP_DLY_FIFO_1				(0<<4)
#define AW8623X_BIT_SYSCTRL3_INTMODE_MASK				(~(3<<2))
#define AW8623X_BIT_SYSCTRL3_INTMODE_BOTH_EDGE			(3<<2)
#define AW8623X_BIT_SYSCTRL3_INTMODE_LEVEL_1			(2<<2)
#define AW8623X_BIT_SYSCTRL3_INTMODE_POS_EDGE			(1<<2)
#define AW8623X_BIT_SYSCTRL3_INTMODE_LEVEL_2			(0<<2)
#define AW8623X_BIT_SYSCTRL3_INTP_MASK					(~(1<<1))
#define AW8623X_BIT_SYSCTRL3_INTP_ACTIVE_HIGH			(1<<1)
#define AW8623X_BIT_SYSCTRL3_INTP_ACTIVE_LOW			(0<<1)
#define AW8623X_BIT_SYSCTRL3_WAKE_MODE_MASK				(~(1<<0))
#define AW8623X_BIT_SYSCTRL3_WAKE_MODE_WAKEUP			(1<<0)
#define AW8623X_BIT_SYSCTRL3_WAKE_MODE_BST				(0<<0)

/* SYSCTRL5: reg 0x49 RW */
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_MASK			(~(7<<2))
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_VBAT_CTRL		(5<<2)
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_TRIG_ENG		(4<<2)
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_CONT_ENG		(3<<2)
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_RTP_ENG			(2<<2)
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_RAM_ENG			(1<<2)
#define AW8623X_BIT_SYSCTRL5_MODULE_SEL_EFUSE_ENG		(0<<2)
#define AW8623X_BIT_SYSCTRL5_DEV_ADDR_CTR_MASK			(~(3<<0))
#define AW8623X_BIT_SYSCTRL5_DEV_ADDR_CTR_HW_IRSW		(3<<0)
#define AW8623X_BIT_SYSCTRL5_DEV_ADDR_CTR_HW_1			(2<<0)
#define AW8623X_BIT_SYSCTRL5_DEV_ADDR_CTR_IRSW			(1<<0)
#define AW8623X_BIT_SYSCTRL5_DEV_ADDR_CTR_HW_2			(0<<0)

/* PWMCFG1: reg 0x4A RW */
#define AW8623X_BIT_PWMCFG1_PRC_EN_MASK					(~(1<<7))
#define AW8623X_BIT_PWMCFG1_PRC_ENABLE					(1<<7)
#define AW8623X_BIT_PWMCFG1_PRC_DISABLE					(0<<7)
#define AW8623X_BIT_PWMCFG1_PRCTIME_MASK				(~(0x7F<<0))

/* PWMCFG2: reg 0x4B RW */
#define AW8623X_BIT_PWMCFG2_PRCT_MODE_MASK				(~(1<<6))
#define AW8623X_BIT_PWMCFG2_PRCT_MODE_VALID				(1<<6)
#define AW8623X_BIT_PWMCFG2_PRCT_MODE_INVALID			(0<<6)
#define AW8623X_BIT_PWMCFG2_REF_SEL_MASK				(~(1<<5))
#define AW8623X_BIT_PWMCFG2_REF_SEL_TRIANGLE			(1<<5)
#define AW8623X_BIT_PWMCFG2_REF_SEL_SAWTOOTH			(0<<5)
#define AW8623X_BIT_PWMCFG2_PD_HWM_MASK					(~(1<<4))
#define AW8623X_BIT_PWMCFG2_PD_HWM_FULL					(1<<4)
#define AW8623X_BIT_PWMCFG2_PD_HWM_HALF					(0<<4)
#define AW8623X_BIT_PWMCFG2_PWMOE_MASK					(~(1<<3))
#define AW8623X_BIT_PWMCFG2_PWMOE_ON					(1<<3)
#define AW8623X_BIT_PWMCFG2_PWMOE_OFF					(0<<3)
#define AW8623X_BIT_PWMCFG2_PWMFRC_MASK					(~(7<<0))

/* PWMCFG3: reg 0x4C RW */
#define AW8623X_BIT_PWMCFG3_PR_EN_MASK					(~(1<<7))
#define AW8623X_BIT_PWMCFG3_PR_ENABLE					(1<<7)
#define AW8623X_BIT_PWMCFG3_PR_DISABLE					(0<<7)
#define AW8623X_BIT_PWMCFG3_PRLVL_MASK					(~(0x7F<<0))
#define AW8623X_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE			(0x3F)

/* PWMCFG4: reg 0x4D RW */
/* PRTIME */
#define AW8623X_PWMCFG4_PRTIME_DEFAULT_VALUE			(0x32)

/* VBATCTRL: reg 0x4E RW */
#define AW8623X_BIT_VBATCTRL_VBAT_PRO_MASK				(~(1<<7))
#define AW8623X_BIT_VBATCTRL_VBAT_PRO_ENABLE			(1<<7)
#define AW8623X_BIT_VBATCTRL_VBAT_PRO_DISABLE			(0<<7)
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_MASK				(~(1<<6))
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_HW				(1<<6)
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_SW				(0<<6)
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_CON_MASK			(~(1<<5))
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_CON_DURING		(1<<5)
#define AW8623X_BIT_VBATCTRL_VBAT_MODE_CON_BEFORE		(0<<5)
#define AW8623X_BIT_VBATCTRL_DELTA_VBAT_MASK			(~(1<<4))
#define AW8623X_BIT_VBATCTRL_DELTA_VBAT_0_2				(1<<4)
#define AW8623X_BIT_VBATCTRL_DELTA_VBAT_0_1				(0<<4)
#define AW8623X_BIT_VBATCTRL_REL_VBAT_MASK				(~(3<<2))
#define AW8623X_BIT_VBATCTRL_REL_VBAT_100				(3<<2)
#define AW8623X_BIT_VBATCTRL_REL_VBAT_50				(2<<2)
#define AW8623X_BIT_VBATCTRL_REL_VBAT_25				(1<<2)
#define AW8623X_BIT_VBATCTRL_REL_VBAT_10				(0<<2)
#define AW8623X_BIT_VBATCTRL_ABS_VBAT_MASK				(~(3<<0))
#define AW8623X_BIT_VBATCTRL_ABS_VBAT_3_5				(3<<0)
#define AW8623X_BIT_VBATCTRL_ABS_VBAT_3_4				(2<<0)
#define AW8623X_BIT_VBATCTRL_ABS_VBAT_3_3				(1<<0)
#define AW8623X_BIT_VBATCTRL_ABS_VBAT_3_2				(0<<0)

/* DETCFG1: reg 0x4F RW */
#define AW8623X_BIT_DETCFG1_EN_VBAT_AVG_MASK			(~(1<<7))
#define AW8623X_BIT_DETCFG1_EN_VBAT_AVG_16_TIMES		(1<<7)
#define AW8623X_BIT_DETCFG1_VBAT_REF_MASK				(~(7<<4))
#define AW8623X_BIT_DETCFG1_VBAT_REF_5_5				(7<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_5_0				(6<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_4_8				(5<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_4_5				(4<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_4_2				(3<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_4_0				(2<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_3_6				(1<<4)
#define AW8623X_BIT_DETCFG1_VBAT_REF_3_3				(0<<4)
#define AW8623X_BIT_DETCFG1_ADC_FS_MASK					(~(3<<2))
#define AW8623X_BIT_DETCFG1_ADC_FS_24K					(3<<2)
#define AW8623X_BIT_DETCFG1_ADC_FS_48K					(2<<2)
#define AW8623X_BIT_DETCFG1_ADC_FS_96K					(1<<2)
#define AW8623X_BIT_DETCFG1_ADC_FS_192K					(0<<2)
#define AW8623X_BIT_DETCFG1_DET_GO_MASK					(~(3<<0))
#define AW8623X_BIT_DETCFG1_DET_GO_ON					(1<<0)
#define AW8623X_BIT_DETCFG1_DET_GO_OFF					(0<<0)

/* DETCFG2: reg 0x50 RW */
#define AW8623X_BIT_DETCFG2_DET_SEQ0_MASK				(~(0X0F<<3))
#define AW8623X_BIT_DETCFG2_DET_SEQ0_HDN_1				(0x0E<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_HDN_2				(0x0A<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_HDP_1				(0x0D<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_HDP_2				(9<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_FTS				(6<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_VOUT				(5<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_OS					(4<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_RL					(3<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_TRIG1				(2<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_PVDD				(1<<3)
#define AW8623X_BIT_DETCFG2_DET_SEQ0_VBAT				(0<<3)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_MASK				(~(7<<0))
#define AW8623X_BIT_DETCFG2_D2S_GAIN_40					(7<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_20					(6<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_10					(5<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_8					(4<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_5					(3<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_4					(2<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_2					(1<<0)
#define AW8623X_BIT_DETCFG2_D2S_GAIN_1					(0<<0)

/* DETRD1: reg 0x51 RO */
#define AW8623X_BIT_DETRD1_AVG_DATA_H_MASK				(~(3<<0))

/* TRIMCFG1: reg 0x59 RW */
#define AW8623X_BIT_TRIMCFG1_RL_TRIM_SRC_MASK			(~(1<<7))
#define AW8623X_BIT_TRIMCFG1_RL_TRIM_SRC_REG			(1<<7)
#define AW8623X_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE			(0<<7)
#define AW8623X_BIT_TRIMCFG1_TRIM_RL_MASK				(~(0x7F<<0))

/* TRIMCFG2: reg 0x5A RW */
#define AW8623X_BIT_TRIMCFG2_OSC_TRIM_SRC_MASK			(~(1<<6))
#define AW8623X_BIT_TRIMCFG2_OSC_TRIM_SRC_REG			(1<<6)
#define AW8623X_BIT_TRIMCFG2_OSC_TRIM_SRC_EFUSE			(0<<6)
#define AW8623X_BIT_TRIMCFG2_TRIM_LRA_MASK				(~(0x3F<<0))

/* TMCFG: reg 0x5E RW */
#define AW8623X_BIT_TMCFG_TM_LOCK						(0x00)
#define AW8623X_BIT_TMCFG_TM_UNLOCK						(0x7d)

/* ANACFG3: reg:0x70 RW */
#define AW8623X_BIT_ANACFG3_EN_OVP_SAR_MASK				(~(1<<7))
#define AW8623X_BIT_ANACFG3_EN_OVP_SAR_ENABLE			(1<<7)
#define AW8623X_BIT_ANACFG3_EN_OVP_SAR_DISABLE			(0<<7)

/* ANACFG4: reg:0x71 RW */
#define AW8623X_BIT_ANACFG4_PRO_HDRV_MASK				(~(1<<7))
#define AW8623X_BIT_ANACFG4_PRO_HDRV_ENABLE				(1<<7)
#define AW8623X_BIT_ANACFG4_PRO_HDRV_DISABLE			(0<<7)

/* ANACFG5: reg:0x72 RW */
#define AW8623X_BIT_ANACFG5_UVLO_ADJ_MASK				(~(3<<0))
#define AW8623X_BIT_ANACFG5_UVLO_ADJ_2_7_V				(3<<0)
#define AW8623X_BIT_ANACFG5_UVLO_ADJ_2_6_V				(2<<0)
#define AW8623X_BIT_ANACFG5_UVLO_ADJ_2_5_V				(1<<0)
#define AW8623X_BIT_ANACFG5_UVLO_ADJ_2_4_V				(0<<0)

/******************************************************
 * AW8624X Register Detail
 *****************************************************/
/* SYSST: reg 0x01 RO */
#define AW8624X_BIT_SYSST_UVLS							(1<<5)
#define AW8624X_BIT_SYSST_FF_AES						(1<<4)
#define AW8624X_BIT_SYSST_FF_AFS						(1<<3)
#define AW8624X_BIT_SYSST_OCDS							(1<<2)
#define AW8624X_BIT_SYSST_OTS							(1<<1)
#define AW8624X_BIT_SYSST_DONES							(1<<0)

/* SYSINT: reg 0x02 RC */
#define AW8624X_BIT_SYSINT_UVLI							(1<<5)
#define AW8624X_BIT_SYSINT_FF_AEI						(1<<4)
#define AW8624X_BIT_SYSINT_FF_AFI						(1<<3)
#define AW8624X_BIT_SYSINT_OCDI							(1<<2)
#define AW8624X_BIT_SYSINT_OTI							(1<<1)
#define AW8624X_BIT_SYSINT_DONEI						(1<<0)

/* SYSINTM: reg 0x03 RW */
#define AW8624X_BIT_SYSINTM_UVLM_MASK					(~(1<<5))
#define AW8624X_BIT_SYSINTM_UVLM_OFF					(1<<5)
#define AW8624X_BIT_SYSINTM_UVLM_ON						(0<<5)
#define AW8624X_BIT_SYSINTM_FF_AEM_MASK					(~(1<<4))
#define AW8624X_BIT_SYSINTM_FF_AEM_OFF					(1<<4)
#define AW8624X_BIT_SYSINTM_FF_AEM_ON					(0<<4)
#define AW8624X_BIT_SYSINTM_FF_AFM_MASK					(~(1<<3))
#define AW8624X_BIT_SYSINTM_FF_AFM_OFF					(1<<3)
#define AW8624X_BIT_SYSINTM_FF_AFM_ON					(0<<3)
#define AW8624X_BIT_SYSINTM_OCDM_MASK					(~(1<<2))
#define AW8624X_BIT_SYSINTM_OCDM_OFF					(1<<2)
#define AW8624X_BIT_SYSINTM_OCDM_ON						(0<<2)
#define AW8624X_BIT_SYSINTM_OTM_MASK					(~(1<<1))
#define AW8624X_BIT_SYSINTM_OTM_OFF						(1<<1)
#define AW8624X_BIT_SYSINTM_OTM_ON						(0<<1)
#define AW8624X_BIT_SYSINTM_DONEM_MASK					(~(1<<0))
#define AW8624X_BIT_SYSINTM_DONEM_OFF					(1<<0)
#define AW8624X_BIT_SYSINTM_DONEM_ON					(0<<0)

/* SYSST2: reg 0x04 RO */
#define AW8624X_BIT_SYSST2_RAM_ADDR_ER					(1<<5)
#define AW8624X_BIT_SYSST2_TRG_ADDR_ER					(1<<4)
#define AW8624X_BIT_SYSST2_DVDD_PORN					(1<<3)
#define AW8624X_BIT_SYSST2_VBG_OK						(1<<2)
#define AW8624X_BIT_SYSST2_FF_FULL						(1<<1)
#define AW8624X_BIT_SYSST2_FF_EMPTY						(1<<0)

/* SYSER: reg 0x05 RC */
#define AW8624X_BIT_SYSER_TRIG1_EVENT					(1<<6)
#define AW8624X_BIT_SYSER_TRIG2_EVENT					(1<<5)
#define AW8624X_BIT_SYSER_TRIG3_EVENT					(1<<4)
#define AW8624X_BIT_SYSER_OV							(1<<3)
#define AW8624X_BIT_SYSER_ADDR_ER						(1<<2)
#define AW8624X_BIT_SYSER_FF_ER							(1<<1)
#define AW8624X_BIT_SYSER_PLL_REF_ER					(1<<0)

/* PLAYCFG3: reg 0x08 RW */
#define AW8624X_BIT_PLAYCFG3_STOP_MODE_MASK				(~(1<<5))
#define AW8624X_BIT_PLAYCFG3_STOP_MODE_NOW				(1<<5)
#define AW8624X_BIT_PLAYCFG3_STOP_MODE_LATER			(0<<5)
#define AW8624X_BIT_PLAYCFG3_BRK_EN_MASK				(~(1<<2))
#define AW8624X_BIT_PLAYCFG3_BRK						(1<<2)
#define AW8624X_BIT_PLAYCFG3_BRK_ENABLE					(1<<2)
#define AW8624X_BIT_PLAYCFG3_BRK_DISABLE				(0<<2)
#define AW8624X_BIT_PLAYCFG3_PLAY_MODE_MASK				(~(3<<0))
#define AW8624X_BIT_PLAYCFG3_PLAY_MODE_STOP				(3<<0)
#define AW8624X_BIT_PLAYCFG3_PLAY_MODE_CONT				(2<<0)
#define AW8624X_BIT_PLAYCFG3_PLAY_MODE_RTP				(1<<0)
#define AW8624X_BIT_PLAYCFG3_PLAY_MODE_RAM				(0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW8624X_BIT_PLAYCFG4_STOP_MASK					(~(1<<1))
#define AW8624X_BIT_PLAYCFG4_STOP_ON					(1<<1)
#define AW8624X_BIT_PLAYCFG4_STOP_OFF					(0<<1)
#define AW8624X_BIT_PLAYCFG4_GO_MASK					(~(1<<0))
#define AW8624X_BIT_PLAYCFG4_GO_ON						(1<<0)
#define AW8624X_BIT_PLAYCFG4_GO_OFF						(0<<0)

/* WAVCFG1-8: reg 0x0A - reg 0x11 RW */
#define AW8624X_BIT_WAVCFG_SEQWAIT_MASK					(~(1<<7))
#define AW8624X_BIT_WAVCFG_SEQWAIT_TIME					(1<<7)
#define AW8624X_BIT_WAVCFG_SEQWAIT_NUMBER				(0<<7)
#define AW8624X_BIT_WAVCFG_SEQ							(0x7F)

/* WAVCFG9-12: reg 0x12 - reg 0x15 RW */
#define AW8624X_BIT_WAVLOOP_SEQ_ODD_MASK				(~(0x0F<<4))
#define AW8624X_BIT_WAVLOOP_SEQ_ODD_INIFINITELY			(0x0F<<4)
#define AW8624X_BIT_WAVLOOP_SEQ_EVEN_MASK				(~(0x0F<<0))
#define AW8624X_BIT_WAVLOOP_SEQ_EVEN_INIFINITELY		(0x0F<<0)
#define AW8624X_BIT_WAVLOOP_INIFINITELY					(0x0F<<0)

/* WAVCFG9: reg 0x12 RW */
#define AW8624X_BIT_WAVCFG9_SEQ1LOOP_MASK				(~(0x0F<<4))
#define AW8624X_BIT_WAVCFG9_SEQ1LOOP_INIFINITELY		(0x0F<<4)
#define AW8624X_BIT_WAVCFG9_SEQ2LOOP_MASK				(~(0x0F<<0))
#define AW8624X_BIT_WAVCFG9_SEQ2LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG10: reg 0x13 RW */
#define AW8624X_BIT_WAVCFG10_SEQ3LOOP_MASK				(~(0x0F<<4))
#define AW8624X_BIT_WAVCFG10_SEQ3LOOP_INIFINITELY		(0x0F<<4)
#define AW8624X_BIT_WAVCFG10_SEQ4LOOP_MASK				(~(0x0F<<0))
#define AW8624X_BIT_WAVCFG10_SEQ4LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG11: reg 0x14 RW */
#define AW8624X_BIT_WAVCFG11_SEQ5LOOP_MASK				(~(0x0F<<4))
#define AW8624X_BIT_WAVCFG11_SEQ5LOOP_INIFINITELY		(0x0F<<4)
#define AW8624X_BIT_WAVCFG11_SEQ6LOOP_MASK				(~(0x0F<<0))
#define AW8624X_BIT_WAVCFG11_SEQ6LOOP_INIFINITELY		(0x0F<<0)

/* WAVCFG12: reg 0x15 RW */
#define AW8624X_BIT_WAVCFG12_SEQ7LOOP_MASK				(~(0x0F<<4))
#define AW8624X_BIT_WAVCFG12_SEQ7LOOP_INIFINITELY		(0x0F<<4)
#define AW8624X_BIT_WAVCFG12_SEQ8LOOP_MASK				(~(0x0F<<0))
#define AW8624X_BIT_WAVCFG12_SEQ8LOOP_INIFINITELY		(0x0F<<0)

/***************** CONT *****************/
/* CONTCFG1: reg 0x18 RW */
#define AW8624X_BIT_CONTCFG1_EDGE_FRE_MASK				(~(0x0F<<4))
#define AW8624X_BIT_CONTCFG1_PEAK_NUM_MASK				(~(3<<1))
#define AW8624X_BIT_CONTCFG1_PEAK_NUM_NO_F0_DET			(0<<1)
#define AW8624X_BIT_CONTCFG1_PEAK_NUM_1					(1<<1)
#define AW8624X_BIT_CONTCFG1_PEAK_NUM_2					(2<<1)
#define AW8624X_BIT_CONTCFG1_PEAK_NUM_3					(3<<1)
#define AW8624X_BIT_CONTCFG1_SIN_MODE_MASK				(~(1<<0))
#define AW8624X_BIT_CONTCFG1_SIN_MODE_COS				(1<<0)
#define AW8624X_BIT_CONTCFG1_SIN_MODE_SINE				(0<<0)

/* CONTCFG5: reg 0x1C RW */
#define AW8624X_BIT_CONTCFG5_OPEN_LOOP_NUM_MASK			(~(7<<5))
#define AW8624X_BIT_CONTCFG5_SMART_LOOP_MASK			(~(1<<4))
#define AW8624X_BIT_CONTCFG5_SMART_LOOP_ON				(1<<4)
#define AW8624X_BIT_CONTCFG5_SMART_LOOP_OFF				(0<<4)
#define AW8624X_BIT_CONTCFG5_BRK_GAIN_MASK				(~(0x0F<<0))

/* CONTCFG6: reg 0x1D RW */
#define AW8624X_BIT_CONTCFG6_TRACK_EN_MASK				(~(1<<7))
#define AW8624X_BIT_CONTCFG6_TRACK_ENABLE				(1<<7)
#define AW8624X_BIT_CONTCFG6_TRACK_DISABLE				(0<<7)
#define AW8624X_BIT_CONTCFG6_DRV1_LVL_MASK				(~(0x7F<<0))

/* CONTCFG7: reg 0x1E RW */
#define AW8624X_BIT_CONTCFG7_DRV_SWITCH_MASK			(~(1<<7))
#define AW8624X_BIT_CONTCFG7_DRV_SWITCH_ENABLE			(1<<7)
#define AW8624X_BIT_CONTCFG7_DRV_SWITCH_DISABLE			(0<<7)
#define AW8624X_BIT_CONTCFG7_DRV2_LVL_MASK				(~(0x7F<<0))

/* CONTRD20: reg 0x2B RO */
#define AW8624X_BIT_CONTRD20_BEMF_PEAK4_L_MASK			(~(3<<6))
#define AW8624X_BIT_CONTRD20_BEMF_PEAK3_L_MASK			(~(3<<4))
#define AW8624X_BIT_CONTRD20_BEMF_PEAK2_L_MASK			(~(3<<2))
#define AW8624X_BIT_CONTRD20_BEMF_PEAK1_L_MASK			(~(3<<0))

/* CONTRD21: reg 0x2C RO */
#define AW8624X_BIT_CONTRD21_BEMF_PEAK5_L_MASK			(~(3<<0))

/***************** RTP *****************/
/* RTPCFG1: reg 0x2D RW */
#define AW8624X_BIT_RTPCFG1_ADDRH_MASK					(~(0x0F<<0))

/* RTPCFG3: reg 0x2F RW */
#define AW8624X_BIT_RTPCFG3_FIFO_AEH_MASK				(~(0x0F<<4))
#define AW8624X_BIT_RTPCFG3_FIFO_AFH_MASK				(~(0x0F<<0))
#define AW8624X_BIT_RTPCFG3_FIFO_AEH					(0x0F<<4)
#define AW8624X_BIT_RTPCFG3_FIFO_AFH					(0x0F<<0)

/***************** TRIGGER *****************/
#define AW8624X_BIT_TRG_ENABLE_MASK						(~(1<<7))
#define AW8624X_BIT_TRG_ENABLE							(1<<7)
#define AW8624X_BIT_TRG_DISABLE							(0<<7)
#define AW8624X_BIT_TRG_SEQ_MASK						(~(0x7F<<0))

/* TRGCFG1: reg 0x33 RW */
#define AW8624X_BIT_TRGCFG1_TRG1_POS_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG1_TRG1_POS_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG1_TRG1_POS_DISABLE			(0<<7)
#define AW8624X_BIT_TRGCFG1_TRG1SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG2: reg 0x34 RW */
#define AW8624X_BIT_TRGCFG2_TRG2_POS_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG2_TRG2_POS_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG2_TRG2_POS_DISABLE			(0<<7)
#define AW8624X_BIT_TRGCFG2_TRG2SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG3: reg 0x35 RW */
#define AW8624X_BIT_TRGCFG3_TRG3_POS_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG3_TRG3_POS_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG3_TRG3_POS_DISABLE			(0<<7)
#define AW8624X_BIT_TRGCFG3_TRG3SEQ_P_MASK				(~(0x7F<<0))

/* TRGCFG4: reg 0x36 RW */
#define AW8624X_BIT_TRGCFG4_TRG1_NEG_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG4_TRG1_NEG_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG4_TRG1_NEG_DISABLE			(0<<7)
#define AW8624X_BIT_TRGCFG4_TRG1SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG5: reg 0x37 RW */
#define AW8624X_BIT_TRGCFG5_TRG2_NEG_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG5_TRG2_NEG_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG5_TRG2_NEG_DISABLE			(0<<7)
#define AW8624X_BIT_TRGCFG5_TRG2SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG6: reg 0x38 RW */
#define AW8624X_BIT_TRGCFG6_TRG3_NEG_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG6_TRG3_NEG_ENABLE				(1<<7)
#define AW8624X_BIT_TRGCFG6_TRG3_NEG_DISABLE			(0<<7)
#define AW862XX_BIT_TRGCFG6_TRG3SEQ_N_MASK				(~(0x7F<<0))

/* TRGCFG7: reg 0x39 RW */
#define AW8624X_BIT_TRGCFG7_TRG1_POR_LEV_BRK_MASK		(~(7<<5))
#define AW8624X_BIT_TRGCFG7_TRG2_POR_LEV_BRK_MASK		(~(7<<1))
#define AW8624X_BIT_TRGCFG7_TRG1_POLAR_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG7_TRG1_POLAR_NEG				(1<<7)
#define AW8624X_BIT_TRGCFG7_TRG1_POLAR_POS				(0<<7)
#define AW8624X_BIT_TRGCFG7_TRG1_MODE_MASK				(~(1<<6))
#define AW8624X_BIT_TRGCFG7_TRG1_MODE_LEVEL				(1<<6)
#define AW8624X_BIT_TRGCFG7_TRG1_MODE_EDGE				(0<<6)
#define AW8624X_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK			(~(1<<5))
#define AW8624X_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE		(1<<5)
#define AW8624X_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE		(0<<5)
#define AW8624X_BIT_TRGCFG7_TRG2_POLAR_MASK				(~(1<<3))
#define AW8624X_BIT_TRGCFG7_TRG2_POLAR_NEG				(1<<3)
#define AW8624X_BIT_TRGCFG7_TRG2_POLAR_POS				(0<<3)
#define AW8624X_BIT_TRGCFG7_TRG2_MODE_MASK				(~(1<<2))
#define AW8624X_BIT_TRGCFG7_TRG2_MODE_LEVEL				(1<<2)
#define AW8624X_BIT_TRGCFG7_TRG2_MODE_EDGE				(0<<2)
#define AW8624X_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK			(~(1<<1))
#define AW8624X_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE		(1<<1)
#define AW8624X_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE		(0<<1)

/* TRGCFG8: reg 0x3A RW */
#define AW8624X_BIT_TRGCFG8_TRG3_POR_LEV_BRK_MASK		(~(7<<5))
#define AW8624X_BIT_TRGCFG8_TRG3_POLAR_MASK				(~(1<<7))
#define AW8624X_BIT_TRGCFG8_TRG3_POLAR_NEG				(1<<7)
#define AW8624X_BIT_TRGCFG8_TRG3_POLAR_POS				(0<<7)
#define AW8624X_BIT_TRGCFG8_TRG3_MODE_MASK				(~(1<<6))
#define AW8624X_BIT_TRGCFG8_TRG3_MODE_LEVEL				(1<<6)
#define AW8624X_BIT_TRGCFG8_TRG3_MODE_EDGE				(0<<6)
#define AW8624X_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK			(~(1<<5))
#define AW8624X_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE		(1<<5)
#define AW8624X_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE		(0<<5)
#define AW8624X_BIT_TRGCFG8_TRG1_STOP_MASK				(~(1<<2))
#define AW8624X_BIT_TRGCFG8_TRG1_STOP					(1<<2)
#define AW8624X_BIT_TRGCFG8_TRG2_STOP_MASK				(~(1<<1))
#define AW8624X_BIT_TRGCFG8_TRG2_STOP					(1<<1)
#define AW8624X_BIT_TRGCFG8_TRG3_STOP_MASK				(~(1<<0))
#define AW8624X_BIT_TRGCFG8_TRG3_STOP					(1<<0)

/* GLBCFG2: reg 0x3C RW */
/* START_DLY */
#define AW8624X_BIT_GLBCFG2_START_DLY_250US				(0x0C)

/* GLBCFG4: reg 0x3E RW */
#define AW8624X_BIT_GLBCFG4_GO_PRIO_MASK				(~(3<<6))
#define AW8624X_BIT_GLBCFG4_TRG3_PRIO_MASK				(~(3<<4))
#define AW8624X_BIT_GLBCFG4_TRG2_PRIO_MASK				(~(3<<2))
#define AW8624X_BIT_GLBCFG4_TRG1_PRIO_MASK				(~(3<<0))

/* SYSCTRL1: reg 0x43 RW */
#define AW8624X_BIT_SYSCTRL1_VBAT_MODE_MASK				(~(1<<7))
#define AW8624X_BIT_SYSCTRL1_VBAT_MODE_HW				(1<<7)
#define AW8624X_BIT_SYSCTRL1_VBAT_MODE_SW				(0<<7)
#define AW8624X_BIT_SYSCTRL1_INT_BRK_MASK				(~(1<<6))
#define AW8624X_BIT_SYSCTRL1_INT_BRK_ON					(1<<6)
#define AW8624X_BIT_SYSCTRL1_INT_BRK_OFF				(0<<6)
#define AW8624X_BIT_SYSCTRL1_RCK_FRE_MASK				(~(3<<4))
#define AW8624X_BIT_SYSCTRL1_RCK_FRE_127				(3<<4)
#define AW8624X_BIT_SYSCTRL1_RCK_FRE_255				(2<<4)
#define AW8624X_BIT_SYSCTRL1_RCK_FRE_374				(1<<4)
#define AW8624X_BIT_SYSCTRL1_RCK_FRE_511				(0<<4)
#define AW8624X_BIT_SYSCTRL1_RAMINIT_MASK				(~(1<<3))
#define AW8624X_BIT_SYSCTRL1_RAMINIT_ON					(1<<3)
#define AW8624X_BIT_SYSCTRL1_RAMINIT_OFF				(0<<3)
#define AW8624X_BIT_SYSCTRL1_EN_FIR_MASK				(~(1<<2))
#define AW8624X_BIT_SYSCTRL1_FIR_ENABLE					(0<<2)
#define AW8624X_BIT_SYSCTRL1_WAKE_MODE_MASK				(~(1<<1))
#define AW8624X_BIT_SYSCTRL1_WAKE_MODE_WAKEUP			(1<<1)
#define AW8624X_BIT_SYSCTRL1_WAKE_MODE_BST				(0<<1)
#define AW8624X_BIT_SYSCTRL1_RTP_CLK_MASK				(~(1<<0))
#define AW8624X_BIT_SYSCTRL1_RTP_PLL					(1<<0)
#define AW8624X_BIT_SYSCTRL1_RTP_OSC					(0<<0)

/* SYSCTRL2: reg 0x44 RW */
#define AW8624X_BIT_SYSCTRL2_WAKE_MASK					(~(1<<7))
#define AW8624X_BIT_SYSCTRL2_WAKE_ON					(1<<7)
#define AW8624X_BIT_SYSCTRL2_WAKE_OFF					(0<<7)
#define AW8624X_BIT_SYSCTRL2_STANDBY_MASK				(~(1<<6))
#define AW8624X_BIT_SYSCTRL2_STANDBY_ON					(1<<6)
#define AW8624X_BIT_SYSCTRL2_STANDBY_OFF				(0<<6)
#define AW8624X_BIT_SYSCTRL2_RTP_DLY_MASK				(~(3<<4))
#define AW8624X_BIT_SYSCTRL2_WAIT_NHZ_MASK				(~(1<<3))
#define AW8624X_BIT_SYSCTRL2_WAIT_NHZ_ENABLE			(0<<3)
#define AW8624X_BIT_SYSCTRL2_WAIT_NHZ_DISABLE			(1<<3)
#define AW8624X_BIT_SYSCTRL2_WCK_PIN_MASK				(~(1<<2))
#define AW8624X_BIT_SYSCTRL2_ENABLE_TRIG1				(1<<2)
#define AW8624X_BIT_SYSCTRL2_DISENABLE_TRIG1			(0<<2)
#define AW8624X_BIT_SYSCTRL2_WAVDAT_MODE_MASK			(~(3<<0))
#define AW8624X_BIT_SYSCTRL2_RATE						(3<<0)
#define AW8624X_BIT_SYSCTRL2_RATE_12K					(2<<0)
#define AW8624X_BIT_SYSCTRL2_RATE_48K					(1<<0)
#define AW8624X_BIT_SYSCTRL2_RATE_24K					(0<<0)

/* SYSCTRL3: reg 0x45 RW */
#define AW8624X_BIT_SYSCTRL3_TSET_MASK					(~(0x0F<<4))
#define AW8624X_BIT_SYSCTRL3_BEMF_SET_MASK				(~(0x0F<<0))

/* SYSCTRL5: reg 0x49 RW */
#define AW8624X_BIT_SYSCTRL5_INTBRK_MD_MASK				(~(1<<7))
#define AW8624X_BIT_SYSCTRL5_INTBRK_MD_ENABLE			(1<<7)
#define AW8624X_BIT_SYSCTRL5_INTBRK_MD_DISABLE			(0<<7)
#define AW8624X_BIT_SYSCTRL5_GAIN_BYPASS_MASK			(~(1<<6))
#define AW8624X_BIT_SYSCTRL5_GAIN_CHANGEABLE			(1<<6)
#define AW8624X_BIT_SYSCTRL5_GAIN_FIXED					(0<<6)
#define AW8624X_BIT_SYSCTRL5_INT_EDGE_MODE_MASK			(~(1<<5))
#define AW8624X_BIT_SYSCTRL5_INT_EDGE_MODE_POS			(0<<5)
#define AW8624X_BIT_SYSCTRL5_INT_EDGE_MODE_BOTH			(1<<5)
#define AW8624X_BIT_SYSCTRL5_INT_MODE_MASK				(~(1<<4))
#define AW8624X_BIT_SYSCTRL5_INT_MODE_EDGE				(1<<4)
#define AW8624X_BIT_SYSCTRL5_INT_MODE_LEVEL				(0<<4)
#define AW8624X_BIT_SYSCTRL5_INTP_MASK					(~(1<<3))
#define AW8624X_BIT_SYSCTRL5_INTP_HIGH					(1<<3)
#define AW8624X_BIT_SYSCTRL5_INTP_LOW					(0<<3)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_MASK				(~(7<<0))
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_1					(0<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_2					(1<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_4					(2<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_5					(3<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_8					(4<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_10				(5<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_20				(6<<0)
#define AW8624X_BIT_SYSCTRL5_D2S_GAIN_40				(7<<0)

/* SYSCTRL6: reg 0x4A RW */
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_MASK			(~(7<<0))
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_1				(0<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_2				(1<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_4				(2<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_5				(3<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_8				(4<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_10				(5<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_20				(6<<0)
#define AW8624X_BIT_SYSCTRL6_F0_D2S_GAIN_40				(7<<0)

/* PWMCFG1: reg 0x4C RW */
#define AW8624X_BIT_PWMCFG1_PRC_EN_MASK					(~(1<<7))
#define AW8624X_BIT_PWMCFG1_PRC_ENABLE					(1<<7)
#define AW8624X_BIT_PWMCFG1_PRC_DISABLE					(0<<7)
#define AW8624X_BIT_PWMCFG1_PRCTIME_MASK				(~(0x7F<<0))

/* PWMCFG3: reg 0x4E RW */
#define AW8624X_BIT_PWMCFG3_PR_EN_MASK					(~(1<<7))
#define AW8624X_BIT_PWMCFG3_PR_ENABLE					(1<<7)
#define AW8624X_BIT_PWMCFG3_PR_DISABLE					(0<<7)
#define AW8624X_BIT_PWMCFG3_PRLVL_MASK					(~(0x7F<<0))
#define AW8624X_BIT_PWMCFG3_PRLVL_DEFAULT_VALUE			(0x3F)

/* PWMCFG4: reg 0x4F RW */
/* PRTIME */
#define AW8624X_PWMCFG4_PRTIME_DEFAULT_VALUE			(0x32)

/* DET_LO: reg 0x50 RO */
#define AW8624X_BIT_DET_LO_TEST_LO_MASK					(~(3<<6))
#define AW8624X_BIT_DET_LO_VBAT_LO_MASK					(~(3<<4))
#define AW8624X_BIT_DET_LO_VBAT							(3<<4)
#define AW8624X_BIT_DET_LO_OS_MASK						(~(3<<2))
#define AW8624X_BIT_DET_LO_RL_MASK						(~(3<<0))
#define AW8624X_BIT_DET_LO_RL							(3<<0)

/* DETCFG1: reg 0x51 RW */
#define AW8624X_BIT_DETCFG1_FTS_GO_MASK					(~(1<<7))
#define AW8624X_BIT_DETCFG1_FTS_GO_ENABLE				(1<<7)
#define AW8624X_BIT_DETCFG1_TEST_GO_MASK				(~(1<<6))
#define AW8624X_BIT_DETCFG1_TEST_GO_ENABLE				(1<<6)
#define AW8624X_BIT_DETCFG1_ADO_SLOT_MODE_MASK			(~(1<<5))
#define AW8624X_BIT_DETCFG1_ADO_SLOT_ADC_32				(1<<5)
#define AW8624X_BIT_DETCFG1_ADO_SLOT_ADC_256			(0<<5)
#define AW8624X_BIT_DETCFG1_RL_OS_MASK					(~(1<<4))
#define AW8624X_BIT_DETCFG1_RL							(1<<4)
#define AW8624X_BIT_DETCFG1_OS							(0<<4)
#define AW8624X_BIT_DETCFG1_PRCT_MODE_MASK				(~(1<<3))
#define AW8624X_BIT_DETCFG1_PRCT_MODE_INVALID			(0<<3)
#define AW8624X_BIT_DETCFG1_PRCT_MODE_VALID				(1<<3)
#define AW8624X_BIT_DETCFG1_CLK_ADC_MASK				(~(7<<0))
#define AW8624X_BIT_DETCFG1_CLK_ADC_12M					(0<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_6M					(1<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_3M					(2<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_1M5					(3<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_M75					(4<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_M37					(5<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_M18					(6<<0)
#define AW8624X_BIT_DETCFG1_CLK_ADC_M09					(7<<0)

/* DETCFG2: reg 0x52 RW */
#define AW8624X_BIT_DETCFG2_VBAT_GO_MASK				(~(1<<1))
#define AW8624X_BIT_DETCFG2_VABT_GO_ON					(1<<1)
#define AW8624X_BIT_DETCFG2_DIAG_GO_MASK				(~(1<<0))
#define AW8624X_BIT_DETCFG2_DIAG_GO_ON					(1<<0)

/* TRIMCFG1: reg:0x59 RW */
#define AW8624X_BIT_TRIMCFG1_RL_TRIM_SRC_MASK			(~(1<<6))
#define AW8624X_BIT_TRIMCFG1_RL_TRIM_SRC_REG			(1<<6)
#define AW8624X_BIT_TRIMCFG1_RL_TRIM_SRC_EFUSE			(0<<6)
#define AW8624X_BIT_TRIMCFG1_TRIM_RL_MASK				(~(63<<0))

/* TRIMCFG3: reg:0x5A RW */
#define AW8624X_BIT_TRIMCFG3_OSC_TRIM_SRC_MASK			(~(1<<7))
#define AW8624X_BIT_TRIMCFG3_OSC_TRIM_SRC_REG			(1<<7)
#define AW8624X_BIT_TRIMCFG3_OSC_TRIM_SRC_EFUSE			(0<<7)
#define AW8624X_BIT_TRIMCFG3_TRIM_LRA_MASK				(~(0x7F<<0))

/* EFRD9: reg:0x64 RO */

/* ANACFG8: reg:0x77 RW */
#define AW8624X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK			(~(3<<6))
#define AW8624X_BIT_ANACFG8_TRTF_CTRL_HDRV				(3<<6)

#endif
