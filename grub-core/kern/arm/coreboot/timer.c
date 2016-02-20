/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2016  Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/machine/kernel.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/time.h>
#include <grub/fdtbus.h>
#include <grub/misc.h>

grub_uint64_t
grub_armv7_get_timer_value(void);

grub_uint32_t
grub_armv7_get_timer_frequency(void);

grub_uint32_t
grub_arm_pfr1(void);

static int have_timer = 0;
static volatile grub_uint32_t *sp804_regs;

static grub_uint64_t
sp804_get_time_ms (void)
{
  static grub_uint32_t high, last_low;
  grub_uint32_t low = ~sp804_regs[1];
  if (last_low > low)
    high++;
  last_low = low;
  return grub_divmod64 ((((grub_uint64_t) high) << 32) | low,
			1000, 0);
}

static grub_err_t
sp804_attach(const void *fdt, unsigned int nodeoffset)
{
  if (have_timer)
    return GRUB_ERR_NONE;
  const grub_uint32_t *reg = grub_fdt_get_prop (fdt, nodeoffset, "reg", 0);
  sp804_regs = (void *) grub_be_to_cpu32 (*reg);
  grub_install_get_time_ms (sp804_get_time_ms);
  have_timer = 1;
  return GRUB_ERR_NONE;
}

struct grub_fdtbus_driver sp804 =
{
  .compatible = "arm,sp804",
  .attach = sp804_attach
};

static grub_uint32_t timer_frequency_in_khz;

static grub_uint64_t
generic_get_time_ms (void)
{
  return grub_divmod64 (grub_armv7_get_timer_value(), timer_frequency_in_khz, 0);
}

static int
try_generic_timer (void)
{
  if (((grub_arm_pfr1 () >> 16) & 0xf) != 1)
    return 0;
  timer_frequency_in_khz = grub_armv7_get_timer_frequency() / 1000;
  if (timer_frequency_in_khz == 0)
    return 0;
  grub_install_get_time_ms (generic_get_time_ms);
  have_timer = 1;
  return 1;
}

void
grub_machine_timer_init (void)
{
  grub_fdtbus_register (&sp804);

  if (!have_timer)
    try_generic_timer ();
  if (!have_timer)
    grub_fatal ("No timer found");
}