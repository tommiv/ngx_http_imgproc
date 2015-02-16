#include "required.h"
#include "helpers.h"
#include "filters.h"
#include "bridge.h"
#include "advancedio.h"

char SIG_PNG[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
char SIG_JPG[] = { 0xFF, 0xD8, 0xFF };

void OnEnvStart() {
	// No op
}

void OnEnvDestroy() {
	// No op
}

int Crop(IplImage** pointer, char* _args) {
	IplImage* image = *pointer;

	size_t col = image->width;
	size_t row = image->height;

	char separator[] = ",";
	char* args = CopyString(_args);
	char* next = NULL;
	char* token = strtok_r(args, separator, &next);

	char* wwmode;
	unsigned int windowWidth = strtol(token ? token : "", &wwmode, 10);

	token = strtok_r(NULL, separator, &next);

	char* whmode;
	unsigned int windowHeight = strtol(token ? token: "", &whmode, 10);

	if (strcmp(wwmode, "") == 0 && strcmp(whmode, "") == 0) {
		float px = col;
		float py = px / windowWidth * windowHeight;

		if (py > row) {
			py = row;
			px = py / windowHeight * windowWidth;
		}

		windowWidth  = (int)round(px);
		windowHeight = (int)round(py);
	} else if (strcmp(wwmode, "px") == 0 && strcmp(whmode, "px") == 0) {
		// nothing to do here?
	} else {
		free(args);
		return IMP_ERROR_INVALID_ARGS;
	}

	if (windowWidth == 0 || windowWidth > col || windowHeight == 0 || windowHeight > row) {
		free(args);
		return IMP_ERROR_INVALID_ARGS;
	}

	int windowX;
	token = strtok_r(NULL, separator, &next);
	if (token == NULL) {
		token = "c";
	}
	
	if (strcmp(token, "l") == 0) {
		windowX = 0;
	} else if (strcmp(token, "r") == 0) {
		windowX = col - windowWidth;
	} else {
		windowX = (int)round((col - windowWidth) / 2.0);
	}

	int windowY;
	token = strtok_r(NULL, separator, &next);
	if (token == NULL) {
		token = "t";
	}

	if (strcmp(token, "t") == 0) {
		windowY = 0;
	} else if (strcmp(token, "b") == 0) {
		windowY = row - windowHeight;
	} else {
		windowY = (int)round((row - windowHeight) / 2.0);
	}

	CvRect region = cvRect(windowX, windowY, windowWidth, windowHeight);
	cvSetImageROI(image, region);

	// Copy region to new image to discard cropped pixels
	IplImage* cropped = cvCreateImage(cvSize(region.width, region.height), image->depth, image->nChannels);
	cvCopy(image, cropped, NULL);
	cvReleaseImage(&image);
	*pointer = cropped;
	
	free(args);
	return IMP_OK;
}

int Resize(IplImage** pointer, char* _args, Config* config, int simple) {
	char* args = CopyString(_args);

	IplImage* image = *pointer;
	CvRect roi = cvGetImageROI(image);
	size_t col = roi.width;
	size_t row = roi.height;

	char separator[] = ",";
	char* next = NULL;
	char* token = strtok_r(args, separator, &next);

	char* xmode;
	unsigned int width = strtol(token ? token : "", &xmode, 10);

	token = strtok_r(NULL, separator, &next);
	char* ymode;
	unsigned int height = strtol(token ? token : "", &ymode, 10);

	if (width == 0 && height == 0) {
		free(args);
		return IMP_ERROR_INVALID_ARGS;
	}

	if (width == 0) {
		width = (int)round((float)height / row * col);
	}

	if (height == 0) {
		height = (int)round((float)width / col * row);
	}

	char* optarg = strtok_r(NULL, separator, &next);
	int allowupscale = optarg && strcmp(optarg, "up") == 0 ? 1 : 0;

	if (allowupscale == 0) {
		width  = fmin(width, col);
		height = fmin(height, row);
	}

	Dimensions* dmd = config->MaxTargetDimensions;
	if ((dmd->W > 0 && width > dmd->W) || (dmd->H > 0 && width > dmd->H)) {
		free(args);
		return IMP_ERROR_TOO_BIG_TARGET;
	}

	IplImage* resized = cvCreateImage(cvSize(width, height), image->depth, image->nChannels);
	int filter = simple ? CV_INTER_NN : width > col || height > row ? CV_INTER_CUBIC : CV_INTER_AREA;
	cvResize(image, resized, filter);
	cvReleaseImage(&image);
	*pointer = resized;

	free(args);
	return IMP_OK;
}

int PrepareWatermark(Config* cfg, ngx_pool_t* pool) {
	char* overlaypath = CopyNgxString(cfg->WatermarkPath);
	FILE* input = fopen(overlaypath, "rb");
	free(overlaypath);
	if (!input) {
		return IMP_ERROR_NO_SUCH_WATERMARK;
	}

	fseek(input, 0, SEEK_END);
	size_t inputsize = ftell(input);
	rewind(input);

	char* blob = malloc(inputsize);
	size_t bytesread = fread(blob, 1, inputsize, input);
	fclose(input);

	CvMat rawencoded = cvMat(1, (int)bytesread, CV_8U, blob);
	IplImage* image = cvDecodeImage(&rawencoded, -1);
	if (!image) {
		return IMP_ERROR_NO_SUCH_WATERMARK;
	}

	cfg->WatermarkInfo = ngx_palloc(pool, sizeof(RecoverInfo));
	cfg->WatermarkInfo->Length      = image->imageSize;
	cfg->WatermarkInfo->Size.width  = image->width;
	cfg->WatermarkInfo->Size.height = image->height;
	cfg->WatermarkInfo->Depth       = image->depth;
	cfg->WatermarkInfo->Channels    = image->nChannels;
	cfg->WatermarkInfo->Step        = image->widthStep;

	u_char* pointer = ngx_palloc(pool, image->imageSize);
	memcpy(pointer, image->imageData, image->imageSize);
	cvReleaseImage(&image);
	free(blob);

	cfg->WatermarkInfo->Pointer = pointer;

	return IMP_OK;
}

int Watermark(IplImage* image, Config* config) {
	RecoverInfo* inf = config->WatermarkInfo;
	IplImage* overlay = cvCreateImageHeader(inf->Size, inf->Depth, inf->Channels);
	cvSetData(overlay, inf->Pointer, inf->Step);

	Position* p = config->WatermarkPosition;

	CvRect origin = cvGetImageROI(image);
	CvRect ovrroi = cvGetImageROI(overlay);

	int basew = origin.width;
	int baseh = origin.height;
	int overw = ovrroi.width;
	int overh = ovrroi.height;

	int left = 0;
	int top  = 0;

	if (p->GravityX == 'c') {
		left = (basew - overw) / 2 + p->OffsetX;
	} else if (p->GravityX == 'r') {
		left = basew - overw - p->OffsetX;
	} else {
		left = p->OffsetX;
	}

	if (p->GravityY == 'c') {
		top = (baseh - overh) / 2 + p->OffsetY;
	} else if (p->GravityY == 'b') {
		top = baseh - overh - p->OffsetY;
	} else {
		top = p->OffsetY;
	}

	CvRect workarea = cvRect(left, top, ovrroi.width, ovrroi.height);
	cvSetImageROI(image, workarea);
	AlphaBlendOver(image, overlay, config->WatermarkOpacity / 100.0);
	cvSetImageROI(image, origin);

	cvReleaseImageHeader(&overlay);

	return IMP_OK;
}

u_char* Info(Album* album, ngx_pool_t* pool) {
	IplImage* image = album->Frames[0].Image;
	u_char* json = ngx_palloc(pool, 256 * sizeof(u_char));
	sprintf(
		(char*)json,
		"{"
			"\"width\":%d,"
			"\"height\":%d,"
			"\"brightness\":%d,"
			"\"count\":%d"
		"}",
		image->width,
		image->height,
		(int)round(CalcPerceivedBrightness(image) * 100),
		album->Count
	);
	return json;
}

JobResult* RunJob(u_char* blob, size_t size, ngx_http_request_t* req, Config* config) {
	// Step 0: Allocate base, parse request
	u_char* tempbuf = ngx_pnalloc(req->pool, req->unparsed_uri.len);
	u_char* _dst = tempbuf; // somehow ngx_unescape_uri corrupts passed buffer pointer
	ngx_unescape_uri(&tempbuf, &req->unparsed_uri.data, req->unparsed_uri.len, 0);
	int properReqLength = tempbuf - _dst;
	// Must. Copy. Again.
	u_char* request = ngx_pnalloc(req->pool, properReqLength + 1);
	memcpy(request, _dst, properReqLength);
	request[properReqLength] = '\0';

	#ifdef IMP_DEBUG
		syslog(LOG_NOTICE, "imp::RunJob():%s", request);
	#endif

	char* crop    = NULL;
	char* resize  = NULL;
	char* quality = NULL;
	char* format  = NULL;

	int isdestructive = 0;
	int page = -1;

	char* filters[config->MaxFiltersCount];
	int filterCount = 0;

	JobResult* answer = (JobResult*)ngx_palloc(req->pool, sizeof(JobResult));
	answer->Code = -1;
	answer->Length = 0;
	answer->Step = IMP_STEP_START;
	answer->MIME = IMP_MIME_INTACT;

	char* token;
	char* context = NULL;
	strtok_r((char*)request, "?", &context);
	
	char* params = strtok_r(NULL, "?", &context);
	if (!params) {
		answer->Code = IMP_ERROR_INVALID_ARGS;
		return answer;
	}
	context = NULL;

	while ((token = strtok_r(params, "&", &context))) {
		params = NULL;
		if (StartsWith(token, "crop")) {
			crop = RewindArgs(token, "=");
		} else if (StartsWith(token, "resize")) {
			resize = RewindArgs(token, "=");
		} else if (StartsWith(token, "quality")) {
			quality = RewindArgs(token, "=");
		} else if (StartsWith(token, "format")) {
			format = RewindArgs(token, "=");
		} else if (StartsWith(token, "page")) {
			page = strtol(RewindArgs(token, "="), NULL, 10);
		} else if (StartsWith(token, "filter")) {
			if (filterCount >= config->MaxFiltersCount) {
				answer->Code = IMP_ERROR_TOO_MUCH_FILTERS;
				return answer;
			} else {
				filters[filterCount] = RewindArgs(token, "-");
				if (isdestructive == 0) {
					isdestructive = CheckDestructive(filters[filterCount]);
				}
				filterCount++;
			}
		}
	}

	// Step 1: Validate input blob, determine decoder
	answer->Step = IMP_STEP_VALIDATE;
	int decodeBasicIo = 
		   ByteCompare(blob, (unsigned char*)&SIG_PNG, len(SIG_PNG)) 
		|| ByteCompare(blob, (unsigned char*)&SIG_JPG, len(SIG_JPG));
	int canDecode = decodeBasicIo;

	#ifdef IMP_FEATURE_ADVANCED_IO
	int decodeAdvancedIo = 0;
	if (!canDecode) {
		FIMEMORY* stream = FreeImage_OpenMemory(blob, size);
		FREE_IMAGE_FORMAT fm = FreeImage_GetFileTypeFromMemory(stream, 0);
		FreeImage_CloseMemory(stream);
		if (fm == FIF_UNKNOWN) {
			char* ext = CopyNgxString(req->exten);
			FreeImage_GetFIFFromFilename(ext);
			free(ext);
		}
		if (FiNotImplemented(fm)) {
			answer->Code = IMP_ERROR_UNSUPPORTED;
			return answer;
		} else {
			canDecode = 1;
			decodeAdvancedIo = fm;
		}
	}
	#else
		if (!canDecode) {
			answer->Code = IMP_ERROR_FEATURE_DISABLED;
			return answer;
		}
	#endif

	if (!canDecode) {
		answer->Code = IMP_ERROR_UNSUPPORTED;
		return answer;
	}

	// Step 1b: Determine encoder, validate options
	char* extension = CopyNgxString(req->exten);
	if (format == NULL) {
		format = extension;
	}

	int encodeBasicIO = 0;
	if (strcmp(format, "jpg") == 0) {
		answer->MIME = IMP_MIME_JPG;
		encodeBasicIO = 1;
	} else if (strcmp(format, "png") == 0) {
		answer->MIME = IMP_MIME_PNG;
		encodeBasicIO = 1;
	} else if (strcmp(format, "json") == 0) {
		answer->MIME = IMP_MIME_JSON;
		encodeBasicIO = 1;
	} else if (strcmp(format, "text") == 0) {
		answer->MIME = IMP_MIME_TEXT;
		encodeBasicIO = 1;
	}

	if (encodeBasicIO && page == -1 && answer->MIME != IMP_MIME_JSON) {
		page = 0;
	}

	int encodeAdvancedIO = 0;
	#ifdef IMP_FEATURE_ADVANCED_IO
	if (answer->MIME == IMP_MIME_INTACT) {
		FREE_IMAGE_FORMAT fm = FreeImage_GetFIFFromFilename(format);
		if (FiNotImplemented(fm)) {
			free(extension);
			answer->Code = IMP_ERROR_UNSUPPORTED;
			return answer;
		} else {
			encodeAdvancedIO = fm;
			answer->MIME = IMP_MIME_ADVIO;
			if (page == -1 && fm != FIF_GIF) {
				page = 0;
			}
		}
	}
	#else
		if (answer->MIME == IMP_MIME_INTACT) {
			free(extension);
			answer->Code = IMP_ERROR_FEATURE_DISABLED;
			return answer;
		}
	#endif

	free(extension);

	if (answer->MIME == IMP_MIME_INTACT) {
		answer->Code = IMP_ERROR_UNSUPPORTED;
		return answer;
	}

	int basicCoderopt[3];
	basicCoderopt[2] = 0;

	#ifdef IMP_FEATURE_ADVANCED_IO
	int advancedCoderopt = 0;
	#endif

	if (encodeBasicIO) {
		if (answer->MIME == IMP_MIME_JPG) {
			basicCoderopt[0] = CV_IMWRITE_JPEG_QUALITY;
			int quantizer = 86;
			if (quality) {
				quantizer = strtol(quality, NULL, 10);
			}
			if (quantizer >= 0 && quantizer <= 100) {
				basicCoderopt[1] = quantizer;
			} else {
				answer->Code = IMP_ERROR_INVALID_ARGS;
				return answer;
			}
		} else if (answer->MIME == IMP_MIME_PNG) {
			basicCoderopt[0] = CV_IMWRITE_PNG_COMPRESSION;
			int quantizer = 9;
			if (quality) {
				quantizer = strtol(quality, NULL, 10);
			}
			if (quantizer >= 0 && quantizer <= 9) {
				basicCoderopt[1] = quantizer;
			} else {
				answer->Code = IMP_ERROR_INVALID_ARGS;
				return answer;
			}
		}
	} else if (quality) {
		#ifdef IMP_FEATURE_ADVANCED_IO
			int quantizer = strtol(quality, NULL, 10);
			switch(encodeAdvancedIO) {
				case FIF_BMP:
					if (!strcmp(quality, "rle")) advancedCoderopt = BMP_SAVE_RLE;
				break;
				case FIF_TARGA:
					if (!strcmp(quality, "rle")) advancedCoderopt = TARGA_SAVE_RLE;
				break;
				case FIF_J2K:
				case FIF_JP2:
				case FIF_WEBP:
					if (quantizer < 0 || quantizer > 512) {
						answer->Code = IMP_ERROR_INVALID_ARGS;
						return answer;
					} else {
						advancedCoderopt = quantizer;
					}
				break;
				case FIF_TIFF:
					if      (!strcmp(quality, "deflate")) advancedCoderopt = TIFF_DEFLATE;
					else if (!strcmp(quality, "lzw"    )) advancedCoderopt = TIFF_LZW;
					else if (!strcmp(quality, "jpeg"   )) advancedCoderopt = TIFF_JPEG;
					else if (!strcmp(quality, "none"   )) advancedCoderopt = TIFF_NONE;
				break;
				case FIF_JPEG:
					advancedCoderopt = quantizer;
				break;
			}
		#endif
	}

	// Step 2: Decode
	answer->Step = IMP_STEP_DECODE;
	Album album;
	album.Error = IMP_OK;
	album.Count = 0;
	if (decodeBasicIo) {
		CvMat rawencoded = cvMat(1, (int)size, CV_8U, blob);
		IplImage* image = cvDecodeImage(&rawencoded, -1);
		if (image) {
			album.Count = 1;
			album.Frames = ngx_palloc(req->pool, sizeof(Frame));
			album.Frames[0].Image = image;
			album.Frames[0].Time = album.Frames[0].Dispose = album.Frames[0].TransparencyKey = 0;
		} else {
			album.Error = IMP_ERROR_UNSUPPORTED;
		}
	} else {
		#ifdef IMP_FEATURE_ADVANCED_IO
			DecodeRequest decoder;
			decoder.Buffer = blob;
			decoder.Length = size;
			decoder.Format = decodeAdvancedIo;
			decoder.IsDestructive = isdestructive;
			decoder.Page = page;
			decoder.Pool = req->pool;
			album = FiLoadFrames(decoder);
		#endif
	}

	if (album.Error != IMP_OK) {
		answer->Code = album.Error;
		return answer;
	}

	// Steps 3-7: main operators
	answer->Step = IMP_STEP_CROP;
	if (crop) {
		int fid;
		for (fid = 0; fid  < album.Count; fid ++) {
			IplImage* image = album.Frames[fid].Image;
			answer->Code = Crop(&image, crop);
			album.Frames[fid].Image = image;
			if (answer->Code) {
				goto finalize;
			}
		}
	}

	answer->Step = IMP_STEP_RESIZE;
	if (resize) {
		int fid;
		for (fid = 0; fid < album.Count; fid++) {
			IplImage* image = album.Frames[fid].Image;
			#ifdef IMP_FEATURE_ADVANCED_IO
				int simple = album.Count > 0 && encodeAdvancedIO == FIF_GIF;
			#else
				int simple = 0;
			#endif
			answer->Code = Resize(&image, resize, config, simple);
			album.Frames[fid].Image = image;
			if (answer->Code) {
				goto finalize;
			}
		}
	}

	answer->Step = IMP_STEP_FILTERING;
	int i, fid;
	for (fid = 0; fid < album.Count; fid++) {
		IplImage* image = album.Frames[fid].Image;
		for (i = 0; i < filterCount; i++) {
			answer->Code = Filter(&image, filters[i], config->AllowExperiments);
			if (answer->Code) {
				goto finalize;
			}
		}
		album.Frames[fid].Image = image;
	}

	answer->Step = IMP_STEP_WATERMARK;
	if (config->WatermarkInfo) {
		int fid;
		for (fid = 0; fid < album.Count; fid++) {
			IplImage* image = album.Frames[fid].Image;
			answer->Code = Watermark(image, config);
			album.Frames[fid].Image = image;
			if (answer->Code) {
				goto finalize;
			}
		}
	}

	int hasAlpha = album.Frames[0].Image->nChannels == 4;
	int needFlatten = hasAlpha && answer->MIME == IMP_MIME_JPG;
	#ifdef IMP_FEATURE_ADVANCED_IO
		if (encodeAdvancedIO) {
			needFlatten = hasAlpha && !FiSupports32bit(encodeAdvancedIO);
		}
	#endif

	if (needFlatten) {
		int fid;
		for (fid = 0; fid < album.Count; fid++) {
			IplImage* image = album.Frames[fid].Image;
			BlendWithPaper(image);
		}
	}

	// alternative exit points
	answer->Step = IMP_STEP_INFO;
	if (answer->MIME == IMP_MIME_JSON) {
		u_char* json = Info(&album, req->pool);
		answer->Code = IMP_OK;
		answer->Length = strlen((char*)json);
		answer->EncodedBytes = json;
		goto finalize;
	}

	if (answer->MIME == IMP_MIME_TEXT) {
		IplImage* image = album.Frames[0].Image;
		Memory res = ASCII(image, quality ? quality : "", req->pool);
		answer->Code = res.Error;
		if (res.Error == IMP_OK) {
			answer->Length       = res.Length;
			answer->EncodedBytes = res.Buffer;
		}
		goto finalize;
	}
	
	// Step 8: Encode && return
	answer->Step = IMP_STEP_ENCODE;
	answer->Code = IMP_OK;

	if (encodeAdvancedIO) {
		#ifdef IMP_FEATURE_ADVANCED_IO
			EncodeRequest encoder;
			encoder.Album  = &album;
			encoder.Pool   = req->pool;
			encoder.Format = encodeAdvancedIO;
			encoder.Flags  = advancedCoderopt;
			Memory blob = FiSaveFrames(encoder);
			if (blob.Error) {
				answer->Code = blob.Error;
			} else {
				u_char* output = ngx_palloc(req->pool, blob.Length);
				memcpy(output, blob.Buffer, blob.Length);
				ngx_pfree(req->pool, blob.Buffer);
				answer->EncodedBytes = output;
				answer->Length = blob.Length;
				answer->MIME = encodeAdvancedIO;
			}
		#endif
	} else {
		IplImage* image = album.Frames[0].Image;
		CvMat* encoded = cvEncodeImage(answer->MIME == IMP_MIME_JPG ? ".jpg" : ".png", image, basicCoderopt);
		answer->Length = encoded->rows * encoded->cols;
		u_char* output = ngx_palloc(req->pool, answer->Length);
		memcpy(output, encoded->data.ptr, answer->Length);
		cvReleaseMat(&encoded);
		answer->EncodedBytes = output;
	}

	goto finalize;

	finalize:
		if (album.Count && album.Frames) {
			int fid;
			for (fid = 0; fid < album.Count; fid++) {
				IplImage* image = album.Frames[fid].Image;
				cvReleaseImage(&image);
			}
			ngx_pfree(req->pool, album.Frames);
		}
		return answer;
}