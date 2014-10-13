#include "required.h"
#include "helpers.h"
#include "filters.h"
#include "bridge.h"

void OnEnvStart() {
	// No op
}

void OnEnvDestroy() {
	// No op
}

int Crop(IplImage** pointer, char* args) {
	IplImage* image = *pointer;

	size_t col = image->width;
	size_t row = image->height;
	
	char separator[] = ",";
	char* next = NULL;
	char* token = strtok_r(args, separator, &next);

	char* wwmode;
	unsigned int windowWidth = strtol(token ? token : "", &wwmode, 10);

	token = strtok_r(NULL, separator, &next);

	char* whmode;
	unsigned int windowHeight = strtol(token ? token: "", &whmode, 10);

	if (strcmp(wwmode, "") == 0 && strcmp(whmode, "") == 0) {
		size_t px = col;
		size_t py = px / windowWidth * windowHeight;

		if (py > row) {
			py = row;
			px = py / windowHeight * windowWidth;
		}

		windowWidth  = (int)round(px);
		windowHeight = (int)round(py);
	} else if (strcmp(wwmode, "px") == 0 && strcmp(whmode, "px") == 0) {
		// nothing to do here?
	} else {
		return IMP_ERROR_INVALID_ARGS;
	}

	if (windowWidth == 0 || windowWidth > col || windowHeight == 0 || windowHeight > row) {
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
	
	return IMP_OK;
}

int Resize(IplImage** pointer, char* args, Config* config) {
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
		return IMP_ERROR_TOO_BIG_TARGET;
	}

	IplImage* resized = cvCreateImage(cvSize(width, height), image->depth, image->nChannels);
	int filter = width > col || height > row ? CV_INTER_CUBIC : CV_INTER_AREA;
	cvResize(image, resized, filter);
	cvReleaseImage(&image);
	*pointer = resized;

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

char* Info(IplImage* image) {
	char* json = malloc(256 * sizeof(char));
	sprintf(
		json,
		"{"
			"\"width\":%d,"
			"\"height\":%d,"
			"\"brightness\":%d"
		"}",
		image->width,
		image->height,
		(int)round(CalcPerceivedBrightness(image) * 100)
	);
	return json;
}

JobResult* RunJob(u_char* blob, size_t size, char* extension, char* request, Config* config) {
	#ifdef IMP_DEBUG
		syslog(LOG_NOTICE, "imp::RunJob():%s", request);
	#endif

	// Unzip
	char* crop    = NULL;
	char* resize  = NULL;
	char* quality = NULL;
	char* format  = NULL;
	int   info    = 0;

	char* filters[config->MaxFiltersCount];
	int filterCount = 0;

	JobResult* answer = (JobResult*)malloc(sizeof(JobResult));
	answer->Code = -1;
	answer->Length = 0;
	answer->Step = IMP_STEP_START;
	answer->MIME = IMP_MIME_INTACT;

	char* token;
	char* context = NULL;
	strtok_r(request, "?", &context);
	
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
		} else if (StartsWith(token, "info")) {
			info = 1;
			RewindArgs(token, "=");
		} else if (StartsWith(token, "filter")) {
			if (filterCount >= config->MaxFiltersCount) {
				answer->Code = IMP_ERROR_TOO_MUCH_FILTERS;
				return answer;
			} else {
				filters[filterCount++] = RewindArgs(token, "-");
			}
		}
	}

	// Run
	CvMat rawencoded = cvMat(1, (int)size, CV_8U, blob);
	IplImage* image = cvDecodeImage(&rawencoded, -1);
	if (!image) {
		answer->Code = IMP_ERROR_UNSUPPORTED;
		goto finalize;
	}

	answer->Step = IMP_STEP_CROP;
	if (crop) {
		answer->Code = Crop(&image, crop);
		if (answer->Code) {
			goto finalize;
		}
	}

	answer->Step = IMP_STEP_RESIZE;
	if (resize) {
		answer->Code = Resize(&image, resize, config);
		if (answer->Code) {
			goto finalize;
		}
	}

	answer->Step = IMP_STEP_FILTERING;
	int i;
	for (i = 0; i < filterCount; i++) {
		answer->Code = Filter(&image, filters[i], config->AllowExperiments);
		if (answer->Code) {
			goto finalize;
		}
	}

	answer->Step = IMP_STEP_WATERMARK;
	if (config->WatermarkInfo) {
		answer->Code = Watermark(image, config);
		if (answer->Code) {
			goto finalize;
		}
	}

	// alternative exit point
	answer->Step = IMP_STEP_INFO;
	if (info) {
		char* json = Info(image);
		answer->Code = IMP_OK;
		answer->Length = strlen(json);
		answer->EncodedBytes = cvCreateMat(1, answer->Length, CV_8U);
		answer->MIME = IMP_MIME_JSON;
		cvSetData(answer->EncodedBytes, json, answer->Length);
		goto finalize;
	}

	int coderopt[3];
	coderopt[2] = 0;
	answer->Step = IMP_STEP_FORMAT;
	if (format == NULL) {
		format = extension;
	}

	if (strcmp(format, "jpg") == 0) {
		answer->MIME = IMP_MIME_JPG;
		coderopt[0] = CV_IMWRITE_JPEG_QUALITY;
	} else if (strcmp(format, "png") == 0) {
		answer->MIME = IMP_MIME_PNG;
		coderopt[0] = CV_IMWRITE_PNG_COMPRESSION;
	} else if (strcmp(format, "webp") == 0) {
		#ifdef IMP_FEATURE_WEBP
			answer->MIME = IMP_MIME_WEBP;
		#else
			answer->Code = IMP_ERROR_FEATURE_DISABLED;
			goto finalize;
		#endif
	} else {
		answer->Code = IMP_ERROR_INVALID_ARGS;
		goto finalize;
	}

	
	answer->Step = IMP_STEP_QUALITY;
	if (quality) {
		int quantizer = strtol(quality, NULL, 10);
		int qisvalid = 
			   ((answer->MIME == IMP_MIME_JPG || answer->MIME == IMP_MIME_WEBP) && quantizer >= 0 && quantizer <= 100)
			|| (answer->MIME == IMP_MIME_PNG && quantizer >= 0 && quantizer <= 9);
		if (qisvalid) {
			coderopt[1] = quantizer;
		} else {
			answer->Code = IMP_ERROR_INVALID_ARGS;
			goto finalize;
		}
	} else {
		if (answer->MIME == IMP_MIME_JPG || answer->MIME == IMP_MIME_WEBP) {
			coderopt[1] = 86;
		} else if (answer->MIME == IMP_MIME_PNG) {
			coderopt[1] = 9;
		}
	}
	
	// Enjoy
	answer->Step = IMP_STEP_GET_BLOB;
	answer->Code = IMP_OK;

	if (answer->MIME == IMP_MIME_WEBP) {
		#ifdef IMP_FEATURE_WEBP
			uint8_t* output;
			CvRect dim = cvGetImageROI(image);
			// it's time for meh
			if (image->nChannels == 4) {
				answer->Length = WebPEncodeBGRA((uint8_t*)image->imageData, dim.width, dim.height, image->widthStep, coderopt[1], &output);
			} else {
				answer->Length = WebPEncodeBGR((uint8_t*)image->imageData, dim.width, dim.height, image->widthStep, coderopt[1], &output);
			}
			answer->EncodedBytes = cvCreateMat(1, answer->Length, CV_8U);
			cvSetData(answer->EncodedBytes, output, answer->Length);
		#endif
	} else {
		char formatcode[6];
		sprintf(formatcode, ".%s", format);
		answer->EncodedBytes = cvEncodeImage(formatcode, image, coderopt);
		answer->Length = answer->EncodedBytes->rows * answer->EncodedBytes->cols;
	}

	goto finalize;

	finalize:
		cvReleaseImage(&image);
		return answer;
}