//
// libvidcap - a cross-platform video capture library
//
// Copyright 2007 Wimba, Inc.
//
// Contributors:
// Peter Grayson <jpgrayson@gmail.com>
// Bill Cholewka <bcholew@gmail.com>
//
// libvidcap is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of
// the License, or (at your option) any later version.
//
// libvidcap is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program.  If not, see
// <http://www.gnu.org/licenses/>.
//

#ifndef _DSHOWSRCMANAGER_H_
#define _DSHOWSRCMANAGER_H_

#include <vector>
#include "DirectShowObject.h"
#include "DevMonitor.h"

class DShowSrcManager : public DirectShowObject
{

protected:
	DShowSrcManager();
	~DShowSrcManager();

public:
	static  DShowSrcManager * instance();
	int     scan(struct sapi_src_list *) const;

	// for device event notifications (additions and removals)
	int     registerNotifyCallback(void *);

	bool    okayToBuildSource(const char *);
	void    sourceReleased(const char *id);
	void    release();

private:
	static  DShowSrcManager *instance_;
	int     numRefs_;

	DevMonitor devMon_;

	struct srcGraphContext
	{
		void          * pSrc;
		const char    * sourceId;
	};

	// list of acquired sources and IDs
	std::vector<srcGraphContext *> srcGraphList_;

private:
	IPin *  getOutPin( IBaseFilter *, int) const;
	HRESULT getPin( IBaseFilter *, PIN_DIRECTION, int, IPin **) const;
};

#endif

