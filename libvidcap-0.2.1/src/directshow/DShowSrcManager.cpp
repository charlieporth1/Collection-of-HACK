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

#include "os_funcs.h"
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <atlbase.h>
#include <qedit.h>
#include <comutil.h>
#include <DShow.h>

#include <vidcap/vidcap.h>
#include "logging.h"

#include "DShowSrcManager.h"
#include "SourceStateMachine.h"

DShowSrcManager * DShowSrcManager::instance_ = 0;

DShowSrcManager::DShowSrcManager()
	: numRefs_(0)
{
	HRESULT hr = CoInitialize(NULL);

	if ( FAILED(hr) )
	{
		throw std::runtime_error("failed calling CoInitialize()");
	}
}

DShowSrcManager::~DShowSrcManager()
{
	srcGraphList_.erase( srcGraphList_.begin(), srcGraphList_.end() );

	CoUninitialize();
}

DShowSrcManager *
DShowSrcManager::instance()
{
	if ( !instance_ )
		instance_ = new DShowSrcManager();

	instance_->numRefs_++;

	return instance_;
}

void
DShowSrcManager::release()
{
	numRefs_--;
	if ( !numRefs_ )
	{
		delete instance_;
		instance_ = 0;
	}
}

int
DShowSrcManager::registerNotifyCallback(void * sapiCtx)
{
	return devMon_.registerCallback(static_cast<sapi_context *>(sapiCtx));
}

int
DShowSrcManager::scan(struct sapi_src_list * srcList) const
{
	//FIXME: consider multiple sources per device

	int newListLen = 0;

	// Create an enumerator (self-releasing)
	CComPtr<ICreateDevEnum> pCreateDevEnum;

	pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);

	if ( !pCreateDevEnum )
	{
		log_error("failed creating device enumerator - to scan\n");
		srcList->len = newListLen;
		return 0;
	}

	// Enumerate video capture devices
	CComPtr<IEnumMoniker> pEm;

	pCreateDevEnum->CreateClassEnumerator(
			CLSID_VideoInputDeviceCategory, &pEm, 0);

	if ( !pEm )
	{
		log_error("Failed creating enumerator moniker\n");
		srcList->len = newListLen;
		return 0;
	}

	pEm->Reset();

	ULONG ulFetched;
	IMoniker * pM;

	while ( pEm->Next(1, &pM, &ulFetched) == S_OK )
	{
		CComPtr< IBindCtx > pbc;

		HRESULT hr = CreateBindCtx(0, &pbc);
		if ( FAILED(hr) )
		{
			log_error("failed CreateBindCtx\n");
			pM->Release();
			srcList->len = newListLen;
			return srcList->len;
		}

		IBaseFilter * pCaptureFilter = 0;
		CComPtr<IPin> pOutPin = 0;

		hr = pM->BindToObject(pbc, 0, IID_IBaseFilter,
				(void **)&pCaptureFilter);

		if ( FAILED(hr) )
		{
			log_error("failed BindToObject\n");
			goto clean_continue;
		}

		//NOTE: We do this to eliminate some 'ghost' entries that
		//      don't correspond to a physically plugged-in device
		pOutPin = getOutPin( pCaptureFilter, 0 );
		if ( !pOutPin )
			goto clean_continue;

		newListLen++;

		struct vidcap_src_info *srcInfo;
		if ( newListLen > srcList->len )
			srcList->list = (struct vidcap_src_info *)realloc(srcList->list,
					newListLen * sizeof(struct vidcap_src_info));

		srcInfo = &srcList->list[newListLen - 1];

		// get device description and ID
		char *tempDesc;
		char *tempID;
		getDeviceInfo(pM, pbc, &tempDesc, &tempID);

		sprintf_s(srcInfo->description, sizeof(srcInfo->description),
				"%s", tempDesc);
		sprintf_s(srcInfo->identifier, sizeof(srcInfo->identifier),
				"%s", tempID);

		free(tempID);
		free(tempDesc);

clean_continue:
		if ( pCaptureFilter )
			pCaptureFilter->Release();

		if ( pOutPin )
			pOutPin.Release();

		pM->Release();
	}

	srcList->len = newListLen;

	return srcList->len;
}

bool
DShowSrcManager::okayToBuildSource(const char *id)
{
	// find appropriate source  in list
	for ( unsigned int i = 0; i < srcGraphList_.size(); i++ )
	{
		if ( !strcmp(srcGraphList_[i]->sourceId, id) )
		{
			// a source with this id already exists - and was acquired
			log_warn("source already acquired: '%s'\n", id);
			return false;
		}
	}

	srcGraphContext *pSrcGraphContext = new srcGraphContext();
	pSrcGraphContext->sourceId = id;

	// these will be filled in later by registerSrcGraph()
	pSrcGraphContext->pSrc = 0;

	// add source to collection
	srcGraphList_.push_back(pSrcGraphContext);

	return true;
}

void DShowSrcManager::sourceReleased(const char *id)
{
	// NOTE: see note in function cancelSrcCaptureCallback()

	// find appropriate source id in list
	for ( unsigned int i = 0; i < srcGraphList_.size(); i++ )
	{
		// found matching source id?
		if ( srcGraphList_[i]->sourceId == id )
		{
			// found source with matching id

			// remove entry from srcGraphList_
			srcGraphContext *pSrcGraphContext = srcGraphList_[i];

			srcGraphList_.erase( srcGraphList_.begin() + i );

			// request that GraphMonitor stop monitoring the graph for errors

			delete pSrcGraphContext;

			return;
		}
	}

	log_warn("couldn't find source '%s' to release\n", id);
}


IPin *
DShowSrcManager::getOutPin( IBaseFilter * pFilter, int nPin ) const
{
	CComPtr<IPin> pComPin = 0;

	getPin(pFilter, PINDIR_OUTPUT, nPin, &pComPin);

	return pComPin;
}

HRESULT
DShowSrcManager::getPin( IBaseFilter * pFilter, PIN_DIRECTION dirrequired,
			int iNum, IPin **ppPin) const
{
	*ppPin = NULL;

	CComPtr< IEnumPins > pEnum;

	HRESULT hr = pFilter->EnumPins(&pEnum);

	if ( FAILED(hr) )
		return hr;

	ULONG ulFound;
	IPin * pPin;

	hr = E_FAIL;

	while ( S_OK == pEnum->Next(1, &pPin, &ulFound) )
	{
		PIN_DIRECTION pindir;

		pPin->QueryDirection(&pindir);

		if ( pindir == dirrequired )
		{
			if ( iNum == 0 )
			{
				*ppPin = pPin;
				hr = S_OK;
				break;
			}
			iNum--;
		}

		pPin->Release();
	}

	return hr;
}

