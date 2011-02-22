//-----------------------------------------------------------------//
// Name        | reorder_func.cpp            | Type: (*) source    //
//-------------------------------------------|       ( ) header    //
// Project     | pco.camera                  |       ( ) others    //
//-----------------------------------------------------------------//
// Platform    | WINDOWS 2000/XP                                   //
//-----------------------------------------------------------------//
// Environment | Microsoft Visual C++                              //
//             |                                                   //
//-----------------------------------------------------------------//
// Purpose     | pco.camera - edge                                 //
//-----------------------------------------------------------------//
// Author      | MBL, PCO AG                                       //
//-----------------------------------------------------------------//
// Revision    | rev. 1.01 rel. 0.00                               //
//-----------------------------------------------------------------//
// Notes       | functions for reordering image data               //
//             |                                                   //
//             |                                                   //
//-----------------------------------------------------------------//
// (c) 2010 PCO AG * Donaupark 11 *                                //
// D-93309      Kelheim / Germany * Phone: +49 (0)9441 / 2005-0 *  //
// Fax: +49 (0)9441 / 2005-20 * Email: info@pco.de                 //
//-----------------------------------------------------------------//

//#include "pco_cl_include.h"
//#include "../PCO_Include/SC2_SDKAddendum.h"
//#include "../PCO_Include/pco_err.h"
#include <stdint.h>
#include <string.h>
#include "sc2_add.h"


void extract_image_me4(void *bufout,void *bufin,int act_width,int act_height)
{

  int x,y,off;
//  uint32_t *picadr_in;
//  uint32_t *picadr_out;
  uint32_t *lineadr_in;
  uint32_t *lineadr_out;
  uint32_t a;

//  picadr_in=(uint32_t *)bufadr;
//  picadr_out=(uint32_t *)bufent->bufadr;
  off=(act_width*12)/32;
  lineadr_in=(uint32_t *)bufin; //picadr_in;//+y*off;
  lineadr_out=(uint32_t *)bufout;//picadr_out;//+y*entry->act_width;

  for(y=0;y<act_height;y++)
  {
   //          lineadr_in=picadr_in+y*off;
   //          lineadr_out=picadr_out+y*entry->act_width;
   for(x=0;x<off;)
   {
    a = (*lineadr_in&0x0000FFF0)>>4;
    a|= (*lineadr_in&0x0000000F)<<24;
    a|= (*lineadr_in&0xFF000000)>>8;
    *lineadr_out=a;
    lineadr_out++;

    a = (*lineadr_in&0x00FF0000)>>12;
    lineadr_in++;
    x++;
    a|= (*lineadr_in&0x0000F000)>>12;
    a|= (*lineadr_in&0x00000FFF)<<16;
    *lineadr_out=a;
    lineadr_out++;
    a = (*lineadr_in&0xFFF00000)>>20;
    a|= (*lineadr_in&0x000F0000)<<8;
    lineadr_in++;
    x++;
    a|= (*lineadr_in&0x0000FF00)<<8;
    *lineadr_out=a;
    lineadr_out++;
    a = (*lineadr_in&0x000000FF)<<4;
    a|= (*lineadr_in&0xF0000000)>>28;
    a|= (*lineadr_in&0x0FFF0000);
    *lineadr_out=a;
    lineadr_out++;
    lineadr_in++;
    x++;
   }
  }
}



void Convert_A(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;

  line_1=(unsigned short*)adr_out;
  line_n=(unsigned short*)adr_out;
  line_n+=(height-1)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   memcpy(line_1,line_in,width*sizeof(unsigned short));
   line_in+=width;
   memcpy(line_n,line_in,width*sizeof(unsigned short));
   line_in+=width;

   line_1+=width;
   line_n-=width;
  }
}

void Convert_B(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;

  line_1=(unsigned short*)adr_out;
  line_1+=(height/2-1)*width;
  line_n=(unsigned short*)adr_out;
  line_n+=(height/2)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   memcpy(line_1,line_in,width*sizeof(unsigned short));
   line_in+=width;
   memcpy(line_n,line_in,width*sizeof(unsigned short));
   line_in+=width;

   line_1-=width;
   line_n+=width;
  }

}

void Convert_C(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;


  line_1=(unsigned short*)adr_out;
  line_1+=(height/2-1)*width;
  line_n=(unsigned short*)adr_out;
  line_n+=(height-1)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   memcpy(line_1,line_in,width*sizeof(unsigned short));
   line_in+=width;
   memcpy(line_n,line_in,width*sizeof(unsigned short));
   line_in+=width;

   line_1-=width;
   line_n-=width;
  }
}

void Convert_D(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;

  line_1=(unsigned short*)adr_out;
  line_n=(unsigned short*)adr_out;
  line_n+=(height/2)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   memcpy(line_1,line_in,width*sizeof(unsigned short));
   line_in+=width;
   memcpy(line_n,line_in,width*sizeof(unsigned short));
   line_in+=width;

   line_1+=width;
   line_n+=width;
  }
}

void Extract_Line(int width,void *bufout,void* bufin)
{
  int x,off;
  uint32_t *lineadr_in;
  uint32_t *lineadr_out;
  uint32_t a;

  off=(width*12)/32;
  lineadr_in=(uint32_t *)bufin;
  lineadr_out=(uint32_t *)bufout;

   for(x=0;x<off;)
   {
    a = (*lineadr_in&0x0000FFF0)>>4;
    a|= (*lineadr_in&0x0000000F)<<24;
    a|= (*lineadr_in&0xFF000000)>>8;
    *lineadr_out=a;
    lineadr_out++;

    a = (*lineadr_in&0x00FF0000)>>12;
    lineadr_in++;
    x++;
    a|= (*lineadr_in&0x0000F000)>>12;
    a|= (*lineadr_in&0x00000FFF)<<16;
    *lineadr_out=a;
    lineadr_out++;
    a = (*lineadr_in&0xFFF00000)>>20;
    a|= (*lineadr_in&0x000F0000)<<8;
    lineadr_in++;
    x++;
    a|= (*lineadr_in&0x0000FF00)<<8;
    *lineadr_out=a;
    lineadr_out++;
    a = (*lineadr_in&0x000000FF)<<4;
    a|= (*lineadr_in&0xF0000000)>>28;
    a|= (*lineadr_in&0x0FFF0000);
    *lineadr_out=a;
    lineadr_out++;
    lineadr_in++;
    x++;
   }
}


void Extract_A(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;
  int off=(width*12)/16;

  line_1=(unsigned short*)adr_out;
  line_n=(unsigned short*)adr_out;
  line_n+=(height-1)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   Extract_Line(width,line_1,line_in);
   line_in+=off;
   Extract_Line(width,line_n,line_in);
   line_in+=off;

   line_1+=width;
   line_n-=width;
  }
}

void Extract_B(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;
  int off=(width*12)/16;

  line_1=(unsigned short*)adr_out;
  line_1+=(height/2-1)*width;
  line_n=(unsigned short*)adr_out;
  line_n+=(height/2)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   Extract_Line(width,line_1,line_in);
   line_in+=off;
   Extract_Line(width,line_n,line_in);
   line_in+=off;

   line_1-=width;
   line_n+=width;
  }

}

void Extract_C(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;
  int off=(width*12)/16;


  line_1=(unsigned short*)adr_out;
  line_1+=(height/2-1)*width;
  line_n=(unsigned short*)adr_out;
  line_n+=(height-1)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   Extract_Line(width,line_1,line_in);
   line_in+=off;
   Extract_Line(width,line_n,line_in);
   line_in+=off;

   line_1-=width;
   line_n-=width;
  }
}

void Extract_D(int width,int height,void *adr_out,void* adr_in)
{
  unsigned short *line_1;
  unsigned short *line_n;
  unsigned short *line_in;
  int off=(width*12)/16;

  line_1=(unsigned short*)adr_out;
  line_n=(unsigned short*)adr_out;
  line_n+=(height/2)*width;
  line_in=(unsigned short*)adr_in;

  for(int y=0;y<height/2;y++)
  {
   Extract_Line(width,line_1,line_in);
   line_in+=off;
   Extract_Line(width,line_n,line_in);
   line_in+=off;

   line_1+=width;
   line_n+=width;
  }
}


void extract_image(void *bufout,void *bufin,int act_width,int act_height,int DataFormat)
{
  if((DataFormat&PCO_CL_DATAFORMAT_MASK)!=PCO_CL_DATAFORMAT_5x12)
   return;

  if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
   Extract_A(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
   Extract_B(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
   Extract_C(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
   Extract_D(act_width,act_height,bufout,bufin);
}

void sort_image(void *bufout,void *bufin,int act_width,int act_height,int DataFormat)
{
  if((DataFormat&PCO_CL_DATAFORMAT_MASK)!=PCO_CL_DATAFORMAT_5x16)
   return;

  if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
   Convert_A(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
   Convert_B(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
   Convert_C(act_width,act_height,bufout,bufin);
  else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
   Convert_D(act_width,act_height,bufout,bufin);
}

//size of bufin must be the size of the full image buffer
//size of bufout must be the size of the full image buffer
//act_width is actual image width in pixel
//act_height is actual image height in lines
//DataFormat the actual DataFormat of camera and grabber
void reorder_image(void *bufout,void *bufin,int act_width,int act_height,int DataFormat)
{
  if((DataFormat&PCO_CL_DATAFORMAT_MASK)==PCO_CL_DATAFORMAT_5x12)
  {
   if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
    Extract_A(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
    Extract_B(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
    Extract_C(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
    Extract_D(act_width,act_height,bufout,bufin);
  }
  else if((DataFormat&PCO_CL_DATAFORMAT_MASK)==PCO_CL_DATAFORMAT_5x16)
  {
   if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
    Convert_A(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
    Convert_B(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
    Convert_C(act_width,act_height,bufout,bufin);
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
    Convert_D(act_width,act_height,bufout,bufin);
  }
  return;
}

//linenumber start from 1 to act_height
//size of bufin must be the size of the full image buffer
//size of bufout can be only the size of one line
//act_width is actual image width in pixel
//act_height is actual image height in lines
//DataFormat the actual DataFormat of camera and grabber
void get_image_line(void *bufout,void *bufin,int line_number,int act_width,int act_height,int DataFormat)
{
  unsigned short *adr;
  int off;
  line_number-=1;
  if((DataFormat&PCO_CL_DATAFORMAT_MASK)==PCO_CL_DATAFORMAT_5x12)
  {
   off=(act_width*12)/16;
   adr=(unsigned short*)bufin;
   if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=line_number*off*2;
    }
    else
    {
     adr+=off;
     adr+=(act_height-line_number-1)*off*2;
    }
    Extract_Line(act_width,bufout,adr);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=(act_height/2-line_number-1)*off*2;
    }
    else
    {
     adr+=off;
     adr+=line_number*off*2;
    }
    Extract_Line(act_width,bufout,adr);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=(act_height/2-line_number-1)*off*2;
    }
    else
    {
     adr+=off;
     adr+=(act_height-line_number-1)*off*2;
    }
    Extract_Line(act_width,bufout,adr);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=line_number*off*2;
    }
    else
    {
     adr+=off;
     adr+=line_number*off*2;
    }
    Extract_Line(act_width,bufout,adr);
   }
  }
  else if((DataFormat&PCO_CL_DATAFORMAT_MASK)==PCO_CL_DATAFORMAT_5x16)
  {
   off=act_width;
   adr=(unsigned short*)bufin;
   if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_BOTTOM_CENTER)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=line_number*off*2;
    }
    else
    {
     adr+=off;
     adr+=(act_height-line_number-1)*off*2;
    }
    memcpy(bufout,adr,act_width*2);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_CENTER_BOTTOM)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=(act_height/2-line_number-1)*off*2;
    }
    else
    {
     adr+=off;
     adr+=line_number*off*2;
    }
    memcpy(bufout,adr,act_width*2);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_CENTER_TOP_BOTTOM_CENTER)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=(act_height/2-line_number-1)*off*2;
    }
    else
    {
     adr+=off;
     adr+=(act_height-line_number-1)*off*2;
    }
    memcpy(bufout,adr,act_width*2);
   }
   else if((DataFormat&SCCMOS_FORMAT_MASK)==SCCMOS_FORMAT_TOP_CENTER_CENTER_BOTTOM)
   {
    if(line_number<act_height/2) //upper half of image
    {
     adr+=line_number*off*2;
    }
    else
    {
     adr+=off;
     adr+=line_number*off*2;
    }
    memcpy(bufout,adr,act_width*2);
   }
  }
}
