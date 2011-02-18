//-----------------------------------------------------------------//
// Name        | sc2_drv_struct.h            | Type: ( ) source    //
//-------------------------------------------|       (*) header    //
// Project     | SC2                         |       ( ) others    //
//-----------------------------------------------------------------//
// Platform    | WINDOWS 2000/XP                                   //
//-----------------------------------------------------------------//
// Environment | Microsoft Visual C++ 6.0                          //
//             |                                                   //
//-----------------------------------------------------------------//
// Purpose     | SC2 - header with structures                      //
//             | used from 1394.dll, cameralink.dll's              //
//             | and SC2_Cam.dll                                   //
//-----------------------------------------------------------------//
// Author      | MBL, PCO AG                                       //
//-----------------------------------------------------------------//
// Revision    | rev. 0.30 rel. 0.00                               //
//-----------------------------------------------------------------//
// Notes       |                                                   //
//             |                                                   //
//             |                                                   // 
//-----------------------------------------------------------------//
// (c) 2003 PCO AG * Donaupark 11 *                                //
// D-93309      Kelheim / Germany * Phone: +49 (0)9441 / 2005-0 *  //
// Fax: +49 (0)9441 / 2005-20 * Email: info@pco.de                 //
//-----------------------------------------------------------------//


//-----------------------------------------------------------------//
// Revision History:                                               //
//-----------------------------------------------------------------//
// Rev.:     | Date:      | Changed:                               //
// --------- | ---------- | ---------------------------------------//
//  0.30     | 17.12.2004 |  new file                              //
//           | 22.02.2005 |  new define REALSIZE                   //
//-----------------------------------------------------------------//
//  0. ...   |            |                                        //

#ifndef  SC2_DRV_STRUCT_H
#define  SC2_DRV_STRUCT_H


#define PCO_SC2_CL_ONLY_SERIAL_INIT 0x00001000
#define PCO_SC2_CL_CLSER_NAME       0x00002000
#define PCO_SC2_CL_CONFIG_NAME      0x00004000
#define PCO_SC2_CL_NO_INIT_COMMAND  0x00008000
#define PCO_SC2_CL_USE_COMPORTS     0x00000100

#define PCO_SC2_FW_REALSIZE         0x00010000

typedef struct _SC2_OpenStruct {
  char* clser_file_name;
  char* config_file_name;
  void* dummy1;
  void* dummy2;
  void* dummy3;
  uint32_t dummy[5];
} SC2_OpenStruct;


//internal message levels for dll
#define INTERNAL_1_M 0x00010000
#define INTERNAL_2_M 0x00020000
#define INTERNAL_3_M 0x00040000
#define INTERNAL_4_M 0x00080000


//defines for general bits
#define NO_START_IMAGE 0x00000001
#define SET_LSB        0x00000002
#define ME4_FAST_STOP  0x00010000

#endif
