#include "required.h"
#include "helpers.h"
#include "filters.h"

static const struct {
	char* Name;
	int   (*FilterCallback)(IplImage**, char*);
	int   Experimental;
} CallbackMap[] = {
	{ "flip"     , Flip     , 0 },
	{ "rotate"   , Rotate   , 0 },
	{ "modulate" , Modulate , 0 },
	{ "colorize" , Colorize , 0 },
	{ "blur"     , Blur     , 0 },
	{ "gamma"    , Gamma    , 0 },
	{ "vignette" , Vignette , 1 },
	{ "gotham"   , Gotham   , 1 },
	{ "lomo"     , Lomo     , 1 },
	{ "kelvin"   , Kelvin   , 1 },
	{ "rainbow"  , Rainbow  , 1 },
	{ "scanline" , Scanline , 1 },
};

static int maplen = sizeof(CallbackMap) / sizeof(CallbackMap[0]);

int Filter(IplImage** pointer, char* request, int allowExperiments) {
	char* context = NULL;
	char* type = strtok_r(request, "=", &context);
	if (type == NULL) {
		return IMP_ERROR_NO_SUCH_FILTER;
	}
	char* args = strtok_r(NULL, "=", &context);
	if (args == NULL) {
		return IMP_ERROR_INVALID_ARGS;
	}

	int i;
	for (i = 0; i < maplen; i++) {
		int found = strcmp(type, CallbackMap[i].Name) == 0 && (allowExperiments || !CallbackMap[i].Experimental);
		if (found) {
			return CallbackMap[i].FilterCallback(pointer, args);
		}
	}

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

int Vignette(IplImage** pointer, char* args) {
	char* context = NULL;

	char* _intn = strtok_r(args, ",", &context);
	float intensity = _intn == NULL ? 0.5 : strtof(_intn, NULL);

	char* _rad = strtok_r(NULL, ",", &context);
	float radius = _rad == NULL ? 1.0 : strtof(_rad, NULL);

	IplImage* image = *pointer;

	float* mask = malloc(image->width * image->height * sizeof(float));
	RadialGradient(mask, cvGetSize(image), intensity, radius, cvPoint(image->width / 2, image->height / 2));

	int lightness = 0;
	cvCvtColor(image, image, CV_BGR2Lab);
	int x, y;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++) {
		float source = cvGetComponent(image, x, y, lightness);
		cvSetComponent(image, x, y, lightness, source * mask[image->width * y + x]);
	}
	cvCvtColor(image, image, CV_Lab2BGR);

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
	cvCvtColor(image, image, CV_BGR2HSV);
	int x, y;
	for (x = 0; x < image->width ; x++)
	for (y = 0; y < image->height; y++) {
		int hue   = cvGetComponent(image, x, y, 0) * 2; // extend range to common 360 deg
		int light = cvGetComponent(image, x, y, 2);
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
		cvSetComponent(image, x, y, 0, hue / 2.0);
		cvSetComponent(image, x, y, 1, saturation);
		cvSetComponent(image, x, y, 2, light);
	}

	cvCvtColor(image, image, CV_HSV2BGR);

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
	cvCvtColor(image, image, CV_BGR2HSV);
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
	cvCvtColor(image, image, CV_HSV2BGR);

	return IMP_OK;
}

void ModulateHSV(IplImage* image, int* hsv) {
	cvCvtColor(image, image, CV_BGR2HSV);

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

	cvCvtColor(image, image, CV_HSV2BGR);
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
// Unfortunately x2 this code also produces weird results, but for transparent source
void AlphaBlendOver(IplImage* source, IplImage* overlay, float opacity) {
	double alpha = 1 - opacity;

	CvRect workarea = cvGetImageROI(source);
	
	int maxrow = fmin(overlay->height, source->height - workarea.y);
	int maxcol = fmin(overlay->width , source->width  - workarea.x);

	int row, col;
	for (row = 0; row < maxrow; row++)
	for (col = 0; col < maxcol; col++) {
		int posx = col + workarea.x;
		int posy = row + workarea.y;

		// note it's BGRA
		float sourceB = cvGetComponent(source, posx, posy, 0) / 255.0;
		float sourceG = cvGetComponent(source, posx, posy, 1) / 255.0;
		float sourceR = cvGetComponent(source, posx, posy, 2) / 255.0;
		float sourceA = source->nChannels == 4 ? cvGetComponent(source, posx, posy, 3) / 255.0 : 1;

		float overlayB = cvGetComponent(overlay, col, row, 0) / 255.0;
		float overlayG = cvGetComponent(overlay, col, row, 1) / 255.0;
		float overlayR = cvGetComponent(overlay, col, row, 2) / 255.0;
		float overlayA = overlay->nChannels == 4 ? cvGetComponent(overlay, col, row, 3) / 255.0 : 1;
		overlayA = fmax(overlayA - alpha, 0);

		float targetB = ((sourceA - overlayA) * sourceB) + (overlayA * overlayB);
		float targetG = ((sourceA - overlayA) * sourceG) + (overlayA * overlayG);
		float targetR = ((sourceA - overlayA) * sourceR) + (overlayA * overlayR);
		
		cvSetComponent(source, posx, posy, 0, targetB * 255);
		cvSetComponent(source, posx, posy, 1, targetG * 255);
		cvSetComponent(source, posx, posy, 2, targetR * 255);
		
		if (source->nChannels == 4) {
			float targetA = sourceA + (1.0 - sourceA * overlayA);
			cvSetComponent(source, posx, posy, 3, targetA * 255);
		}
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
			int r = cvGetComponent(image, x, y, 2);
			int g = cvGetComponent(image, x, y, 1);
			int b = cvGetComponent(image, x, y, 0);
			sum += sqrt(
				r * r * 0.241 + 
				g * g * 0.691 + 
				b * b * 0.068
			);
		}
	}
	return sum / (image->width * image->height) / 255.0;
}