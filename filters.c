#include "required.h"
#include "helpers.h"
#include "filters.h"

static const struct {
	char* Name;
	int   (*FilterCallback)(IplImage**, char*);
	int   Experimental;
	int   Destructive;
} CallbackMap[] = {
	{ "flip"     , Flip     , 0, 0 },
	{ "rotate"   , Rotate   , 0, 0 },
	{ "modulate" , Modulate , 0, 0 },
	{ "colorize" , Colorize , 0, 0 },
	{ "blur"     , Blur     , 0, 1 },
	{ "gamma"    , Gamma    , 0, 0 },
	{ "contrast" , Contrast , 0, 0 },
	{ "gradmap"  , Gradmap  , 0, 0 },
	{ "vignette" , Vignette , 1, 1 },
	{ "gotham"   , Gotham   , 1, 0 },
	{ "lomo"     , Lomo     , 1, 0 },
	{ "kelvin"   , Kelvin   , 1, 0 },
	{ "rainbow"  , Rainbow  , 1, 0 },
	{ "scanline" , Scanline , 1, 0 },
	#ifdef IMP_FEATURE_SLOW_FILTERS
	{ "cartoon"  , Cartoon  , 1, 0 },
	#endif
};

static int maplen = sizeof(CallbackMap) / sizeof(CallbackMap[0]);

int CheckDestructive(char* request) {
	int i;
	for (i = 0; i < maplen; i++) {
		if (ByteCompare((unsigned char*)request, (unsigned char*)CallbackMap[i].Name, strlen(CallbackMap[i].Name))) {
			return CallbackMap[i].Destructive;
		}
	}
	return 0;
}

// #todo: this free(request); are really annoying
int Filter(IplImage** pointer, char* _request, int allowExperiments) {
	char* request = CopyString(_request);
	char* context = NULL;
	char* type = strtok_r(request, "=", &context);

	if (type == NULL) {
		free(request);
		return IMP_ERROR_NO_SUCH_FILTER;
	}
	char* args = strtok_r(NULL, "=", &context);
	if (args == NULL) {
		free(request);
		return IMP_ERROR_INVALID_ARGS;
	}

	int i;
	for (i = 0; i < maplen; i++) {
		int found = strcmp(type, CallbackMap[i].Name) == 0 && (allowExperiments || !CallbackMap[i].Experimental);
		if (found) {
			int result = CallbackMap[i].FilterCallback(pointer, args);
			free(request);
			return result;
		}
	}

	free(request);
	return IMP_ERROR_NO_SUCH_FILTER;
}

int Flip(IplImage** pointer, char* args) {
	if (strlen(args) != 2) {
		return IMP_ERROR_INVALID_ARGS;
	}

	int horizontal = 0;
	int vertical   = 0;
	
	if (args[0] == '1') {
		horizontal = 1;
	} else if (args[0] != '0') {
		return IMP_ERROR_INVALID_ARGS;
	}
	
	if (args[1] == '1') {
		vertical = 1;
	} else if (args[1] != '0') {
		return IMP_ERROR_INVALID_ARGS;
	}

	IplImage* image  = *pointer;
	IplImage* result = cvCreateImage(cvGetSize(image), image->depth, image->nChannels);
	if (horizontal && vertical) {
		cvFlip(image, result, -1);
	} else if (horizontal) {
		cvFlip(image, result, 1);
	} else if (vertical) {
		cvFlip(image, result, 0);
	} else {
		cvReleaseImage(&result);
		return IMP_OK;
	}

	*pointer = result;
	cvReleaseImage(&image);

	return IMP_OK;
}

int Rotate(IplImage** pointer, char* args) {
	IplImage* image = *pointer;

	int amount = strtol(args, NULL, 10);
	if (amount == 90 || amount == 270) {
		IplImage* transposed = cvCreateImage(cvSize(image->height, image->width), image->depth, image->nChannels);
		IplImage* result = cvCreateImage(cvSize(image->height, image->width), image->depth, image->nChannels);
		cvTranspose(image, transposed);
		cvFlip(transposed, result, 270 - amount);
		*pointer = result;
		cvReleaseImage(&image);
		cvReleaseImage(&transposed);
		return IMP_OK;
	} else if (amount == 180) {
		IplImage* result = cvCreateImage(cvGetSize(image), image->depth, image->nChannels);
		cvFlip(image, result, -1);
		*pointer = result;
		cvReleaseImage(&image);
		return IMP_OK;
	} else {
		return IMP_ERROR_INVALID_ARGS;
	}
}

int Modulate(IplImage** pointer, char* args) {
	int params[3];
	char* context = NULL;
	int i;
	for (i = 0; i < 3; i++) {
		char* token = strtok_r(args, ",", &context);
		if (token == NULL) {
			return IMP_ERROR_INVALID_ARGS;
		}
		args = NULL;
		params[i] = strtol(token, NULL, 10);
	}

	if (params[0] < 0 || params[0] > CV_HUE_WHEEL_RESOLUTION) {
		return IMP_ERROR_INVALID_ARGS;
	}
	if (params[2] <= 0) {
		return IMP_ERROR_INVALID_ARGS;	
	}

	ModulateHSV(*pointer, params);

	return IMP_OK;
}

int Colorize(IplImage** pointer, char* args) {
	char* context = NULL;
		
	int rgbl = 3;
	int tokl = 2;

	char* color = strtok_r(args, ",", &context);
	if (!color || (int)strlen(color) != rgbl * tokl) {
		return IMP_ERROR_INVALID_ARGS;
	}

	int rgb[rgbl];
	int i;
	for (i = 0; i < rgbl; i++) {
		char* token = malloc(tokl + 1);
		memcpy(token, color + (i * tokl), tokl);
		token[tokl] = '\0';
		rgb[i] = strtol(token, NULL, 16);
		free(token);
	}

	char* oparg = strtok_r(NULL, ",", &context);
	float opacity = oparg ? strtof(oparg, NULL) : 0.5f;
	if (color == NULL || opacity < 0 || opacity > 1) {
		return IMP_ERROR_INVALID_ARGS;
	}

	AlphaBlendAddColor(*pointer, rgb, opacity);

	return IMP_OK;
}

int Blur(IplImage** pointer, char* args) {
	char* context = NULL;
	char* arg = strtok_r(args, ",", &context);
	if (arg == NULL) {
		return IMP_ERROR_INVALID_ARGS;
	}
	
	float sigma = strtof(arg, NULL);
	if (sigma < 0) {
		return IMP_ERROR_INVALID_ARGS;
	}

	cvSmooth(*pointer, *pointer, CV_GAUSSIAN, 0, 0, sigma, 0);

	return IMP_OK;
}

int Gamma(IplImage** pointer, char* args) {
	ApplyGamma(*pointer, strtof(args, NULL));
	return IMP_OK;
}

int Contrast(IplImage** pointer, char* args) {
	float value = strtof(args, NULL);
	if (value <= 0) {
		return IMP_ERROR_INVALID_ARGS;
	}
	BrightnessContrast(*pointer, 0, value);
	return IMP_OK;
}

int Gradmap(IplImage** pointer, char* args) {
	int maxsteps = 8;
	int rgbl = 3;
	int tokl = 2;

	unsigned char** colors = malloc(maxsteps * sizeof(unsigned char*));
	int c;
	for (c = 0; c < maxsteps; c++) colors[c] = NULL;

	int step = 0;
	int valid = 1;

	char* inp = args;
	char* context = NULL;
	char* current = NULL;

	while ((current = strtok_r(inp, ",", &context))) {
		inp = NULL;

		if (strlen(current) != 6) {
			valid = 0;
			break;
		}

		colors[step] = colors[step] = malloc(rgbl);
		int i;
		for (i = 0; i < rgbl; i++) {
			char* token = malloc(tokl + 1);
			memcpy(token, current + (i * tokl), tokl);
			token[tokl] = '\0';
			colors[step][i] = strtol(token, NULL, 16);
			free(token);
		}

		step++;
	}

	if (valid) {
		unsigned char* lut = NULL;
		lut = CalculateGradientLUT(colors, step);
		IplImage* image = *pointer;
		int x, y;
		for (x = 0; x < image->width; x++)
		for (y = 0; y < image->height; y++) {
			int offset = ((
				cvGetComponent(image, x, y, CV_RGB_RED) + 
				cvGetComponent(image, x, y, CV_RGB_GREEN) + 
				cvGetComponent(image, x, y, CV_RGB_BLUE)
			) / 3) * 3;

			cvSetComponent(image, x, y, CV_RGB_RED  , lut[offset + 0]);
			cvSetComponent(image, x, y, CV_RGB_GREEN, lut[offset + 1]);
			cvSetComponent(image, x, y, CV_RGB_BLUE , lut[offset + 2]);
		}
		free(lut);
	}

	for (c = 0; c < maxsteps; c++) {
		if (colors[c]) free(colors[c]);
	}
	free(colors);

	return valid ? IMP_OK : IMP_ERROR_INVALID_ARGS;
}

// Original algorithm was designed to work in CIELab colorspace,
// but conversion (integer RGB) => LAB is too hard to implement in-place.
// To support proper algorithm flow with 4channel images
// a lot of channel split/merge and mem copy/reallocation should be done.
// So I suppose it's better to apply gradient in HSV colorspace. 
// It still provides decent results in my test 
// but allows to do most conversions in-place in integer domain
int Vignette(IplImage** pointer, char* args) {
	char* context = NULL;

	char* _intn = strtok_r(args, ",", &context);
	float intensity = _intn == NULL ? 0.5 : strtof(_intn, NULL);

	char* _rad = strtok_r(NULL, ",", &context);
	float radius = _rad == NULL ? 1.0 : strtof(_rad, NULL);

	IplImage* image = *pointer;

    CvSize size = cvGetSize(image);

	float* mask = malloc(image->width * image->height * sizeof(float));
	RadialGradient(mask, size, intensity, radius, cvPoint(image->width / 2, image->height / 2));

	RGB2HSV(image);
	int x, y;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++) {
		float source = cvGetComponent(image, x, y, CV_HSV_VAL);
		cvSetComponent(image, x, y, CV_HSV_VAL, source * mask[image->width * y + x]);
	}
	HSV2RGB(image);

	free(mask);

	return IMP_OK;
}

int Gotham(IplImage** pointer, char* args) {
	int hsv[] = { 120, 5, 100 };
	ModulateHSV(*pointer, hsv);
	int rgb[] = { 17, 27, 93 };
	AlphaBlendAddColor(*pointer, rgb, 0.15);
	ApplyGamma(*pointer, 0.3);
	BrightnessContrast(*pointer, -0.07, 1.5);
	return IMP_OK;
}

int Lomo(IplImage** pointer, char* args) {
	IplImage* image = *pointer;
	int x, y, c;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++)
	for (c = 1; c < 3; c++) {
		float val = cvGetComponent(image, x, y, c);
		val = fmax(fmin(val * 1.5 - 50, 255), 0);
		cvSetComponent(image, x, y, c, val);
	}
	return IMP_OK;
}

int Kelvin(IplImage** pointer, char* args) {
	int hsv[] = { 120, 50, 100 };
	ModulateHSV(*pointer, hsv);
	int rgb[] = { 255, 153, 0 };
	AlphaBlendAddColor(*pointer, rgb, 0.5);
	return IMP_OK;
}

int Rainbow(IplImage** pointer, char* args) {
	int sat = 255; // full
	if (strcmp(args, "mid") == 0) {
		sat = 190;
	} else if (strcmp(args, "pale") == 0) {
		sat = 120;
	} else if (strcmp(args, "full") != 0) {
		return IMP_ERROR_INVALID_ARGS;
	}

	IplImage* image = *pointer;
	RGB2HSV(image);
	int x, y;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++) {
		int hue   = cvGetComponent(image, x, y, CV_HSV_HUE) * 2; // extend range to common 360 deg
		int light = cvGetComponent(image, x, y, CV_HSV_VAL);
		int saturation = sat;

		if (light < 20) { // black
			light = 0;
			saturation = 0;
		} else if (light > 254) { // white
			saturation = 0;
		} else if (hue <= 10 || hue > 340) { // red
			hue = 0;
		} else if (hue >= 10 && hue < 35) { // orange
			hue = 30;
		} else if (hue >= 35 && hue < 68) { // yellow
			hue = 60;
		} else if (hue >= 68 && hue < 150) { // green
			hue = 120;
		} else if (hue >= 150 && hue < 200) { // light blue
			hue = 195;
		} else if (hue >= 200 && hue < 250) { // blue 
			hue = 225;
		} else { // violet
			hue = 285;
		}
		cvSetComponent(image, x, y, CV_HSV_HUE, hue / 2.0);
		cvSetComponent(image, x, y, CV_HSV_SAT, saturation);
		cvSetComponent(image, x, y, CV_HSV_VAL, light);
	}

	HSV2RGB(image);

	return IMP_OK;
}

int Scanline(IplImage** pointer, char* args) {
	char* context = NULL;
	
	char* _ints = strtok_r(args, ",", &context);
	float intensity = strtof(_ints, NULL);
	if (intensity < 0 || intensity > 1) {
		return IMP_ERROR_INVALID_ARGS;
	}

	char* _opac = strtok_r(NULL, ",", &context);
	float opacity = _opac == NULL ? 0 : strtof(_opac, NULL);
	if (opacity < 0 || opacity > 1) {
		return IMP_ERROR_INVALID_ARGS;
	}

	char* _freq = strtok_r(NULL, ",", &context);
	int freq = _freq == NULL ? 1 : strtol(_freq, NULL, 10);
	if (freq < 1) {
		return IMP_ERROR_INVALID_ARGS;
	}

	char* _w = strtok_r(NULL, ",", &context);
	int width = _w == NULL ? 1 : strtol(_w, NULL, 10);
	if (width < 1) {
		return IMP_ERROR_INVALID_ARGS;
	}	

	IplImage* image = *pointer;
	RGB2HSV(image);
	int skipped = 0;
	int drawed  = 0;
	int x, y;
	for (y = 0; y < image->height; y++) {
		if (skipped == freq) {
			if (drawed == width) {
				skipped = drawed = 0;
			} else {
				for (x = 0; x < image->width; x++) {
					cvSetComponent(image, x, y, 1, 255 * opacity);
					cvSetComponent(image, x, y, 2, 255 * intensity);
				}
				drawed++;
			}
		} else {
			skipped++;
		}
	}
	HSV2RGB(image);

	return IMP_OK;
}

#ifdef IMP_FEATURE_SLOW_FILTERS
// This filter creates cool effect, but k-means algorithm too complex and therefore slow for runtime usage
int Cartoon(IplImage** pointer, char* args) {
	IplImage* image = *pointer;
	IplImage* result = cvCreateImage(cvGetSize(image), image->depth, image->nChannels);
    Kmeans(image, 10);
    cvSmooth(image, result, CV_BILATERAL, 7, 0, 150, 150);
    
    IplImage* edges = cvCreateImage(cvGetSize(result), IPL_DEPTH_8U, 1);
    cvCanny(result, edges, 50, 200, 3);
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* contours = 0;
    cvFindContours(edges, storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
    
    CvSeq* iter;
    for (iter = contours; iter != 0; iter = iter->h_next){
        cvDrawContours(result, iter, CV_RGB(40, 40, 40), cvScalarAll(0), 0, 1, CV_AA, cvPoint(0,0));
    }
    
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&edges);
    cvReleaseImage(&image);

    *pointer = result;

    return IMP_OK;
}
#endif

static unsigned char AsciiDensityWide[]   = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
static unsigned char AsciiDensityNarrow[] = "@%8#*+=-:. ";
Memory ASCII (IplImage* image, char* args, ngx_pool_t* pool) {
	unsigned char* table;
	if (strcmp(args, "wide") == 0) {
		table = AsciiDensityWide;
	} else {
		table = AsciiDensityNarrow;
	}
	int tablelen = strlen((const char*)table);
	float factor = 256.0 / tablelen;

	int width  = image->width;
	int height = image->height;

	int buflen = (width + 1) * height - 1;
	unsigned char* ascii = ngx_palloc(pool, buflen);

	RGB2HSV(image);
	int x, y;
	for (y = 0; y < height; y++) {
		int rowoffset = y * (width + 1);
		for (x = 0; x < width; x++) {
			int density = floor(cvGetComponent(image, x, y, CV_HSV_VAL) / factor);
			ascii[rowoffset + x] = table[density];
		}
		if (rowoffset > 0) {
			ascii[rowoffset - 1] = '\n';
		}
	}

	Memory result;
	result.Buffer = ascii;
	result.Length = buflen;
	result.Error  = IMP_OK;
	return result;
}

void ModulateHSV(IplImage* image, int* hsv) {
	RGB2HSV(image);

	int x, y;
	for (x = 0; x < image->width; x++)
	for (y = 0; y < image->height; y++) {
		int component = 0;
		if (hsv[component] != 0) {
			int hue = cvGetComponent(image, x, y, component) + hsv[component];
			if (hue > CV_HUE_WHEEL_RESOLUTION) {
				hue -= CV_HUE_WHEEL_RESOLUTION;
			}
			cvSetComponent(image, x, y, component, hue);
		}
		
		for (component++; component < 3; component++) {
			int cval = cvGetComponent(image, x, y, component);
			cval = fmin(cval * hsv[component] / 100.0, 255);
			cvSetComponent(image, x, y, component, cval);
		}
	}

	HSV2RGB(image);
}

void ApplyGamma(IplImage* image, float gamma) {
	int* lut = CalculateGammaLUT(gamma);
	int x, y, c;
	for (x = 0; x < image->width    ; x++)
	for (y = 0; y < image->height   ; y++)
	for (c = 0; c < image->nChannels; c++) {
		int val = cvGetComponent(image, x, y, c);
		cvSetComponent(image, x, y, c, lut[val]);
	}
	free(lut);
}

int* CalculateGammaLUT(float gamma) {
	float inverse = 1 / gamma;
	int size = 256;
	int* lut = malloc(size * sizeof(int));
	int i;
	for (i = 0; i < size; i++) {
		lut[i] = (int)(pow(i / 255.0, inverse) * 255.0);
	}
	return lut;
}

unsigned char* CalculateGradientLUT(unsigned char** colors, int length) {
	int segments = length - 1;
    int resolution = 256;
    float innerResolution = resolution / (float)segments;

    unsigned char* lut = malloc(resolution * 3);

    int c, i, j, pointer = 0;
    for (c = 0; c < segments; c++) {
        unsigned char* from = colors[c];
        unsigned char* to   = colors[c + 1];
        for (i = 0; i < (int)innerResolution; i++) {
            float step = i / innerResolution;
            for (j = 0; j < 3; j++) {
            	lut[pointer] = round(from[j] + step * (to[j] - from[j]));
            	pointer++;
            }
        };
    };

	return lut;
}

void BrightnessContrast(IplImage* image, float br, float ct) {
	int x, y, c;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++)
	for (c = 0; c < fmin(image->nChannels, 3); c++) {
		int val = cvGetComponent(image, x, y, c);
		val = (ct * val) + (br * 255);
		val = fmax(fmin(val, 255), 0);
		cvSetComponent(image, x, y, c, val);
	}
}

// Implementation of cvAddWeighted without solid color matrix creation 
void AlphaBlendAddColor(IplImage* source, int* rgb, float alpha) {
	float beta = 1 - alpha;
	int x, y, c;
	for (x = 0; x < source->width ; x++)
	for (y = 0; y < source->height; y++)
	for (c = 0; c < 3; c++) {
		cvSetComponent(source, x, y, c, (beta * cvGetComponent(source, x, y, c)) + (rgb[2 - c] * alpha));
	}
}

// Unfortunately, cvAddWeighted produces weird results for transparent overlay
void AlphaBlendOver(IplImage* destination, IplImage* source, float opacity) {
	float alpha = 1 - opacity;

	CvRect workarea = cvGetImageROI(destination);
	
	int maxrow = fmin(source->height, destination->height - workarea.y);
	int maxcol = fmin(source->width , destination->width  - workarea.x);

	int row, col;
	for (row = 0; row < maxrow; row++)
	for (col = 0; col < maxcol; col++) {
		int posx = col + workarea.x;
		int posy = row + workarea.y;

		int destinationB = cvGetComponent(destination, posx, posy, CV_RGB_BLUE );
		int destinationG = cvGetComponent(destination, posx, posy, CV_RGB_GREEN);
		int destinationR = cvGetComponent(destination, posx, posy, CV_RGB_RED  );
		float destinationA = destination->nChannels == 4 ? cvGetComponent(destination, posx, posy, CV_ALPHA) / 255.0 : 1;

		int sourceB = cvGetComponent(source, col, row, CV_RGB_BLUE );
		int sourceG = cvGetComponent(source, col, row, CV_RGB_GREEN);
		int sourceR = cvGetComponent(source, col, row, CV_RGB_RED  );
		float sourceA = source->nChannels == 4 ? cvGetComponent(source, col, row, CV_ALPHA) / 255.0 : 1;
		sourceA = fmax(sourceA - alpha, 0);

		float targetA = sourceA + destinationA * (1 - sourceA);
		int targetB, targetG, targetR;
		if (targetA == 0) {
			targetB = targetG = targetR = 0;
		} else {
			targetB = (sourceB * sourceA + destinationB * destinationA * (1 - sourceA)) / targetA;
			targetG = (sourceG * sourceA + destinationG * destinationA * (1 - sourceA)) / targetA;
			targetR = (sourceR * sourceA + destinationR * destinationA * (1 - sourceA)) / targetA;
		}

		cvSetComponent(destination, posx, posy, CV_RGB_BLUE , targetB);
		cvSetComponent(destination, posx, posy, CV_RGB_GREEN, targetG);
		cvSetComponent(destination, posx, posy, CV_RGB_RED  , targetR);
		
		if (destination->nChannels == 4) {
			cvSetComponent(destination, posx, posy, CV_ALPHA, targetA * 255);
		}
	}
}

// Discard transparency by blend with #FFFFFFFF paper,
// used to save proper image when encoder does not support alpha channel
void BlendWithPaper(IplImage* source) {
	int x, y;
	for (y = 0; y < source->height; y++)
	for (x = 0; x < source->width ; x++) {
		int overlayB = cvGetComponent(source, x, y, CV_RGB_BLUE );
		int overlayG = cvGetComponent(source, x, y, CV_RGB_GREEN);
		int overlayR = cvGetComponent(source, x, y, CV_RGB_RED  );
		int overlayA = cvGetComponent(source, x, y, CV_ALPHA    );

		int   diffalpha = 255 - overlayA;
		float prodalpha = overlayA / 255.0;
		int targetB = diffalpha + (overlayB * prodalpha);
		int targetG = diffalpha + (overlayG * prodalpha);
		int targetR = diffalpha + (overlayR * prodalpha);
		
		cvSetComponent(source, x, y, CV_RGB_BLUE , targetB);
		cvSetComponent(source, x, y, CV_RGB_GREEN, targetG);
		cvSetComponent(source, x, y, CV_RGB_RED  , targetR);
		
		cvSetComponent(source, x, y, CV_ALPHA, 255);
	}
}

// Port from Browny's "Filters fun"
// https://github.com/browny/filters-fun
// Math-related functions such as distance calculations was put to helpers.c
// "Point gradient" mask implementation via simple float array
void RadialGradient(float* mask, CvSize size, float power, float radius, CvPoint center) {
	float maxImageRad = radius * GetMaxDisFromCorners(size, center);

	int x, y;
	for (x = 0; x < size.width ; x++) 
	for (y = 0; y < size.height; y++) {
		float distance = Dist(center, cvPoint(x, y));
		float rawvalue = distance / maxImageRad * power;
		mask[size.width * y + x] = pow(cos(rawvalue), 4);
	}
}

// Calculates average image brightness via weighted rgb distance from black
// Algorithm comparision here http://zoltanb.co.uk/how-to-calculate-the-perceived-brightness-of-a-colour/
float CalcPerceivedBrightness(IplImage* image) {
	float sum = 0;
	int x, y;
	if (image->nChannels == 1) {
		for (x = 0; x < image->width ; x++)
		for (y = 0; y < image->height; y++) {
			sum += cvGetComponent(image, x, y, 0);
		}
	} else {
		for (x = 0; x < image->width ; x++)
		for (y = 0; y < image->height; y++) {
			int r = cvGetComponent(image, x, y, CV_RGB_RED);
			int g = cvGetComponent(image, x, y, CV_RGB_GREEN);
			int b = cvGetComponent(image, x, y, CV_RGB_BLUE);
			sum += sqrt(
				r * r * 0.241 + 
				g * g * 0.691 + 
				b * b * 0.068
			);
		}
	}
	return sum / (image->width * image->height) / 255.0;
}

void Kmeans(IplImage* image, int k) {
	IplImage* normal = cvCreateImage(cvGetSize(image), IPL_DEPTH_32F, image->nChannels);
	cvConvertScale(image, normal, 1/255.0, 0);
	CvMat* points = cvCreateMat(image->width * image->height, 3, CV_32FC1);
    
	float* data = (float*)normal->imageData;
	int step = normal->widthStep/sizeof(float);
	int x, y, c, idx = 0;
	for (y = 0; y < normal->height; y++)
	for (x = 0; x < normal->width; x++, idx++)
	for (c = 0; c < 3; c++) {
		cvSetReal2D(points, idx, c, data[y * step + x * normal->nChannels + c]);
	}
    
	CvMat* labels = cvCreateMat(normal->height * normal->width, 1, CV_32SC1);
	CvMat* centers = cvCreateMat(k, 3, CV_32FC1);
	CvTermCriteria criteria = cvTermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 10, 1.0);
	cvKMeans2(points, k, labels, criteria, 4, NULL, KMEANS_PP_CENTERS, centers, 0);
    
    idx = 0;
	for (y = 0; y < normal->height; y++)
	for (x = 0; x < normal->width; x++, idx++) {
		int cluster = cvGetReal2D(labels, idx, 0);
		for (c = 0; c < 3; c++) {
			float val = cvGetReal2D(centers, cluster, c);
			data[y * step + x * normal->nChannels + c] = val;
		}
	}
    
    cvConvertScale(normal, image, 255.0, 0);
    
    cvReleaseImage(&normal);
    cvReleaseMat(&points);
    cvReleaseMat(&labels);
    cvReleaseMat(&centers);
}