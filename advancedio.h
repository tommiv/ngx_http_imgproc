#ifdef IMP_FEATURE_ADVANCED_IO

#define GIF_DISPOSAL_UNSPECIFIED 0
#define GIF_DISPOSAL_LEAVE       1
#define GIF_DISPOSAL_BACKGROUND  2
#define GIF_DISPOSAL_PREVIOUS    3

int FiNotImplemented(FREE_IMAGE_FORMAT format);

Album  FiLoadFrames(DecodeRequest req);
Memory FiSaveFrames(EncodeRequest req);

#endif