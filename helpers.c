#include "required.h"
#include "helpers.h"

int StartsWith(char* haystack, const char* needle) {
	return strstr(haystack, needle) == haystack ? 1 : 0;
}

int ByteCompare(unsigned char* haystack, unsigned char* needle, int length) {
	int i;
    for (i = 0; i < length; i++) {
        if (haystack[i] != needle[i]) {
            return 0;
        }
    }
    return 1;
}

char* RewindArgs(char* pointer, const char* stopper) {
	while ((int)pointer[0] != *stopper) {
		pointer++;
	}
	return ++pointer;
}

void OutputErrorString(const char* msg, ngx_http_request_t* r) {
	#ifdef IMP_DEBUG
		syslog(LOG_ERR, "%s", msg);
	#endif
	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "%s", msg);
}

char* CopyNgxString(ngx_str_t string) {
	char* genericstr = malloc(string.len + 1);
	memcpy(genericstr, string.data, string.len);
	genericstr[string.len] = '\0';
	return genericstr;
}

char* CopyString(char* string) {
	int len = strlen(string);
	char* genericstr = malloc(len + 1);
	memcpy(genericstr, string, len + 1);
	return genericstr;
}

float Dist(CvPoint a, CvPoint b) {
	return sqrt(pow((float)(a.x - b.x), 2) + pow((float)(a.y - b.y), 2));
}

float GetMaxDisFromCorners(CvSize imgSize, CvPoint center) {
	CvPoint corners[4];
	corners[0] = cvPoint(0, 0);
	corners[1] = cvPoint(imgSize.width, 0);
	corners[2] = cvPoint(0, imgSize.height);
	corners[3] = cvPoint(imgSize.width, imgSize.height);
	float maxDis = 0;

	int i;
	for (i = 0; i < 4; ++i) {
		float dis = Dist(corners[i], center);
		if (maxDis < dis) {
			maxDis = dis;
		}
	}
	return maxDis;
}

// cvCvtColor fails on transparent images, cvSplit/cvMerge looks too expensive
// hue is [0, 180], s/v is [0, 255]
void RGB2HSV(IplImage* image) {
	int pitch = image->widthStep;
	unsigned char* y   = (unsigned char*)image->imageData;
	unsigned char* end = y + (pitch * image->height);
	while (y < end) {
		unsigned char* line = y;
		int x = 0;
		int end = image->width * image->nChannels;
		for (; x < end; x+= image->nChannels) {
			int b = line[x];
			int g = line[x + 1];
			int r = line[x + 2];
			int min = MIN3(r, g, b);
			int max = MAX3(r, g, b);
			int delta = max - min;
			int h = 0;
			int s = 0;
			int v = max;
			if (v != 0) {
				s = 255 * delta / v;
			}
			if (s != 0) {
				if (max == r) {
					h = 30 * (g - b) / delta;
				} else if (max == g) {
					h = 60 + 30 * (b - r) / delta;
				} else {
					h = 120 + 30 * (r - g) / delta;
				}
			}
			if (h < 0) h += 180;
			line[x    ] = h;
			line[x + 1] = s;
			line[x + 2] = v;
		}
		y += pitch;
	}
}

void HSV2RGB(IplImage* image) {
	int pitch = image->widthStep;
	unsigned char* y = (unsigned char*)image->imageData;
	unsigned char* end = y + (pitch * image->height);
	while (y < end) {
		unsigned char* line = y;
		int x = 0;
		int end = image->width * image->nChannels;
		for (; x < end; x+= image->nChannels) {
			float h = line[x] * 2;
			float s = line[x + 1];
			float v = line[x + 2];
			int r = 0;
			int g = 0;
			int b = 0;

			if (s == 0) {
				r = g = b = v;
			} else {
				s /= 255;
				h /= 60; // sector [0, 5]
				int i = floor(h);
				float f = h - i; // factorial part of h
				int p = v * (1 - s);
				int q = v * (1 - s * f);
				int t = v * (1 - s * (1 - f));

				switch(i) {
					case 0:
						r = v;
						g = t;
						b = p;
					break;
					case 1:
						r = q;
						g = v;
						b = p;
					break;
					case 2:
						r = p;
						g = v;
						b = t;
					break;
					case 3:
						r = p;
						g = q;
						b = v;
					break;
					case 4:
						r = t;
						g = p;
						b = v;
					break;
					default:
						r = v;
						g = p;
						b = q;
					break;
				}
			}

			line[x    ] = b;
			line[x + 1] = g;
			line[x + 2] = r;
		}
		y += pitch;
	}
}