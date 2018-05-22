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

#include <windows.h>
#include <atlbase.h>
#include <comutil.h>

#include "DirectShowObject.h"
#include "logging.h"

// Allocates and returns the description and ID for a given device
int
DirectShowObject::getDeviceInfo(IMoniker * pM, IBindCtx * pbc,
		char ** description, char **ID) const
{
	USES_CONVERSION;

	HRESULT hr;

	CComPtr< IMalloc > pMalloc;

	hr = CoGetMalloc(1, &pMalloc);

	if ( FAILED(hr) )
	{
		log_error("failed CoGetMalloc\n");
		return 1;
	}

	LPOLESTR pDisplayName;

	hr = pM->GetDisplayName(pbc, 0, &pDisplayName);

	if ( FAILED(hr) )
	{
		log_warn("failed GetDisplayName\n");
		return 1;
	}

	// This gets stack memory, no dealloc needed.
	char * pszDisplayName = OLE2A(pDisplayName);

	// Allocate a copy of this identifier
	*ID = _strdup(pszDisplayName);

	pMalloc->Free(pDisplayName);

	CComPtr< IPropertyBag > pPropBag;

	hr = pM->BindToStorage(pbc, 0, IID_IPropertyBag,
			(void **)&pPropBag);

	if ( FAILED(hr) )
	{
		log_warn("failed getting video device property bag\n");
		return 1;
	}

	VARIANT v;
	VariantInit(&v);

	char * pszFriendlyName = 0;

	hr = pPropBag->Read(L"FriendlyName", &v, 0);

	if ( SUCCEEDED(hr) )
		pszFriendlyName =
			_com_util::ConvertBSTRToString(v.bstrVal);

	// Allocate a copy of this description
	*description = _strdup(pszFriendlyName);

	delete [] pszFriendlyName;
	return 0;
}

