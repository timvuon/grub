/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

/* SPECIAL NOTES!

   Please note following when reading the code below:

   - In this driver we assume that every memory can be accessed by same memory
     bus.  If there are different address spaces do not use this code as a base
     code for other archs.

   - Every function in this code assumes that bounds checking has been done in
     previous phase and they are opted out in here.  */

#include <grub/machine/vbe.h>
#include <grub/machine/vbefill.h>
#include <grub/machine/vbeutil.h>
#include <grub/types.h>
#include <grub/video.h>

/* Generic filler that works for every supported mode.  */
void
grub_video_i386_vbefill (struct grub_video_i386_vbeblit_info *dst,
                         grub_video_color_t color, int x, int y,
                         int width, int height)
{
  int i;
  int j;

  for (j = 0; j < height; j++)
    for (i = 0; i < width; i++)
      set_pixel (dst, x+i, y+j, color);
}

/* Optimized filler for direct color 32 bit modes.  It is assumed that color
   is already mapped to destination format.  */
void
grub_video_i386_vbefill_direct32 (struct grub_video_i386_vbeblit_info *dst,
                                  grub_video_color_t color, int x, int y,
                                  int width, int height)
{
  int i;
  int j;
  grub_uint32_t *dstptr;
  grub_size_t rowskip;

  /* Calculate the number of bytes to advance from the end of one line
     to the beginning of the next line.  */
  rowskip = dst->mode_info->pitch - dst->mode_info->bytes_per_pixel * width;

  /* Get the start address.  */
  dstptr = (grub_uint32_t *) grub_video_vbe_get_video_ptr (dst, x, y);

  for (j = 0; j < height; j++)
    {
      for (i = 0; i < width; i++)
        *dstptr++ = color;

      /* Advance the dest pointer to the right location on the next line.  */
      dstptr = (grub_uint32_t *) (((char *) dstptr) + rowskip);
    }
}

/* Optimized filler for direct color 24 bit modes.  It is assumed that color
   is already mapped to destination format.  */
void
grub_video_i386_vbefill_direct24 (struct grub_video_i386_vbeblit_info *dst,
                                  grub_video_color_t color, int x, int y,
                                  int width, int height)
{
  int i;
  int j;
  grub_size_t rowskip;
  grub_uint8_t *dstptr;
  grub_uint8_t fill0 = (grub_uint8_t)((color >> 0) & 0xFF);
  grub_uint8_t fill1 = (grub_uint8_t)((color >> 8) & 0xFF);
  grub_uint8_t fill2 = (grub_uint8_t)((color >> 16) & 0xFF);

  /* Calculate the number of bytes to advance from the end of one line
     to the beginning of the next line.  */
  rowskip = dst->mode_info->pitch - dst->mode_info->bytes_per_pixel * width;

  /* Get the start address.  */
  dstptr = (grub_uint8_t *) grub_video_vbe_get_video_ptr (dst, x, y);

  for (j = 0; j < height; j++)
    {
      for (i = 0; i < width; i++)
        {
          *dstptr++ = fill0;
          *dstptr++ = fill1;
          *dstptr++ = fill2;
        }

      /* Advance the dest pointer to the right location on the next line.  */
      dstptr += rowskip;
    }
}

/* Optimized filler for direct color 16 bit modes.  It is assumed that color
   is already mapped to destination format.  */
void
grub_video_i386_vbefill_direct16 (struct grub_video_i386_vbeblit_info *dst,
                                  grub_video_color_t color, int x, int y,
                                  int width, int height)
{
  int i;
  int j;
  grub_size_t rowskip;
  grub_uint8_t *dstptr;
  grub_uint8_t fill0 = (grub_uint8_t)((color >> 0) & 0xFF);
  grub_uint8_t fill1 = (grub_uint8_t)((color >> 8) & 0xFF);

  /* Calculate the number of bytes to advance from the end of one line
     to the beginning of the next line.  */
  rowskip = dst->mode_info->pitch - dst->mode_info->bytes_per_pixel * width;

  /* Get the start address.  */
  dstptr = (grub_uint8_t *) grub_video_vbe_get_video_ptr (dst, x, y);

  for (j = 0; j < height; j++)
    {
      for (i = 0; i < width; i++)
        {
          *dstptr++ = fill0;
          *dstptr++ = fill1;
        }

      /* Advance the dest pointer to the right location on the next line.  */
      dstptr += rowskip;
    }
}

/* Optimized filler for index color.  It is assumed that color
   is already mapped to destination format.  */
void
grub_video_i386_vbefill_direct8 (struct grub_video_i386_vbeblit_info *dst,
				 grub_video_color_t color, int x, int y,
				 int width, int height)
{
  int i;
  int j;
  grub_size_t rowskip;
  grub_uint8_t *dstptr;
  grub_uint8_t fill = (grub_uint8_t)color & 0xFF;

  /* Calculate the number of bytes to advance from the end of one line
     to the beginning of the next line.  */
  rowskip = dst->mode_info->pitch - dst->mode_info->bytes_per_pixel * width;

  /* Get the start address.  */
  dstptr = (grub_uint8_t *) grub_video_vbe_get_video_ptr (dst, x, y);

  for (j = 0; j < height; j++)
    {
      for (i = 0; i < width; i++)
        *dstptr++ = fill;

      /* Advance the dest pointer to the right location on the next line.  */
      dstptr += rowskip;
    }
}
