/*
 * Copyright (C) 2006 Stefan Siegl <stesie@brokenpipe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include <hurd.h>
#include <mach.h>
#include <device/device.h>

#include "iwlib.h"

static mach_port_t
master(void)
{
  /* the device master port */
  mach_port_t device_master = MACH_PORT_NULL;

  if(device_master == MACH_PORT_NULL) {
    if(get_privileged_ports(0, &device_master))
      return MACH_PORT_NULL;
  }

  return device_master;
}

/*
 * iw_set_ext, iw_get_ext
 *
 * try to send the ioctl() from the wireless tools to the gnumach
 * kernel somehow.
 *
 * Since there is no ioctl RPC, we need to use a
 * combination of get_status and set_status.
 */
int
iw_set_ext(int skfd, char *ifname, int req, struct iwreq *data)
{
  /* we need to call gnumach using device_open, we cannot use the
   * socket as some ioctl base */
  (void) skfd;

  kern_return_t err;
  device_t device;

  /* Set device name */
  strncpy(data->ifr_name, ifname, IFNAMSIZ);

  if((err = device_open(master(), D_WRITE | D_READ, ifname, &device)))
    return errno = EBADF, -1;

  if((req == SIOCSIWENCODE || req == SIOCSIWESSID
      || req == SIOCSIWNICKN || req == SIOCSIWSPY)
     && data->u.data.pointer)
    {
      /* ioctls which require an iw_point structure. we don't use
       * the user space pointer approach as linux does, but send the
       * data just along with device_set_status ... */
      struct iw_point *iwp = &data->u.data;

      int buflen = sizeof(* data) + iwp->length;

      if(iwp->length % sizeof(natural_t))
	buflen += sizeof(natural_t) - (iwp->length % sizeof(natural_t));

      char *buf = malloc(buflen);
      if(! buf)
	{
	  err = ENOMEM;
	  goto out;
	}

      memcpy(buf, data, sizeof(* data));
      memcpy(buf + sizeof(* data), iwp->pointer, iwp->length);

      err = device_set_status(device, req, (dev_status_t) buf,
			      buflen / sizeof(natural_t));

      if(! err) 
	{
	  void *ptr = iwp->pointer;
	  memcpy(data, buf, sizeof(* data));
	  iwp->pointer = ptr;

	  memcpy(iwp->pointer, buf + sizeof(* data), iwp->length);
	}

      free(buf);
    }
  else
    {  
      err = device_set_status(device, req, (dev_status_t) data,
			      sizeof(* data) / sizeof(natural_t));
    }

 out:
  device_close(device);

  return err ? (errno = EINVAL, -1) : 0;
}


int
iw_get_ext(int skfd, char *ifname, int req, struct iwreq *data)
{
  /* we need to call gnumach using device_open, we cannot use the
   * socket as some ioctl base */
  (void) skfd;

  kern_return_t err;
  device_t device;

  /* Set device name */
  strncpy(data->ifr_name, ifname, IFNAMSIZ);
  
  if((err = device_open(master(), D_WRITE | D_READ, ifname, &device)))
    return errno = EBADF, -1;

  if((req == SIOCGIWENCODE || req == SIOCGIWESSID || req == SIOCGIWRANGE
      || req == SIOCGIWNICKN || req == SIOCGIWSPY)
     && data->u.data.pointer)
    {
      /* ioctls which require an iw_point structure. we don't use
       * the user space pointer approach as linux does, but send the
       * data just along with device_get_status ... */
      struct iw_point *iwp = &data->u.data;

      int buflen = sizeof(* data) + iwp->length;

      if(iwp->length % sizeof(natural_t))
	buflen += sizeof(natural_t) - (iwp->length % sizeof(natural_t));

      char *buf = malloc(buflen);
      if(! buf)
	{
	  err = ENOMEM;
	  goto out;
	}

      unsigned int bufsz = buflen / sizeof(natural_t);
      err = device_get_status(device, req, (dev_status_t) buf, &bufsz);
      
      if(! err) 
	{
	  void *ptr = iwp->pointer;
	  memcpy(data, buf, sizeof(* data));
	  iwp->pointer = ptr;	/* restore the pointer, we just overwrote */

	  memcpy(iwp->pointer, buf + sizeof(* data), iwp->length);
	}

      free(buf);
    }
  else
    {  
      mach_msg_type_number_t bufsz = sizeof(* data) / sizeof(natural_t);
      err = device_get_status(device, req, (dev_status_t) data, &bufsz);
    }

 out:
  device_close(device);

  return err ? (errno = EINVAL, -1) : 0;
}

