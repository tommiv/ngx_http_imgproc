#include "required.h"
#include "helpers.h"
#include "bridge.h"
#include "module.h"

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt   ngx_http_next_body_filter;

// Module commands list
static ngx_command_t ngx_http_imgproc_commands[] = {
	{
		ngx_string("imgproc"),
		NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, Enable),
		NULL
	}, {
		ngx_string("imgproc_watermark"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, WatermarkPath),
		NULL
	}, {
		ngx_string("imgproc_watermark_position"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE4,
		OnParseWatermarkPosition,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
	}, {
		ngx_string("imgproc_watermark_opacity"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, WatermarkOpacity),
		NULL
	}, {
		ngx_string("imgproc_max_src_size"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_size_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, MaxSrcSize),
		NULL
	}, {
		ngx_string("imgproc_max_target_dimensions"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
		OnParseMaxTargetDims,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
	}, {
		ngx_string("imgproc_max_filters_count"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, MaxFiltersCount),
		NULL
	}, {
		ngx_string("imgproc_allow_experiments"),
		NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(Config, AllowExperiments),
		NULL
	},
	ngx_null_command
};

// Module events
static ngx_http_module_t ngx_http_imgproc_module_ctx = {
	NULL,
	OnModuleInit, // postconf
	NULL,
	NULL,
	NULL,
	NULL,
	OnConfigCreate,
	OnConfigMerge
};

// Module export
ngx_module_t ngx_http_imgproc_module = {
	NGX_MODULE_V1,
	&ngx_http_imgproc_module_ctx,
	ngx_http_imgproc_commands,
	NGX_HTTP_MODULE,
	NULL,
	NULL,
	OnProcessInit,
	NULL,
	NULL,
	OnProcessDie,
	NULL,
	NGX_MODULE_V1_PADDING
};

// Callbacks
static ngx_int_t OnProcessInit(ngx_cycle_t* cycle) {
	OnEnvStart();
	return NGX_OK;
}

static void OnProcessDie(ngx_cycle_t* cycle) {
	OnEnvDestroy();
}

static ngx_int_t OnModuleInit(ngx_conf_t* cfg) {
	ngx_http_next_body_filter   = ngx_http_top_body_filter;
	ngx_http_top_body_filter    = BodyFilter;
	ngx_http_next_header_filter = ngx_http_top_header_filter;
	ngx_http_top_header_filter  = HeaderFilter;
	return NGX_OK;
}

static void* OnConfigCreate(ngx_conf_t* cfg) {
	Config* conf = ngx_pcalloc(cfg->pool, sizeof(Config));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}
	conf->Enable           = NGX_CONF_UNSET;
	conf->WatermarkOpacity = NGX_CONF_UNSET;
	conf->MaxSrcSize       = NGX_CONF_UNSET;
	conf->MaxFiltersCount  = NGX_CONF_UNSET;
	conf->AllowExperiments = NGX_CONF_UNSET;
	return conf;
}

static char* OnConfigMerge(ngx_conf_t* cfg, void* parent, void* child) {
	Config* prev = parent;
	Config* conf = child;
	
	ngx_conf_merge_value(conf->Enable, prev->Enable, 0);
	if (conf->Enable == 0) {
		return NGX_CONF_OK;
	} else if (conf->Enable != 1) {
		ngx_conf_log_error(NGX_LOG_EMERG, cfg, 0, "imp::[enable] should on/off value");
		return NGX_CONF_ERROR;
	}

	ngx_conf_merge_str_value(conf->WatermarkPath, prev->WatermarkPath, "");
	if (conf->WatermarkPath.len > 0) {
		ngx_conf_merge_value(conf->WatermarkOpacity, prev->WatermarkOpacity, 100);
		if (conf->WatermarkOpacity <= 0 || conf->WatermarkOpacity > 100) {
			ngx_conf_log_error(NGX_LOG_EMERG, cfg, 0, "imp::[watermark_opacity] is out of valid range 1-100");
			return NGX_CONF_ERROR;
		}
		
		if (!conf->WatermarkPosition) {
			if (prev->WatermarkPosition) {
				conf->WatermarkPosition = prev->WatermarkPosition;
			} else {
				ngx_conf_log_error(NGX_LOG_EMERG, cfg, 0, "imp::no any valid [watermark_position] found");
				return NGX_CONF_ERROR;
			}
		}

		int wmarkReadCode = PrepareWatermark(conf, cfg->pool);
		if (wmarkReadCode != IMP_OK) {
			ngx_conf_log_error(NGX_LOG_EMERG, cfg, 0, "imp::[watermark] path is not exists or not readable. Fix this or comment out [watermark] directive");
			return NGX_CONF_ERROR;
		}
	} else {
		conf->WatermarkInfo = NULL;
	}

	if (!conf->MaxTargetDimensions) {
		if (prev->MaxTargetDimensions) {
			conf->MaxTargetDimensions = prev->MaxTargetDimensions;
		} else {
			Dimensions* deflt = (Dimensions*)ngx_palloc(cfg->pool, sizeof(Dimensions));
			deflt->W = 2000;
			deflt->H = 2000;
			conf->MaxTargetDimensions = deflt;
		}
	}

	ngx_conf_merge_uint_value(conf->MaxSrcSize, prev->MaxSrcSize, 4 * 1024 * 1024);

	ngx_conf_merge_value(conf->MaxFiltersCount, prev->MaxFiltersCount, 5);

	ngx_conf_merge_value(conf->AllowExperiments, prev->AllowExperiments, 0);
	if (conf->AllowExperiments != 0 && conf->AllowExperiments != 1) {
		ngx_conf_log_error(NGX_LOG_EMERG, cfg, 0, "imp::[allow_experiments] should on/off value");
		return NGX_CONF_ERROR;
	}

	return NGX_CONF_OK;
}

static ngx_int_t HeaderFilter(ngx_http_request_t* r) {
	if (!r->args.len) {
		return ngx_http_next_header_filter(r);	
	}

	Config* conf = ngx_http_get_module_loc_conf(r, ngx_http_imgproc_module);
	if (!conf->Enable || r->headers_out.status != NGX_HTTP_OK) {
		return ngx_http_next_header_filter(r);
	}

    PeerContext* ctx = ngx_http_get_module_ctx(r, ngx_http_imgproc_module);
    if (ctx) {
        ngx_http_set_ctx(r, NULL, ngx_http_imgproc_module);
        return ngx_http_next_header_filter(r);
    }
	ctx = ngx_pcalloc(r->pool, sizeof(PeerContext));
	if (ctx == NULL) {
		return NGX_ERROR;
	}
    ngx_http_set_ctx(r, ctx, ngx_http_imgproc_module);

    int isMixed = 
    	   r->headers_out.content_type.len >= sizeof("multipart/x-mixed-replace") - 1
        && ngx_strncasecmp(r->headers_out.content_type.data, (u_char*)"multipart/x-mixed-replace", sizeof("multipart/x-mixed-replace") - 1) == 0;
    if (isMixed) {
    	OutputErrorString("imp::multipart/x-mixed-replace response", r);
        return NGX_ERROR;
    }

    long long int len = r->headers_out.content_length_n;
    if (len != -1 && len > (off_t)conf->MaxSrcSize) {
    	char msg[64];
    	sprintf(msg, "imp::too big source image: %lld", len);
    	OutputErrorString(msg, r);
        return NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
    }

    if (len == -1) {
        ctx->Length = conf->MaxSrcSize;
    } else {
        ctx->Length = (size_t)len;
    }
    ctx->ReadComplete = 0;
    ctx->DataSent = 0;

    if (r->headers_out.refresh) {
        r->headers_out.refresh->hash = 0;
    }

    r->main_filter_need_in_memory = 1;
    r->allow_ranges = 0;

    return NGX_OK;
}

static ngx_int_t BodyFilter(ngx_http_request_t* r, ngx_chain_t* in) {
	// Step 1: emergency check
	if (in == NULL) {
		OutputErrorString("imp::input chain is null", r);
		return ngx_http_next_body_filter(r, in);
	}

	if (!r->args.len) {
		#ifdef IMP_DEBUG
			syslog(LOG_NOTICE, "imp::skipped because of empty args");
		#endif
		return ngx_http_next_body_filter(r, in);	
	}

	Config* conf = ngx_http_get_module_loc_conf(r, ngx_http_imgproc_module);
	if (conf == NULL) {
		OutputErrorString("imp::module config is null", r);
		return ngx_http_next_body_filter(r, in);
	} else if (!conf->Enable) {
		#ifdef IMP_DEBUG
			syslog(LOG_NOTICE, "imp::disabled, skipped");
		#endif
		return ngx_http_next_body_filter(r, in);
	} else if (r->headers_out.status != NGX_HTTP_OK) {
		return ngx_http_next_body_filter(r, in);
	}

	PeerContext* ctx = ngx_http_get_module_ctx(r, ngx_http_imgproc_module);
	if (ctx == NULL) {
		OutputErrorString("imp::module context is null", r);
		return ngx_http_next_body_filter(r, in);
	}

	// Step 2: already done check
	if (ctx->DataSent) { // #todo: is this needed?
		return ngx_http_next_body_filter(r, in);
	}

	// Step 3: complete buffer compilation progress check
	if (!ctx->ReadComplete) {
		ngx_int_t chunkState = OnChunkAvailable(r, in);
		if (chunkState == NGX_AGAIN) {
			return NGX_OK;
		} else if (chunkState == NGX_ERROR) {
			return ngx_http_filter_finalize_request(r, &ngx_http_imgproc_module, NGX_HTTP_UNSUPPORTED_MEDIA_TYPE);
		}
	}
	ctx->ReadComplete = 1;

	// Step 4: run processing
	r->connection->buffered &= ~NGX_HTTP_IMAGE_BUFFERED;
	JobResult* result = RunJob(ctx->Image, ctx->Length, r, conf);

	// Step 5: check processing result
	#ifdef IMP_DEBUG
		syslog(LOG_NOTICE, "imp::done; code:%d", result->Code);
	#endif
	if (result->Code) {
		ngx_int_t httpCode = NGX_HTTP_INTERNAL_SERVER_ERROR;
		switch (result->Code) {
			case IMP_ERROR_UNSUPPORTED:
				OutputErrorString("imp::imgproc can't read data", r);
				httpCode = NGX_HTTP_UNSUPPORTED_MEDIA_TYPE;
			break;
			case IMP_ERROR_INVALID_ARGS:
				httpCode = NGX_HTTP_BAD_REQUEST;
			break;
			case IMP_ERROR_UPSCALE:
			case IMP_ERROR_NO_SUCH_FILTER:
			case IMP_ERROR_TOO_MUCH_FILTERS:
				httpCode = NGX_HTTP_NOT_ALLOWED;
			break;
			case IMP_ERROR_TOO_BIG_TARGET:
				httpCode = NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
			break;
			case IMP_ERROR_FEATURE_DISABLED:
				httpCode = 424; // Failed Dependency
			break;
		}
		char msg[44];
		sprintf(msg, "imp::Job failed at step %d with code %d", result->Step, result->Code);
		OutputErrorString(msg, r);
		return ngx_http_filter_finalize_request(r, &ngx_http_imgproc_module, httpCode);
	}

	// Step 6: initiate destructor
	// Not used anymore, but left here in case of
	// ngx_pool_cleanup_t* cln = ngx_pool_cleanup_add(r->pool, sizeof(PeerContext));
	// cln->handler = OnPeerClose;
	// cln->data = ctx;

	// Step 7: return response
	ngx_buf_t* imgdata = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	imgdata->pos = result->EncodedBytes;
	imgdata->last = result->EncodedBytes + result->Length;
	imgdata->memory = 1;
	imgdata->last_buf = 1;
	r->headers_out.content_length_n = result->Length;

	if (result->MIME != IMP_MIME_INTACT) {
		char* content_type = NULL;
		switch (result->MIME) {
			case IMP_MIME_JPG:
				content_type = "image/jpeg";
			break;
			case IMP_MIME_PNG:
				content_type = "image/png";
			break;
			case IMP_MIME_JSON:
				content_type = "application/json";
			break;
			case IMP_MIME_TEXT:
				content_type = "text/plain";
			break;
			default:
				#ifdef IMP_FEATURE_ADVANCED_IO
					content_type = (char*)FreeImage_GetFIFMimeType(result->MIME);
				#else
					content_type = "image/png";
				#endif
			break;
		}
		r->headers_out.content_type.data = (u_char*)content_type;
		r->headers_out.content_type.len  = strlen(content_type);
	}

	ngx_chain_t out;
	out.buf = imgdata;
	out.next = NULL;

	ctx->DataSent = 1;
	ngx_int_t sendState = ngx_http_next_header_filter(r);
	if (sendState == NGX_ERROR || sendState > NGX_OK || r->header_only) {
		return NGX_ERROR;
	} else {
		return ngx_http_next_body_filter(r, &out);
	}
}

static ngx_int_t OnChunkAvailable(ngx_http_request_t* r, ngx_chain_t* in) {
    PeerContext* ctx = ngx_http_get_module_ctx(r, ngx_http_imgproc_module);
    if (ctx->Image == NULL) {
        ctx->Image = ngx_palloc(r->pool, ctx->Length);
        if (ctx->Image == NULL) {
            return NGX_ERROR;
        }
        ctx->Last = ctx->Image;
    }

    u_char *p = ctx->Last;
    ngx_buf_t*   b;
    ngx_chain_t* cl;
    for (cl = in; cl; cl = cl->next) {
        b = cl->buf;
        
        size_t size = b->last - b->pos;
        size_t rest = ctx->Image + ctx->Length - p;
        if (size > rest) {
        	OutputErrorString("imp::too big source", r);
            return NGX_ERROR;
        }

        p = ngx_cpymem(p, b->pos, size);
        b->pos += size;

        if (b->last_buf) {
            ctx->Last = p;
            return NGX_OK;
        }
    }

    ctx->Last = p;
    r->connection->buffered |= NGX_HTTP_IMAGE_BUFFERED;

    return NGX_AGAIN;
}

static char* OnParseWatermarkPosition(ngx_conf_t* input, ngx_command_t* cmd, void* _cfg) {
	Position* result = (Position*)ngx_palloc(input->pool, sizeof(JobResult));
	ngx_str_t* value = input->args->elts;

	u_char* token = value[1].data;
	if (token == NULL || (ngx_strcmp(token, "l") != 0 && ngx_strcmp(token, "c") != 0 && ngx_strcmp(token, "r") != 0)) {
		goto error;
	} else {
		result->GravityX = token[0];
	}

	token = value[2].data;
	if (token == NULL || (ngx_strcmp(token, "t") != 0 && ngx_strcmp(token, "c") != 0 && ngx_strcmp(token, "b") != 0)) {
		goto error;
	} else {
		result->GravityY = token[0];
	}

	if (!value[3].len) {
		goto error;
	} else {
		result->OffsetX = ngx_atoi(value[3].data, value[3].len);
	}

	if (!value[4].len) {
		goto error;
	} else {
		result->OffsetY = ngx_atoi(value[4].data, value[4].len);
	}

	Config* config = _cfg;
	config->WatermarkPosition = result;

	return NGX_CONF_OK;

	error:
		ngx_conf_log_error(NGX_LOG_EMERG, input, 0, "imp::[watermark_position] parse error");
		return NGX_CONF_OK;
}

static char* OnParseMaxTargetDims(ngx_conf_t* input, ngx_command_t* cmd, void* _cfg) {
	Dimensions* result = (Dimensions*)ngx_palloc(input->pool, sizeof(Dimensions));
	ngx_str_t* value = input->args->elts;
	result->W = fmax(0, ngx_atoi(value[1].data, value[1].len));
	result->H = fmax(0, ngx_atoi(value[2].data, value[2].len));
	Config* config = _cfg;
	config->MaxTargetDimensions = result;
	return NGX_CONF_OK;
}