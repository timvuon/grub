/* reboot.c - command to reboot the computer.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2007,2008  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/command.h>

#if defined(GRUB_MACHINE_IEEE1275)
#include <grub/machine/kernel.h>
#elif defined(GRUB_MACHINE_EFI)
#include <grub/efi/efi.h>
#elif defined(GRUB_MACHINE_PCBIOS)
#include <grub/machine/init.h>
#else
/* Platforms shipping standalone reboot, such as coreboot.  */
#include <grub/cpu/reboot.h>
#endif


static grub_err_t
grub_cmd_reboot (grub_command_t cmd __attribute__ ((unused)),
		 int argc __attribute__ ((unused)),
		 char **args __attribute__ ((unused)))
{
  grub_reboot ();
  return 0;
}

static grub_command_t cmd;

GRUB_MOD_INIT(reboot)
{
  cmd = grub_register_command ("reboot", grub_cmd_reboot,
			       0, "Reboot the computer");
}

GRUB_MOD_FINI(reboot)
{
  grub_unregister_command (cmd);
}
