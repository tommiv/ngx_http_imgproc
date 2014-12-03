int  Filter   (IplImage** pointer, char* request, int allowExperiments);
int  Flip     (IplImage** pointer, char* args);
int  Rotate   (IplImage** pointer, char* args);
int  Modulate (IplImage** pointer, char* args);
int  Colorize (IplImage** pointer, char* args);
int  Blur     (IplImage** pointer, char* args);
int  Gamma    (IplImage** pointer, char* args);
int  Vignette (IplImage** pointer, char* args);
int  Gotham   (IplImage** pointer, char* args);
int  Lomo     (IplImage** pointer, char* args);
int  Kelvin   (IplImage** pointer, char* args);
int  Rainbow  (IplImage** pointer, char* args);
int  Scanline (IplImage** pointer, char* args);

int CheckDestructive(char* request);

void ModulateHSV(IplImage* image, int* hsv);
void ApplyGamma(IplImage* image, float gamma);
int* CalculateGammaLUT(float gamma);
void BrightnessContrast(IplImage* image, float br, float ct);

void AlphaBlendAddColor(IplImage* source, int* rgb, float alpha);
void AlphaBlendOver(IplImage* source, IplImage* overlay, float opacity);
void BlendWithPaper(IplImage* source);

void RadialGradient(float* mask, CvSize size, float power, float radius, CvPoint center);

float CalcPerceivedBrightness(IplImage* image);