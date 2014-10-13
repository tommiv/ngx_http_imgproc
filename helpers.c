#include "required.h"
#include "helpers.h"

int StartsWith(char* haystack, const char* needle) {
	return strstr(haystack, needle) == haystack ? 1 : 0;
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