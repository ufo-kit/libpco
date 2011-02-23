//-----------------------------------------------------------------//
// Name        | sc2_telegram.h              | Type: ( ) source    //
//-------------------------------------------|       (*) header    //
// Project     | SC2                         |       ( ) others    //
//-----------------------------------------------------------------//
// Platform    | M16C/62N as well as PC / Win32                    //
//-----------------------------------------------------------------//
// Environment | - Mitsubishi NC30 Compiler for M16C Ver 5.0 Rel 2 //
//             | - Microsoft Visual C++ 6.0                        //
//-----------------------------------------------------------------//
// Purpose     | SC2 - Packet Structure defines                    //
//-----------------------------------------------------------------//
// Author      | MBL, PCO AG                                       //
//-----------------------------------------------------------------//
// Revision    | Rev. 0.23                                         //
//-----------------------------------------------------------------//
// Notes       |                                                   //
//-----------------------------------------------------------------//
// Attention!! | Attention!! If these structures are released to   //
//             | market in any form, the position of each data     //
//             | entry must not be moved any longer! Enhancements  //
//             | can be done by exploiting the dummy entries and   //
//             | dummy fields.                                     //
//-----------------------------------------------------------------//
//   (c) 2004 PCO AG * Donaupark 11 * D-93309 Kelheim / Germany    //
//   Phone: +49 (0)9441 / 2005-0 * Fax: +49 (0)9441 / 2005-20      //
//   Email: info@pco.de                                            //
//-----------------------------------------------------------------//


//-----------------------------------------------------------------//
// Revision History:                                               //
//-----------------------------------------------------------------//
// Rev.:     | Date:      | Changed:                               //
// --------- | ---------- | ---------------------------------------//
//  0.01     | 10.06.2003 |  MBL/new file                          //
//-----------------------------------------------------------------//
//  0.02     | 26.06.2003 |  LWA: Checksum added to all telegrams  //
//           |            |       (see also comments)              //
//-----------------------------------------------------------------//
//  0.12     | xx.07.2003 |  New: REQUEST IMAGE command /LWA       //
//-----------------------------------------------------------------//
//  0.13     | 24.07.2003 |  Removed:                              //
//           |            |  - ISOCHRONOUS_START_MSG structure     //
//           |            |  - ISOCHRONOUS_STOP_MSG structure      //
//           |            |  - INTERNAL_SEND_IMAGES_MSG structure  //
//           |            |  - INTERNAL_STOP_IMAGES_MSG structure  //
//           |            |    (not used any longer)               //
//-----------------------------------------------------------------//
//  0.14     | 01.09.2003 |  Renamed: /LWA                         //
//           |            |  - SC2_Preview_Mode_Response to        //
//           |            |      SC2_Liveview_Mode_Response        //
//           |            |  - SC2_Set_Preview_Mode to             //
//           |            |      SC2_Set_Liveview_Mode             //
//           |            |  Due to renaming Preview to Live View. //
//-----------------------------------------------------------------//
//  0.15     | 01.09.2003 |  Changed /LWA                          //
//           |            |  Removed: - SC2_Image_Ready_Message    //
//           |            |  Added:   - SC2_Image_Avail_Message    //
//           |            |           - SC2_No_Image_Avail_Message //
//           |            |           - SC2_Record_On_Message      //
//           |            |           - SC2_Record_Off_Message     //
//           | 24.09.2003 |  Added /LWA                            //
//           |            |    - SC2_Load_COC_Data                 //
//           |            |    - SC2_Send_COC_Command              //
//           | 16.10.2003 |  Added /MBL                            //
//           |            |    - SC2_COC_Operation_Mode_Response   //
//           |            |    - SC2_Set_COC_Operation_Mode        //
//           |            |    - SC2_COC_Runtime_Response          //
//-----------------------------------------------------------------//
//  0.16     | 19.11.2003 |  Changed LWA                           //
//           |            |  - SC2_Camera_Description_Response     //
//           |            |    changed. Added descriptors for ROI  //
//           |            |    stepping and min. delay / exposure  //
//           |            |    time steps.                         //
//           |            |  - SC2_Camera_Health_Status_Response   //
//           |            |    Status field included               //
//-----------------------------------------------------------------//
//  0.17     | 19.11.2003 |  Added LWA                             //
//           |            |  - structures for fixed data area,     //
//           |            |    like camera serial no, hardware     //
//           |            |    version, etc.                       //
//           | 16.01.2004 |  Changed LWA                           //
//           |            |  - Hardware revison (structure         //
//           |            |    (SC2_HWREVISION) changed: now one   //
//           |            |    revision code as word instead of    //
//           |            |    two bytes wih minor and major rev.  //
//           | 03.02.2004 |  Added MBL                             //
//           |            |    Chip descriptor color_pattern,      //
//           |            |    pattern typ                         //
//           |            |                                        //
//           | 17.02.2004 |  Added structures: LWa                 //
//           |            |  - Start_DSNU_Calibration              //
//           |            |  - Start_DSNU_Calibration_Response     //
//           |            |  - Set_DSNU_Correction_Mode            //
//           |            |  - Get_DSNU_Correction_Mode_Response   //
//           |            |                                        //
//           | 26.02.2004 |  Added structures: LWa                 //
//           |            |  - SC2_Read_Head_EE_Data               //
//           |            |  - SC2_Write_Head_EE_Data              //
//           |            |  - SC2_Head_EE_Data_Response           //
//           |            |                                        //
//-----------------------------------------------------------------//
//  0.18     | 22.03.2004 |  Changed LWA                           //
//           |            |  - reorganized update failure tele-    //
//           |            |    grams such that they are similar to //
//           |            |    standard failure telegram, i.e.     //
//           |            |    failure code is dword after size.   //
//           | 02.04.2004 |  Added telegrams: /LWA                 //
//           |            |  - SC2_Get_FPS_Exposure_Mode           //
//           |            |  - SC2_Set_FPS_Exposure_Mode           //
//           |            |  - SC2_FPS_Exposure_Mode_Response      //
//           | 25.06.2004 |  Added telegrams: /FRE                 //
//           |            |  - SC2_Get_Bit_Alignment               //
//           |            |  - SC2_Bit_Alignment_Response          //
//-----------------------------------------------------------------//
//  0.19     | 01.07.2004 |  Changed LWA                           //
//           |            |  - the member dwtime_us in structure   //
//           |            |    SC2_COC_Runtime_Response changed to //
//           |            |    dwtime_ns.                          //
//           | 06.07.2004 |  Added telegrams: /MBL CameraLink      //
//           |            |  - SC2_Set_CL_Baudrate                 //
//           |            |  - SC2_Get_CL_Baudrate                 //
//           |            |  - SC2_Set_CL_Configuration            //
//           |            |  - SC2_Get_CL_Confuguration            //
//-----------------------------------------------------------------//
//  0.20     | 23.07.2004 |  LWA:  telegrams for internal use like //
//           |            |        debug and update telegrams      //
//           |            |        moved to SC2_TELEGRAM_INTERN.H  //
//           | 16.09.2004 |  LWA:  telegrams added:                //
//           |            |                                        //
//           |            |  SC2_IEEE1394_Iso_Byte_Order_Response  //
//           |            |  SC2_Get_IEEE1394_Iso_Byte_Order       //
//           |            |                                        //
//           |            |  Used for OEM camera, not for          //
//           |            |  pco.camera series                     //
//           |            |                                        //
//           | 19.10.2004 |  LWA:  telegrams added:                //
//           |            |                                        //
//           |            |  SC2_Repeat_Image                      //
//           |            |  SC2_Repeat_Image_Response             //
//           |            |    (commands sends last image again)   //
//           |            |  SC2_Cancel_Image_Transfer             //
//           |            |  SC2_Cancel_Image_Transfer_Response    //
//           |            |    (aborts running image transfer)     //
//-----------------------------------------------------------------//
//  0.21     | 11.04.2005 |  LWA:  added telegrams:                //
//           |            |  SC2_Get_Noise_Filter_Mode             //
//           |            |  SC2_Set_Noise_Filter_Mode             //
//           |            |  SC2_Noise_Filter_Mode_Response        //
//           |            |  SC2_Camera_Desription: new member     //
//           |            |    wNoiseFilterDESC                    //
//-----------------------------------------------------------------//
//  0.21     | 04.05.2005 |  LWA:  changed telegram:               //
//           |            |  SC2_Camera_Desription:                //
//           |            |    - dwGeneralCaps1 added              //
//           |            |    - wNoiseFilterDESC removed          //
//-----------------------------------------------------------------//
//  0.22     | 19.10.2005 |  MBL:  definitionen aus intern:        //
//           |            |    - SC2_Image_Transfer_Done_Message   //
//           |            |                                        //
//           | 27.02.2006 |  LWA:  added telegrams:                //
//           |            |  SC2_Camera_Name_Response              //
//           |            |                                        //
//           | 09.03.2006 |  LWA:  added telegrams:                //
//           |            |  SC2_Get_Hot_Pixel_Correction_Mode     //
//           |            |  SC2_Set_Hot_Pixel_Correction_Mode     //
//           |            |  SC2_Hot_Pixel_Correction_Mode_Response//
//-----------------------------------------------------------------//
//  0.23     | 01.06.2006 |  Preparation for modulation mode:      //
//           |   (FRe)    |  Added modulation mode parameters and  //
//           |            |  telegrams:                            //
//           |            |    do_S(G)ET_MODULATION_MODE           //
//           |            |  Added second descriptor, flags and    //
//           |            |  telegram:                             //
//           |            |    do_GET_DESCRIPTION_EX               //
//           |            |  Changed header to local               //
//           |            |  c:\pco_include\include in order to    //
//           |            |   support header file repository       //
//-----------------------------------------------------------------//
//  0.24     | 19.09.2007 | FRE:Added tel. for GET_INFO_STRING     //
//-----------------------------------------------------------------//
//  0.25     | 05.03.2009 | FRE: Added Get/SetFrameRate            //
//           |            | Added HW IO functions and desc.        //
//           | 01.07.2009 | FRE:Add. SC2_Get_Image_Timing_Response //
//-----------------------------------------------------------------//
//  0.26     | 31.08.2010 | FRE:Added tel. for Lookup Tables       //
//-----------------------------------------------------------------//

  // LWA 26.06.03: Checksum byte was added because now, if declaring
  //               a telegram variable the checksum byte is included
  //               and writing the checksum byte will not fail.
  //               For the length of the telegram now take simply the
  //               sizeof operator: Telegram.Length = sizeof(Telegram)


#ifndef  SC2_TELEGRAM_H
#define  SC2_TELEGRAM_H

#include "sc2_common.h"

#define FWREVISION 0x00009000

#ifdef WIN32
//#pragma message("Structures packed to 1 byte boundary!")
#pragma pack(push) 
#pragma pack(1)            
#endif

#ifdef __MICROBLAZE__
#define struct struct __attribute__ ((packed))
#endif

// ================================================================================== //
// ======== FireWire address space definition for telegram transmission ============= //
// ================================================================================== //


#define IEEE1394_MasterResponseAddress  0xF8000000L

  // this is the FireWire address space for the master (PC / driver) to get   //
  // and detect response telegrams.   -> cameras write telegrams to address:  //
  //           0xFFFF_0000_0000 + <IEEE1394_MasterResponseAddress>            //


#define IEEE1394_MasterInterruptAddress 0xF9000000L

  // this is the FireWire address space for the master (PC / driver) to get   //
  // and detect telegrams sent by the camera initiated by itsself (error      //
  // messages etc.  -> cameras write (interrupt) telegrams to address:        //
  //           0xFFFF_0000_0000 + <IEEE1394_MasterInterruptAddress>           //


#define IEEE1394_CameraCommandAddress   0xFA000000L

  // this is the FireWire address space for the client (pco.camera) to get and //
  // detect command telegrams.   -> master (PC) write telegrams to address:    //
  //          0xFFFF_0000_0000 + <IEEE1394_CameraCommandAddress>               //



////////////////////////////////////////////////////////////////////////////////////////
// General Telegram Prototypes                                                        //
////////////////////////////////////////////////////////////////////////////////////////

typedef struct                         // telegram header prototype
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
} SC2_Telegram_Header;


typedef struct                         // general purpose telegram, 
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bData[268];              // longest telegram (update) padded to uint32_t
} SC2_Telegram;


typedef struct                         // simple telegram with no parameters
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;
} SC2_Simple_Telegram;



////////////////////////////////////////////////////////////////////////////////////////
// Telegrams for version management (firmware & hardware)                             //
////////////////////////////////////////////////////////////////////////////////////////

typedef struct           // structure for saving HW info in device flash/eeprom etc.
{
  uint16_t  wBatchNo; 
  uint16_t  wRevision;       // revision code
  uint16_t  wVariant;        // variant
}
SC2_HWVERSION;    


typedef struct           // structure for saving FW info in device flash/eeprom etc.
{
  uint8_t  bMinorRev;       // use range 0 to  99
  uint8_t  bMajorRev;       // use range 0 to 255
  uint16_t  wVariant;        // variant
  uint8_t  bHour;           // build date of version
  uint8_t  bMinute;
  uint8_t  bDay;
  uint8_t  bMonth;
  uint16_t  wYear;
}
SC2_FWVERSION;           // length is trimmed to 32 byte


typedef struct
{
  char  szName[16];      // string with board name
  uint16_t  wBatchNo;        // production batch no
  uint16_t  wRevision;       // revision code
  uint16_t  wVariant;        // variant
}
SC2_Hardware_DESC;


typedef struct
{
  uint16_t                   wCode;          // telegram code 
  uint16_t                   wSize;          // telegram length
  uint16_t                   BoardNum;       // number of devices
  SC2_Hardware_DESC      Board[10];
  uint8_t                   bCks;           // checksum byte
}
SC2_Hardware_Versions_Response;    


typedef struct
{
  char  szName[16];      // string with device name
  uint8_t  bMinorRev;       // use range 0 to 99
  uint8_t  bMajorRev;       // use range 0 to 255
  uint16_t  wVariant;        // variant
}
SC2_Firmware_DESC;


typedef struct
{
  uint16_t                   wCode;           // telegram code 
  uint16_t                   wSize;           // telegram length
  uint16_t                   DeviceNum;       // number of devices
  SC2_Firmware_DESC      Device[10];
  uint8_t                   bCks;            // checksum byte
}
SC2_Firmware_Versions_Response;    


#define SC2_Get_Hardware_Versions SC2_Simple_Telegram
#define SC2_Get_Firmware_Versions SC2_Simple_Telegram

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bNum;                    // block enumeration (0->1...10, 1-> 11...20 ...)
  uint8_t        bCks;                    // checksum byte
}
SC2_Get_Ext_Firmware_Versions;

#define SC2_Get_Ext_Hardware_Versions SC2_Get_Ext_Firmware_Versions


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMailboxNo;              // No of mailbox
  uint8_t        bData[64];               // data
  uint8_t        bCks;                    // checksum byte
}
SC2_Write_Mailbox;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMailboxNo;              // No of mailbox
  uint8_t        bCks;                    // checksum byte
}
SC2_Write_Mailbox_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMailboxNo;              // 
  uint8_t        bCks;                    // checksum byte
}
SC2_Read_Mailbox;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMailboxNo;              // 
  uint16_t        wReadStatus;             // status of operation
  uint8_t        bData[64];               // data
  uint8_t        bCks;                    // checksum byte
}
SC2_Read_Mailbox_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
}
SC2_Get_Mailbox_Status;

typedef struct
{
  uint16_t        wCode;                    // telegram code 
  uint16_t        wSize;                    // telegram length
  uint16_t        wNumberOfMailboxes;       // number of mailboxes implemented
  uint16_t        wMailboxStatus[8];        // status of up to 8 mailboxes
  uint8_t        bCks;                     // checksum byte
}
SC2_Get_Mailbox_Status_Response;

#define NUMSETUPFLAGS 4
typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wType;                   // general type 
  uint32_t       dwSetupFlags[NUMSETUPFLAGS];// Camera setup flags
  uint8_t        bCks;                    // checksum byte
}
SC2_Set_Camera_Setup;

#define SC2_Set_Camera_Setup_Response SC2_Set_Camera_Setup
#define SC2_Get_Camera_Setup_Response SC2_Set_Camera_Setup



/////////////////////////////////////////////////////////////////////
// Interface specific Commands 
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMasterNode;             // Master Node to respond to
  uint16_t        wIsochChannel;           // Used isochronous channel
  uint16_t        wIsochPacketLen;         // Length of a single packet
  uint16_t        wIsochPacketNum;         // Number of packets per image
  uint8_t        bCks;                    // checksum byte
} SC2_IEEE1394_Interface_Params_Response;  

#define SC2_Set_IEEE1394_Interface_Params SC2_IEEE1394_Interface_Params_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_IEEE1394_Interface_Params;  


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // mode
  uint8_t        bCks;                    // checksum byte
} SC2_IEEE1394_Iso_Byte_Order_Response;  


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_IEEE1394_Iso_Byte_Order;  

#define SC2_Set_IEEE1394_Iso_Byte_Order SC2_IEEE1394_Iso_Byte_Order_Response


/////////////////////////////////////////////////////////////////////
// interface specific commands CameraLink
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwBaudrate;              // Baudrate of CameraLink serial interface
  uint8_t        bCks;                    // checksum byte
} SC2_Get_CL_Baudrate_Response;  

#define SC2_Set_CL_Baudrate SC2_Get_CL_Baudrate_Response




typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwClockFrequency;        // Pixelclock in Hz
  uint8_t        bCCline;                 // Usage of CameraLink CC1-CC4 lines 
  uint8_t        bDataFormat;             // register with Testpattern and Datasplitter switch     
  uint8_t        bTransmit;               // single or continuous transmitting images 
  uint8_t        bCks;                    // checksum byte
} SC2_Get_CL_Configuration_Response;  

#define SC2_Set_CL_Configuration SC2_Get_CL_Configuration_Response



/////////////////////////////////////////////////////////////////////
// General Control/Status
/////////////////////////////////////////////////////////////////////


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwerrmess;               // 
  uint8_t        bCks;                    // checksum byte
} SC2_Failure_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wCamType;                // Camera type
  uint16_t        wCamSubType;             // Camera sub type
  uint32_t       dwSerialNumber;          // Serial number of camera
  uint32_t       dwHWVersion;             // Hardware version number
  uint32_t       dwFWVersion;             // Firmware version number
  uint16_t        wInterfaceType;          // Interface type
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Type_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  char        szName[40];              // camera name, null terminated
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Name_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwWarnings;              // Warnings
  uint32_t       dwErrors;                // Errors
  uint32_t       dwStatus;                // Status info
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Health_Status_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Reset_Settings_To_Default_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwWarnings;              // Warnings
  uint32_t       dwErrors;                // Errors
  uint8_t        bCks;                    // checksum byte
} SC2_Initiate_Selftest_Procedure_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  int16_t       sCCDtemp;
  int16_t       sCamtemp;
  int16_t       sPStemp;
  uint8_t        bCks;                    // checksum byte
} SC2_Temperature_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwType;                  // info type
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Info_String;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  char        szName[40];              // info name, null terminated
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Info_String_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wFanMin;                 // minimum fan setting (0 = off) 
  uint16_t        wFanMax;                 // maximum fan setting (100 = standard) 
  uint16_t        wStepSize;               // step size for set value
  uint16_t        wSetValue;               // set value,
  uint16_t        wActualValue;            // actual value, set value may be overridden by camera
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Fan_Control_Status_Response;

#define SC2_Get_Fan_Control_Status SC2_Simple_Telegram

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSetValue;               // set value,
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Fan_Control_Params;

#define SC2_Set_Fan_Control_Params_Response SC2_Set_Fan_Control_Params


/////////////////////////////////////////////////////////////////////
// Image Sensor Control
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // Sizeof this struct
  uint16_t        wSensorTypeDESC;         // Sensor type
  uint16_t        wSensorSubTypeDESC;      // Sensor subtype
  uint16_t        wMaxHorzResStdDESC;      // Maxmimum horz. resolution in std.mode       (10)
  uint16_t        wMaxVertResStdDESC;      // Maxmimum vert. resolution in std.mode
  uint16_t        wMaxHorzResExtDESC;      // Maxmimum horz. resolution in ext.mode
  uint16_t        wMaxVertResExtDESC;      // Maxmimum vert. resolution in ext.mode
  uint16_t        wDynResDESC;             // Dynamic resolution of ADC in bit
  uint16_t        wMaxBinHorzDESC;         // Maxmimum horz. binning                      (20)
  uint16_t        wBinHorzSteppingDESC;    // Horz. bin. stepping (0:bin, 1:lin)
  uint16_t        wMaxBinVertDESC;         // Maxmimum vert. binning
  uint16_t        wBinVertSteppingDESC;    // Vert. bin. stepping (0:bin, 1:lin)
  uint16_t        wRoiHorStepsDESC;        // Roi hor can be changed in these steps
  uint16_t        wRoiVertStepsDESC;       // Roi vert can be changed in these steps      (30)
  uint16_t        wNumADCsDESC;            // Number of ADCs in system
  uint32_t       dwPixelRateDESC[4];      // Possible pixelrate in Hz                    (48)
  uint16_t        wConvFactDESC[4];        // Possible conversion factor in e/cnt         (56)
  uint16_t        wIRDESC;                 // IR enhancment possibility
  uint32_t       dwMinDelayDESC;          // Minimum delay time in ns                    (62)
  uint32_t       dwMaxDelayDESC;          // Maximum delay time in ms
  uint32_t       dwMinDelayStepDESC;      // Minimum delay time step in ns               (70)
  uint32_t       dwMinExposureDESC;       // Minimum exposure time in ns
  uint32_t       dwMaxExposureDESC;       // Maximum exposure time in ms
  uint32_t       dwMinExposureStepDESC;   // Minimum exposure time step in ns            (82)
  uint32_t       dwMinDelayIRDESC;        // Minimum delay time in ns IR mode
  uint32_t       dwMaxDelayIRDESC;        // Maximum delay time in ms IR mode            (90)
  uint32_t       dwMinExposureIRDESC;     // Minimum exposure time in ns IR mode
  uint32_t       dwMaxExposureIRDESC;     // Maximum exposure time in ms IR mode
  uint16_t        wTimeTableDESC;          // Timetable for exp/del possibility           (100)
  uint16_t        wDoubleImageDESC;        // Double image mode possibility
  int16_t       sMinCoolSetDESC;         // Minimum value for cooling
  int16_t       sMaxCoolSetDESC;         // Maximum value for cooling
  int16_t       sDefaultCoolSetDESC;     // Default value for cooling
  uint16_t        wPowerDownModeDESC;      // Power down mode possibility                 (110)
  uint16_t        wOffsetRegulationDESC;   // Offset regulation possibility
  uint16_t        wColorPattern;           // Color Pattern of color chip 
                                       // four nibbles (0,1,2,3) in word 
                                       //  ----------------- 
                                       //  | 3 | 2 | 1 | 0 |
                                       //  ----------------- 
                                       //   
                                       // describe row,column  2,2 2,1 1,2 1,1
                                       // 
                                       //   column1 column2
                                       //  ----------------- 
                                       //  |       |       |
                                       //  |   0   |   1   |   row1
                                       //  |       |       |
                                       //  -----------------
                                       //  |       |       |
                                       //  |   2   |   3   |   row2
                                       //  |       |       |
                                       //  -----------------
                                       // 
  uint16_t        wColorPatternTyp;        // Pattern type of color chip          
  uint16_t        wDSNUCorrectionModeDESC; // DSNU correction mode
  uint32_t       dwGeneralCaps1;          // general capabilites descr. 1 see also SC2_DEFS.h (122)
  uint32_t       dwGeneralCaps2;          // General capabilities 2
  uint32_t       dwExtSyncFrequency[2];   // lists two frequencies for external sync feature
  uint32_t       dwReserved[4];           // 32bit dummy
  uint16_t        wReserved;                                                            //(152)
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Description_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // Sizeof this struct
  uint32_t       dwMinPeriodicalTimeDESC2;// Minimum periodical time tp in (nsec)
  uint32_t       dwMaxPeriodicalTimeDESC2;// Maximum periodical time tp in (msec)        (12)
  uint32_t       dwMinPeriodicalConditionDESC2;// System imanent condition in (nsec)
                                       // tp - (td + te) must be equal or longer than
                                       // dwMinPeriodicalCondition
  uint32_t       dwMaxNumberOfExposuresDESC2; // Maximum number of exporures possible        (20)
  int32_t        lMinMonitorSignalOffsetDESC2; // Minimum monitor signal offset tm in (nsec)
                                       // if(td + tstd) > dwMinMon.)
                                       //   tm must not be longer than dwMinMon
                                       // else
                                       //   tm must not be longer than td + tstd
  uint32_t       dwMaxMonitorSignalOffsetDESC2;// Maximum -''- in (nsec)                      
  uint32_t       dwMinPeriodicalStepDESC2;// Minimum step for periodical time in (nsec)  (32)
  uint32_t       dwStartTimeDelayDESC2;   // Minimum monitor signal offset tstd in (nsec)
                                       // see condition at dwMinMonitorSignalOffset
  uint32_t       dwMinMonitorStepDESC2;   // Minimum step for monitor time in (nsec)     (40)
  uint32_t       dwMinDelayModDESC2;      // Minimum delay time for modulate mode in (nsec)
  uint32_t       dwMaxDelayModDESC2;      // Maximum delay time for modulate mode in (nsec)
  uint32_t       dwMinDelayStepDESC2;     // Minimum delay time step for modulate mode in (nsec)(52)
  uint32_t       dwMinExposureModDESC2;   // Minimum exposure time for modulate mode in (nsec)
  uint32_t       dwMaxExposureModDESC2;   // Maximum exposure time for modulate mode in (nsec)(60)
  uint32_t       dwMinExposureStepDESC2;  // Minimum exposure time step for modulate mode in (nsec)
  uint32_t       dwModulateCapsDESC2;     // Modulate capabilities descriptor
  uint32_t       dwReserved[16];                                                         //(132)
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Description_2_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wType;                   // descriptor type
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Camera_Description;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wFormat;
  uint8_t        bCks;                    // checksum byte
} SC2_Sensor_Format_Response;

#define SC2_Set_Sensor_Format SC2_Sensor_Format_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wROI_x0;
  uint16_t        wROI_y0;
  uint16_t        wROI_x1;
  uint16_t        wROI_y1;
  uint8_t        bCks;                    // checksum byte
} SC2_ROI_Response;

#define SC2_Set_ROI SC2_ROI_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wBinningx;
  uint16_t        wBinningy;
  uint8_t        bCks;                    // checksum byte
} SC2_Binning_Response;


#define SC2_Set_Binning SC2_Binning_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwPixelrate;
  uint8_t        bCks;                    // checksum byte
} SC2_Pixelrate_Response;

#define SC2_Set_Pixelrate SC2_Pixelrate_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wGain;
  uint8_t        bCks;                    // checksum byte
} SC2_Conversion_Factor_Response;

#define SC2_Set_Conversion_Factor SC2_Conversion_Factor_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Double_Image_Mode_Response;

#define SC2_Set_Double_Image_Mode SC2_Double_Image_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_ADC_Operation_Response;

#define SC2_Set_ADC_Operation SC2_ADC_Operation_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_IR_Sensitivity_Response;

#define SC2_Set_IR_Sensitivity SC2_IR_Sensitivity_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  int16_t       sTemp;
  uint8_t        bCks;                    // checksum byte
} SC2_Cooling_Setpoint_Response;

#define SC2_Set_Cooling_Setpoint SC2_Cooling_Setpoint_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Offset_Mode_Response;

#define SC2_Set_Offset_Mode SC2_Offset_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Noise_Filter_Mode_Response;

#define SC2_Set_Noise_Filter_Mode  SC2_Noise_Filter_Mode_Response
#define SC2_Get_Noise_Filter_Mode  SC2_Simple_Telegram

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wNumOfSignals;
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Num_HW_IO_Signals_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wNumSignal;              // signal to query
  uint8_t        bCks;                    // checksum byte
} SC2_Get_HW_IO_Signal_Descriptor;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  char        szSignalName[4][24];     // signal name, null terminated
  uint16_t wSignalDefinitions;             // Flags showing signal options
                                       // 0x01: Signal can be enabled/disabled
                                       // 0x02: Signal is a status (output)
                                       // Rest: future use, set to zero!
  uint16_t wSignalTypes;                   // Flags showing the selectability of signal types
                                       // 0x01: TTL
                                       // 0x02: High Level TTL
                                       // 0x04: Contact Mode
                                       // 0x08: RS485 diff.
                                       // Rest: future use, set to zero!
  uint16_t wSignalPolarity;                // Flags showing the selectability
                                       // of signal levels/transitions
                                       // 0x01: Low Level active
                                       // 0x02: High Level active
                                       // 0x04: Rising edge active
                                       // 0x08: Falling edge active
                                       // Rest: future use, set to zero!
  uint16_t wSignalFilter;                  // Flags showing the selectability of filter
                                       // settings
                                       // 0x01: Filter can be switched off (t > ~65ns)
                                       // 0x02: Filter can be switched to medium (t > ~1us)
                                       // 0x04: Filter can be switched to high (t > ~100ms)
  uint8_t        bCks;                    // checksum byte
} SC2_Get_HW_IO_Signal_Descriptor_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode; 
  uint16_t        wMul[4];                 // multiplier
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Bayer_Multiplier;

#define SC2_Get_Bayer_Multiplier           SC2_Simple_Telegram
#define SC2_Set_Bayer_Multiplier_Response  SC2_Set_Bayer_Multiplier
#define SC2_Get_Bayer_Multiplier_Response  SC2_Set_Bayer_Multiplier


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  char        szCCM[3][3][8];          // matrix elements
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Color_Correction_Matrix_Response;

#define SC2_Get_Color_Correction_Matrix  SC2_Simple_Telegram


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // DSNU operating mode
  uint16_t        wRsrvd;                  // RFU
  uint8_t        bCks;                    // checksum byte
} SC2_Set_DSNU_Adjust_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_DSNU_Adjust_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // DSNU operating mode
  uint16_t        wRsrvd;                  // RFU
  uint8_t        bCks;                    // checksum byte
} SC2_Get_DSNU_Adjust_Mode_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // DSNU init mode
  uint16_t        wRsrvd;                  // RFU
  uint8_t        bCks;                    // checksum byte
} SC2_Init_DSNU_Adjustment;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // DSNU init mode
  uint16_t        wRsrvd;                  // RFU
  uint8_t        bCks;                    // checksum byte
} SC2_Init_DSNU_Adjustment_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // CDI operating mode
  uint16_t        wRsrvd;                  // RFU
  uint8_t        bCks;                    // checksum byte
} SC2_Set_CDI_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_CDI_Mode;

#define SC2_Get_CDI_Mode_Response SC2_Set_CDI_Mode

/////////////////////////////////////////////////////////////////////
// hot pixel commands, belongs to sensor related commands
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // mode
  uint8_t        bCks;                    // checksum byte
} SC2_Hot_Pixel_Correction_Mode_Response;

#define SC2_Set_Hot_Pixel_Correction_Mode  SC2_Hot_Pixel_Correction_Mode_Response
#define SC2_Get_Hot_Pixel_Correction_Mode  SC2_Simple_Telegram


typedef struct
{
  uint16_t        wX;                   // telegram code 
  uint16_t        wY;                   // telegram length
}
SC2_HOTPIX;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint16_t        wIndex;                  // index of this telegram (0 = first)
  uint16_t        wNumValid;               // number of valid pixels in total!
  SC2_HOTPIX  strHotPix[64];           // array of 64 Hot Pixels
  uint8_t        bCks;                    // checksum
} SC2_WRITE_HOT_PIXEL_LIST;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint16_t        wIndex;                  // no. of this telegram (0 = first)
  uint8_t        bCks;                    // checksum
} SC2_WRITE_HOT_PIXEL_LIST_RESPONSE;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint16_t        wIndex;                  // index of this telegram (0 = first)
  uint8_t        bCks;                    // checksum
} SC2_READ_HOT_PIXEL_LIST;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint16_t        wIndex;                  // index of this telegram (0 = first)
  uint16_t        wNumValid;               // number of valid pixels in total!
  uint16_t        wNumMax;                 // max. number of pixels for this list
  SC2_HOTPIX  strHotPix[64];           // array of 64 Hot Pixels
  uint8_t        bCks;                    // checksum
} SC2_READ_HOT_PIXEL_LIST_RESPONSE;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint32_t       dwMagic1;                // Magic1 and Magic2 for prohibiting
  uint32_t       dwMagic2;                //   unauthorized or accidental clearing
  uint8_t        bCks;                    // checksum
} SC2_CLEAR_HOT_PIXEL_LIST;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wListNo;                 // list no to access several lists
  uint8_t        bCks;                    // checksum
} SC2_CLEAR_HOT_PIXEL_LIST_RESPONSE;

/////////////////////////////////////////////////////////////////////
// lookup table commands, belongs to sensor related commands (edge)
/////////////////////////////////////////////////////////////////////

#define SC2_Get_Lookuptable_Info SC2_Simple_Telegram

typedef struct
{
  char        Description[20];         // e.g. "HD/SDI 12 to 10"
  uint16_t        wIdentifier;             // define loadable LUTs, range 0x0001 to 0xFFFF
  uint8_t        bInputWidth;             // maximal Input in Bits
  uint8_t        bOutputWidth;            // maximal Output in Bits
  uint16_t        wFormat;                 // accepted data structures (see defines)
}
SC2_LUT_DESC;


typedef struct
{
  uint16_t          wCode;                   // telegram code
  uint16_t          wSize;                   // telegram length
  uint16_t          wLutNumber;              // number of LUTs
  SC2_LUT_DESC  LutDesc[10];
  uint8_t          bCks;                    // checksum byte
} SC2_Get_Lookuptable_Info_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code
  uint16_t        wSize;                   // telegram length
  uint16_t        wIdentifier;             // define LUT to be activated, 0x0000 for no LUT
  uint16_t        wParameter;              // optional parameter
  uint8_t        bCks;
} SC2_Set_Lookuptable;

#define SC2_Set_Lookuptable_Response SC2_Set_Lookuptable
#define SC2_Get_Lookuptable          SC2_Simple_Telegram
#define SC2_Get_Lookuptable_Response SC2_Set_Lookuptable


typedef struct
{
  uint16_t        wCode;                   // telegram code
  uint16_t        wSize;                   // telegram length
  uint16_t        wIdentifier;             // define LUT to be loaded
  uint16_t        wPacketNum;              // for loading LUT in several steps
  uint16_t        wFormat;                 // data structure in *bData (see defines)
  uint16_t        wLength;                 // valid number of bytes within this call
  uint8_t        bData[256];
  uint8_t        bCks;                    // checksum byte
} SC2_Load_Lookuptable;

//  NOTE:
//    no additional parameters as max. telegram length is already reached!


typedef struct
{
  uint16_t        wCode;                   // telegram code
  uint16_t        wSize;                   // telegram length
  uint16_t        wIdentifier;             // define LUT to be loaded
  uint16_t        wPacketNum;              // for loading LUT in several steps
  uint16_t        wFormat;                 // data structure in *bData (see defines)
  uint16_t        wLength;                 // valid number of bytes within this call
  uint8_t        bCks;                    // checksum byte
} SC2_Load_Lookuptable_Response;

/////////////////////////////////////////////////////////////////////
// timing control 
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wTimebaseDelay;
  uint16_t        wTimebaseExposure;
  uint8_t        bCks;                    // checksum byte
} SC2_Timebase_Response;

#define SC2_Set_Timebase SC2_Timebase_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwDelay;
  uint32_t       dwExposure;
  uint8_t        bCks;                    // checksum byte
} SC2_Delay_Exposure_Response;

#define SC2_Set_Delay_Exposure SC2_Delay_Exposure_Response


typedef struct 
{
  uint32_t       dwDelay;
  uint32_t       dwExposure;
} SC2_Delay_Exposure_pair;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  SC2_Delay_Exposure_pair delexp[16];
  uint8_t        bCks;                    // checksum byte
} SC2_Delay_Exposure_Table_Response;

#define SC2_Set_Delay_Exposure_Table SC2_Delay_Exposure_Table_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // mode to be set
  uint8_t        bCks;                    // checksum byte
} SC2_Set_FPS_Exposure_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // current mode
  uint32_t       dwExposure;
  uint8_t        bCks;                    // checksum byte
} SC2_FPS_Exposure_Mode_Response;

#define SC2_Get_FPS_Exposure_Mode SC2_Simple_Telegram


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Framerate;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;                 // return status of set operation
  uint32_t       dwFramerate;             // requested fram rate in mHz
  uint32_t       dwExposure;              // requested exposure time in ns
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Framerate_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // mode to be set
  uint32_t       dwFramerate;             // requested fram rate in mHz
  uint32_t       dwExposure;              // requested exposure time in ns
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Framerate;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;                 // return status of set operation
  uint32_t       dwFramerate;             // requested fram rate in mHz
  uint32_t       dwExposure;              // requested exposure time in ns
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Framerate_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwtime_s;                  
  uint32_t       dwtime_ns;
  uint8_t        bCks;                    // checksum byte
}SC2_COC_Runtime_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwtime_s;                  
  uint32_t       dwtime_ns;
  uint8_t        bCks;                    // checksum byte
}SC2_COC_Exptime_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Trigger_Mode_Response;

#define SC2_Set_Trigger_Mode SC2_Trigger_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Sync_Mode_Response;

#define SC2_Set_Camera_Sync_Mode SC2_Camera_Sync_Mode_Response
#define SC2_Get_Camera_Sync_Mode SC2_Simple_Telegram


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wReturn;
  uint8_t        bCks;                    // checksum byte
} SC2_Force_Trigger_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // PIV mode 0 = off, 1 = on, others RFU
  uint16_t        wReserved[4];            // RFU, set to 0
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Fast_Timing_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // PIV mode 0 = off, 1 = on, others RFU
  uint16_t        wReserved[4];            // RFU, set to 0
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Fast_Timing_Mode_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Fast_Timing_Mode;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // PIV mode 0 = off, 1 = on, others RFU
  uint16_t        wReserved[4];            // RFU, set to 0
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Fast_Timing_Mode_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Busy_Status_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code
  uint16_t        wSize;                   // telegram length
  uint32_t       dwStatus;                // Status signal flags
  uint32_t       dwImageCount;            // Image count of last finished image
  uint32_t       dwReserved1;             // for future use, set to zero
  uint32_t       dwReserved2;             // for future use, set to zero
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_Sensor_Signal_Status_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wPdnMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Power_Down_Mode_Response;

#define SC2_Set_Power_Down_Mode SC2_Power_Down_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwPdnTime;
  uint8_t        bCks;                    // checksum byte
} SC2_User_Power_Down_Time_Response;

#define SC2_Set_User_Power_Down_Time SC2_User_Power_Down_Time_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;
  uint8_t        bCks;                    // checksum byte
} SC2_ExpTrig_Signal_Status_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wModulationMode;         // Mode for modulation (0 = modulation off, 1 = modulation on)
  uint32_t       dwPeriodicalTime;        // Periodical time (unit depending on timebase) for modulation
  uint16_t        wTimebasePeriodical;     // timebase for periodical time for modulation  0 -> ns, 1 -> �s, 2 -> ms
  uint32_t       dwNumberOfExposures;     // Number of exposures during modulation
  int32_t        lMonitorOffset;          // Monitor offset value
  uint8_t        bCks;                    // checksum byte
} SC2_Modulation_Mode_Response;

#define SC2_Set_Modulation_Mode SC2_Modulation_Mode_Response

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wNumSignal;              // signal to query
  uint8_t        bCks;                    // checksum byte
} SC2_Get_HW_IO_Signal;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wEnabled;                // Flag shows enable state of the signal (0: off, 1: on)
  uint16_t        wType;                   // Selected signal type
  uint16_t        wPolarity;               // Selected signal polarity
  uint16_t        wFilterSetting;          // Selected signal filter
  uint16_t        wSelected;               // Select signal
  uint8_t        bCks;                    // checksum byte
} SC2_Get_HW_IO_Signal_Response;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wNumSignal;              // signal to query
  uint16_t        wEnabled;                // Flag shows enable state of the signal (0: off, 1: on)
  uint16_t        wType;                   // Selected signal type
  uint16_t        wPolarity;               // Selected signal polarity
  uint16_t        wFilterSetting;          // Selected signal filter
  uint16_t        wSelected;               // Select signal
  uint8_t        bCks;                    // checksum byte
} SC2_Set_HW_IO_Signal;

#define SC2_Set_HW_IO_Signal_Response SC2_Set_HW_IO_Signal 

typedef struct

{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       FrameTime_ns;            // Frametime replaces COC_Runtime
  uint32_t       FrameTime_s;   
  uint32_t       ExposureTime_ns;
  uint32_t       ExposureTime_s;
  uint32_t       TriggerSystemDelay_ns;   // System internal min. trigger delay
  uint32_t       TriggerSystemJitter_ns;  // Max. possible trigger jitter -0/+ ... ns
  uint32_t       TriggerDelay_ns;         // Resulting trigger delay = system delay
  uint32_t       TriggerDelay_s;          // + delay of SetDelayExposureTime ...
  uint32_t       Reserved[4];
} SC2_Get_Image_Timing_Response;

/////////////////////////////////////////////////////////////////////
// storage control
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwRamSize;
  uint16_t        wPageSize;
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_RAM_Size_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint32_t       dwSegment1Size;
  uint32_t       dwSegment2Size;
  uint32_t       dwSegment3Size;
  uint32_t       dwSegment4Size;
  uint8_t        bCks;                    // checksum byte
} SC2_Camera_RAM_Segment_Size_Response;

#define SC2_Set_Camera_RAM_Segment_Size SC2_Camera_RAM_Segment_Size_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Clear_RAM_Segment_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint8_t        bCks;                    // checksum byte
} SC2_Active_RAM_Segment_Response;

#define SC2_Set_Active_RAM_Segment SC2_Active_RAM_Segment_Response



/////////////////////////////////////////////////////////////////////
// recording control
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Storage_Mode_Response;

#define SC2_Set_Storage_Mode SC2_Storage_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Recorder_Submode_Response;

#define SC2_Set_Recorder_Submode SC2_Recorder_Submode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wState;
  uint8_t        bCks;                    // checksum byte
} SC2_Recording_State_Response;

#define SC2_Set_Recording_State SC2_Recording_State_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Arm_Camera_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Acquire_Mode_Response;

#define SC2_Set_Acquire_Mode SC2_Acquire_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;
  uint8_t        bCks;                    // checksum byte
} SC2_acqenbl_Signal_Status_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bDay;
  uint8_t        bMonth;
  uint16_t        wYear;
  uint16_t        wHours;
  uint8_t        bMinutes;
  uint8_t        bSeconds;
  uint8_t        bCks;                    // checksum byte
} SC2_Date_Time_Response;

#define SC2_Set_Date_Time SC2_Date_Time_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;
  uint8_t        bCks;                    // checksum byte
} SC2_Timestamp_Mode_Response;

#define SC2_Set_Timestamp_Mode SC2_Timestamp_Mode_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wMode;                   // 0 = 
  uint32_t       dwDelayImages;           //   
  uint8_t        bCks;                    // checksum byte
} SC2_Record_Stop_Event_Response;

#define SC2_Set_Record_Stop_Event SC2_Record_Stop_Event_Response 
#define SC2_Get_Record_Stop_Event SC2_Simple_Telegram


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wReserved0;              // rfu, set to 0
  uint32_t       dwReserved1;             // rfu, set to 0  
  uint8_t        bCks;                    // checksum byte
} SC2_Stop_Record_Response;

#define SC2_Stop_Record SC2_Stop_Record_Response 


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wReserved;               // reserved for future use  
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Event_Monitor_Configuration;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wConfig;                 // 0 = 
  uint16_t        wReserved0;              //   
  uint16_t        wReserved1;              //   
  uint16_t        wReserved2;              //   
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Event_Monitor_Configuration;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wConfig;                 // 0 = 
  uint16_t        wReserved0;              //   
  uint16_t        wReserved1;              //   
  uint16_t        wReserved2;              //   
  uint8_t        bCks;                    // checksum byte
} SC2_Event_Monitor_Configuration_Response;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wIndex;                  // index for reading a complete list
  uint16_t        wReserved;               //   
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Event_List;


typedef struct
{
  uint32_t  dwImageNumber;              // Number of Image (same as for ReadImage command!)
  uint8_t   bEVENT_TIME_US_BCD[3];      // 0x000000 to 0x999999, where first byte is least significant byte
  uint8_t   bEVENT_TIME_SEC_BCD;        // 0x00 to 0x59
  uint8_t   bEVENT_TIME_MIN_BCD;        // 0x00 to 0x59
  uint8_t   bEVENT_TIME_HOUR_BCD;       // 0x00 to 0x23
  uint8_t   bEVENT_TIME_DAY_BCD;        // 0x01 to 0x31  
  uint8_t   bEVENT_TIME_MON_BCD;        // 0x01 to 0x12
  uint8_t   bEVENT_TIME_YEAR_BCD;       // 0x00 to 0x99 only last two digits, 2000 has to be added
  uint8_t   bFiller;                    
  uint16_t   wEventMask;                 // which events occured, see also Mode parameter of  Set Event
} SC2_EVENT_LIST_ENTRY;              //   Monitor Configuration


typedef struct
{
  uint16_t        wCode;                    // telegram code 
  uint16_t        wSize;                    // telegram length
  uint16_t        wIndex;                   // index for reading a complete list
  uint16_t        wMaxEvents;               // max. size of event list (entries) (Camera feature)
  uint16_t        wValidEvents;             // current number of events in list (total)
  uint16_t        wValidEventsInTelegram;   // valid number of events in this response telegram! 

  SC2_EVENT_LIST_ENTRY   strEvent[8];  // 8 events per call

  uint8_t        bCks;                    // checksum byte
} SC2_Get_Event_List_Response;


/////////////////////////////////////////////////////////////////////
// image read commands
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint16_t        wRes_hor;
  uint16_t        wRes_ver;
  uint16_t        wBin_x;
  uint16_t        wBin_y;
  uint16_t        wRoi_x0;
  uint16_t        wRoi_y0;
  uint16_t        wRoi_x1;
  uint16_t        wRoi_y1;
  uint8_t        bCks;                    // checksum byte
} SC2_Segment_Image_Settings_Response;

// Note: was formerly: SC2_Image_Settings_related_to_Segment_Response;


typedef struct                        // Structure for telegram
{
  uint16_t        wCode;                // command code
  uint16_t        wSize;                 // length of command
  uint16_t        wSegment;
  uint8_t        bCks;                    // checksum byte
} SC2_Segment_Image_Settings;


// Note: was formerly: SC2_Image_Settings_related_to_Segment;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint32_t       dwValid;
  uint32_t       dwMax;
  uint8_t        bCks;                    // checksum byte
} SC2_Number_of_Images_Response;


typedef struct                         // Structure for telegram
{
  uint16_t        wCode;                   // command code
  uint16_t        wSize;                   // length of command
  uint16_t        wSegment;
  uint8_t        bCks;                    // checksum byte
} SC2_Number_of_Images;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint32_t       dwStartImage; 
  uint32_t       dwLastImage; 
  uint8_t        bCks;                    // checksum byte
} SC2_Read_Images_from_Segment_Response;

#define SC2_Read_Images_from_Segment SC2_Read_Images_from_Segment_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Request_Image_Response;

#define SC2_Request_Image SC2_Request_Image_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        Reserved[4];             // reserved for future use, set to 0!
  uint8_t        bCks;                    // checksum byte
} SC2_Repeat_Image_Response;

#define SC2_Repeat_Image SC2_Repeat_Image_Response


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        Reserved;                // reserved for future use, set to 0!
  uint8_t        bCks;                    // checksum byte
} SC2_Cancel_Image_Transfer_Response;

#define SC2_Cancel_Image_Transfer SC2_Cancel_Image_Transfer_Response


typedef struct
{
  uint16_t        wCode;               // command code
  uint16_t        wSize;               // length of command
  uint16_t        wAlignment;          // bit alignment
  uint8_t        bCks;                // checksum byte
} SC2_Bit_Alignment_Response;

#define SC2_Set_Bit_Alignment SC2_Bit_Alignment_Response 
#define SC2_Get_Bit_Alignment SC2_Simple_Telegram


typedef struct
{
  uint16_t        wCode;               // command code
  uint16_t        wSize;               // length of command
  uint8_t        bCks;                // checksum byte
} SC2_Get_Metadata_Mode;


typedef struct
{
  uint16_t        wCode;               // command code
  uint16_t        wSize;               // length of command
  uint16_t        wMode;               // mode 0x0000 = w/o, 0x0001 w. metadata
  uint16_t        wReserved1;          // RFU, set to 0x0000
  uint16_t        wReserved2;          // RFU, set to 0x0000
  uint8_t        bCks;                // checksum byte
} SC2_Set_Metadata_Mode;


typedef struct
{
  uint16_t        wCode;               // command code
  uint16_t        wSize;               // length of command
  uint16_t        wMode;               // mode 0x0000 = w/o, 0x0001 w. metadata
  uint16_t        wReserved1;          // RFU, set to 0x0000
  uint16_t        wReserved2;          // RFU, set to 0x0000
  uint16_t        wMetadataSize;       // size in byte = size in pixel
  uint16_t        wMetadataVersion;    // version of the metadata
  uint8_t        bCks;                // checksum byte
} SC2_Metadata_Mode_Response;


#define PCO_METADATA_VERSION         0x0001     // current version!


/////////////////////////////////////////////////////////////////////
// Telegram structures for HD/SDI image transfer
/////////////////////////////////////////////////////////////////////

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint16_t        wInterface;
  uint16_t        wMode;
  uint16_t        wSpeed; 
  uint32_t       dwRangeLow; 
  uint32_t       dwRangeHigh;
  uint32_t       dwStartPos;
  uint8_t        bCks;                    // checksum byte
} SC2_Play_Images_from_Segment;

#define SC2_Play_Images_from_Segment_Response SC2_Play_Images_from_Segment


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Play_Position;


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wStatus;                 // current status
  uint32_t       dwPosition;              // current play position
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Play_Position_Response;	


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wSegment;
  uint16_t        wMode1;
  uint16_t        wMode2;
  uint32_t       dwSetPos1; 
  uint32_t       dwClrPos1; 
  uint32_t       dwSetPos2; 
  uint32_t       dwClrPos2; 
  uint32_t       dwSetPos3; 
  uint32_t       dwClrPos3; 
  uint32_t       dwSetPos4; 
  uint32_t       dwClrPos4; 
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Video_Payload_Identifier;

#define SC2_Set_Video_Payload_Identifier_Response SC2_Set_Video_Payload_Identifier


typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wInterface;
  uint8_t        bCks;                    // checksum byte
} SC2_Get_Interface_Output_Format;

typedef struct
{
  uint16_t        wCode;                   // telegram code 
  uint16_t        wSize;                   // telegram length
  uint16_t        wInterface;
  uint16_t        wFormat;
  uint16_t        wReserved1; 
  uint16_t        wReserved2; 
  uint8_t        bCks;                    // checksum byte
} SC2_Set_Interface_Output_Format;

#define SC2_Set_Interface_Output_Format_Response SC2_Set_Interface_Output_Format
#define SC2_Get_Interface_Output_Format_Response SC2_Set_Interface_Output_Format



/////////////////////////////////////////////////////////////////////
// "Interrupt" Messages sent from camera to PC
/////////////////////////////////////////////////////////////////////

//#define SC2_Image_Avail_Message           SC2_Simple_Telegram
//#define SC2_No_Image_Avail_Message        SC2_Simple_Telegram
//#define SC2_Record_Off_Message            SC2_Simple_Telegram

#define SC2_Image_Transfer_Done_Message   SC2_Simple_Telegram




#ifdef WIN32
//#pragma message("Structures packed back to normal!")
#pragma pack(pop)  
#endif

#ifdef __MICROBLAZE__
#undef struct
#endif

#endif  // -- ifndef SC2_TELEGRAM_H --------------------------------------- //

