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

#include <stdio.h>

#include "sapi.h"
#include "SourceStateMachine.h"
#include "DShowSrcManager.h"
#include "logging.h"

static const char * identifier = "DirectShow";
static const char * description = "DirectShow video capture API";

// Function to handle invalid parameter events
// triggered by "secure" functions
_invalid_parameter_handler newHandler;
_invalid_parameter_handler old_handler;

static int
source_format_validate(struct sapi_src_context * src_ctx,
		const struct vidcap_fmt_info * fmt_nominal,
		struct vidcap_fmt_info * fmt_native)
{
	SourceStateMachine * dshow_src =
		static_cast<SourceStateMachine *>(src_ctx->priv);

	return dshow_src->validateFormat(fmt_nominal, fmt_native);
}

static int
source_format_bind(struct sapi_src_context * src_ctx,
		const struct vidcap_fmt_info * fmtInfo)
{
	SourceStateMachine * dshow_src =
		static_cast<SourceStateMachine *>(src_ctx->priv);

	return dshow_src->bindFormat(fmtInfo);
}

static void
my_invalid_parameter_handler(const wchar_t * expression,
		const wchar_t * function,
		const wchar_t * file,
		unsigned int line,
		uintptr_t reserved)
{
	wprintf(L"Invalid parameter detected in function %s."
			L" File: %s Line: %d\n", function, file, line);
	wprintf(L"Expression: %s\n", expression);

	exit(-1);
}

static int
source_capture_start(struct sapi_src_context * src_ctx)
{
	SourceStateMachine * dshow_src =
		static_cast<SourceStateMachine *>(src_ctx->priv);

	return dshow_src->start();
}

static int
source_capture_stop(struct sapi_src_context *src_ctx)
{
	SourceStateMachine *dshow_src =
		static_cast<SourceStateMachine *>(src_ctx->priv);

	return dshow_src->stop();
}

static void
sapi_dshow_destroy(struct sapi_context * sapi_ctx)
{
	DShowSrcManager * src_manager =
		static_cast<DShowSrcManager *>(sapi_ctx->priv);

	src_manager->release();

	free(sapi_ctx->user_src_list.list);
}

static int
monitor_sources(struct sapi_context * sapi_ctx)
{
	DShowSrcManager * src_manager =
		static_cast<DShowSrcManager *>(sapi_ctx->priv);

	return src_manager->registerNotifyCallback(sapi_ctx);
}

static int
scan_sources(struct sapi_context * sapi_ctx,
			struct sapi_src_list * src_list)
{
	DShowSrcManager * src_manager =
		static_cast<DShowSrcManager *>(sapi_ctx->priv);

	return src_manager->scan(src_list);
}

static int
source_release(struct sapi_src_context * src_ctx)
{
	delete static_cast<SourceStateMachine *>(src_ctx->priv);

	return 0;
}

static int
source_acquire(struct sapi_context * sapi_ctx,
		struct sapi_src_context * src_ctx,
		const struct vidcap_src_info * src_info)
{
	//FIXME: check that this src info matches a real src (when not NULL)

	DShowSrcManager * src_manager =
		static_cast<DShowSrcManager *>(sapi_ctx->priv);

	// Handle default case - user passes NULL
	if ( !src_info )
	{
		// TODO: This method for choosing the default source has
		// the problem where subsequent calls to vidcap_src_acquire()
		// requesting the default source will result in failure.
		// We should choose the first non-acquired source.
		if ( scan_sources(sapi_ctx, &sapi_ctx->user_src_list) > 0 )
		{
			src_info = &sapi_ctx->user_src_list.list[0];
		}
		else
		{
			log_warn("failed to acquire default source\n");
			return -1;
		}
	}

	memcpy(&src_ctx->src_info, src_info, sizeof(src_ctx->src_info));

	try
	{
		src_ctx->priv = new SourceStateMachine(src_ctx, src_manager);
	}
	catch ( std::runtime_error & e )
	{
		log_warn("failed constructing source '%s': %s\n",
				src_info->identifier, e.what());
		return -1;
	}
	catch ( ... )
	{
		log_warn("failed constructing source '%s'\n",
				src_info->identifier);
		return -1;
	}

	if ( src_ctx->priv == (void *)0 )
	{
		log_warn("failed constructing source state machine for '%s'\n",
				src_info->identifier);
		return -1;
	}

	// Save acquired source to the dshow-specific source context
	src_ctx->release         = source_release;
	src_ctx->format_validate = source_format_validate;
	src_ctx->format_bind     = source_format_bind;
	src_ctx->start_capture   = source_capture_start;
	src_ctx->stop_capture    = source_capture_stop;

	return 0;
}

extern "C" int
sapi_dshow_initialize(struct sapi_context *sapi_ctx)
{
	// setup handler for "secure" functions to handle invalid parameters
	newHandler = my_invalid_parameter_handler;
	old_handler = _set_invalid_parameter_handler(newHandler);

	// Disable the message box for assertions.
	_CrtSetReportMode(_CRT_ASSERT, 0);

	try
	{
		sapi_ctx->priv = DShowSrcManager::instance();
	}
	catch( std::runtime_error & e )
	{
		log_error("failed constructing DShowSrcManager: %s\n", e.what());
		return -1;
	}

	if ( sapi_ctx->priv == 0 )
	{
		log_error("failed constructing DShowSrcManager\n");
		return -1;
	}

	sapi_ctx->identifier = identifier;
	sapi_ctx->description = description;
	sapi_ctx->acquire = sapi_acquire;
	sapi_ctx->release = sapi_release;
	sapi_ctx->destroy = sapi_dshow_destroy;
	sapi_ctx->monitor_sources = monitor_sources;
	sapi_ctx->scan_sources = scan_sources;
	sapi_ctx->acquire_source = source_acquire;

	return 0;
}

