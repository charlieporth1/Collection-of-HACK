/*
 * libvidcap - a cross-platform video capture library
 *
 * Copyright 2007 Wimba, Inc.
 *
 * Contributors:
 * Peter Grayson <jpgrayson@gmail.com>
 * Bill Cholewka <bcholew@gmail.com>
 *
 * libvidcap is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * libvidcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "gworld.h"
#include "logging.h"

int gworld_init(struct gworld_info * gi, const Rect * dim, OSType pixel_format)
{
	OSErr err;

	if ( (err = QTNewGWorld(&gi->gworld, pixel_format, dim, 0, 0, 0)) )
	{
		log_error("failed QTNewGWorld() %d\n", err);
		return -1;
	}

	if ( !LockPixels(GetGWorldPixMap(gi->gworld)) )
	{
		log_error("failed LockPixels()\n");
		return -1;
	}

	return 0;
}

int gworld_destroy(struct gworld_info * gi)
{
	if ( !gi->gworld )
		return 0;

	UnlockPixels(GetGWorldPixMap(gi->gworld));
	DisposeGWorld(gi->gworld);

	gi->gworld = 0;

	return 0;
}

GWorldPtr gworld_get(struct gworld_info * gi)
{
	return gi->gworld;
}
