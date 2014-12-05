#include "required.h"
#include "helpers.h"

#ifdef IMP_FEATURE_ADVANCED_IO

#include "advancedio.h"

static FREE_IMAGE_FORMAT notimplemented[] = {
	FIF_UNKNOWN,
    FIF_ICO,
    FIF_KOALA,
    FIF_LBM,
    FIF_IFF,
    FIF_MNG,
    FIF_PCD,
    FIF_PCX,
    FIF_RAS,
    FIF_WBMP,
    FIF_PSD,
    FIF_CUT,
    FIF_XBM,
    FIF_DDS,
    FIF_HDR,
    FIF_FAXG3,
    FIF_SGI,
    FIF_EXR,
    FIF_PFM,
    FIF_PICT,
    FIF_RAW,
    FIF_JXR
};
int FiNotImplemented(FREE_IMAGE_FORMAT format) {
    int size = sizeof(notimplemented) / sizeof(notimplemented[0]);
    int i;
    for (i = 0; i < size; i++) {
        if (format == notimplemented[i]) {
            return 1;
        }
    }
    return 0;
}

static FREE_IMAGE_FORMAT no32bitsupport[] = {
    FIF_JPEG,
    FIF_J2K,
    FIF_JP2,
    FIF_PBM,
    FIF_PBMRAW,
    FIF_PGM,
    FIF_PGMRAW,
    FIF_PPM,
    FIF_PPMRAW
};
int FiSupports32bit(FREE_IMAGE_FORMAT format) {
    int size = sizeof(no32bitsupport) / sizeof(no32bitsupport[0]);
    int i;
    for (i = 0; i < size; i++) {
        if (format == no32bitsupport[i]) {
            return 0;
        }
    }
    return 1;
}

static FIBITMAP* IplToFI32(IplImage* image) {
    FIBITMAP* frame32b = FreeImage_Allocate(image->width, image->height, 32, 0, 0, 0);
    unsigned char* frame32data = FreeImage_GetBits(frame32b);
    int validtransparency = image->nChannels == 4;
    int bytecount32 = 4;
    int pitch = FreeImage_GetPitch(frame32b);
    int x, y, c;
    for (y = 0; y < image->height; y++) {
        int row = image->height - 1 - y;
        for (x = 0; x < image->width; x++) {
            int offset = y * pitch + x * bytecount32;
            for (c = 0; c < 3; c++) {
                frame32data[offset + c] = cvGetComponent(image, x, row, c);
            }
            frame32data[offset + 3] = validtransparency ? cvGetComponent(image, x, row, 3) : 255;
        }
    }
    return frame32b;
}

static FIBITMAP* IplToFI24(IplImage* image) {
    FIBITMAP* frame24b = FreeImage_Allocate(image->width, image->height, 24, 0, 0, 0);
    unsigned char* frame24data = FreeImage_GetBits(frame24b);
    int bytecount24 = 3;
    int pitch = FreeImage_GetPitch(frame24b);
    int x, y, c;
    for (y = 0; y < image->height; y++) {
        int row = image->height - 1 - y;
        for (x = 0; x < image->width; x++) {
            int offset = y * pitch + x * bytecount24;
            for (c = 0; c < bytecount24; c++) {
                frame24data[offset + c] = cvGetComponent(image, x, row, c);
            }
        }
    }
    return frame24b;
}

static void LoadGIF(Album* result, FIMEMORY* mem, ngx_pool_t* pool, int isdestructive, int page) {
    FIMULTIBITMAP* container = FreeImage_LoadMultiBitmapFromMemory(FIF_GIF, mem, 0);
    if (!container) {
        result->Error = IMP_ERROR_DECODE_FAILED;
        return;
    }
    
    int framecount = FreeImage_GetPageCount(container);
    if (page != -1) {
        result->Count = 1;
        isdestructive = 1;
        if (page > framecount - 1) {
            page = 0;
        }
    } else {
        result->Count = framecount;
    }
    result->Frames = ngx_palloc(pool, framecount * sizeof(Frame));
    
    int* master = NULL;

    int canvasW = 0;
    int canvasH = 0;
    
    int frameid;
    for (frameid = 0; frameid < framecount; frameid++) {
        FIBITMAP* frame = FreeImage_LockPage(container, frameid);
        
        int w = FreeImage_GetWidth(frame);
        int h = FreeImage_GetHeight(frame);

        if (frameid == 0) {
            canvasW = w;
            canvasH = h;
        }
        
        FITAG* tag = NULL;
        FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
        if (tag) {
            const long* time = FreeImage_GetTagValue(tag);
            result->Frames[frameid].Time = *time;
        }
        
        tag = NULL;
        FreeImage_GetMetadata(FIMD_ANIMATION, frame, "DisposalMethod", &tag);
        if (tag) {
            const int* disposeval = FreeImage_GetTagValue(tag);
            result->Frames[frameid].Dispose = *disposeval;
        }

        tag = NULL;
        FreeImage_GetMetadata(FIMD_ANIMATION, frame, "DisposalMethod", &tag);
        if (tag) {
            const int* disposeval = FreeImage_GetTagValue(tag);
            result->Frames[frameid].Dispose = *disposeval;
        }

        short left = 0;
        short top  = 0;
        tag = NULL;
        FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameLeft", &tag);
        if (tag) {
            const short* leftval = FreeImage_GetTagValue(tag);
            left = *leftval;
        }

        tag = NULL;
        FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTop", &tag);
        if (tag) {
            const short* topval = FreeImage_GetTagValue(tag);
            top = *topval;
        }
        
        result->Frames[frameid].TransparencyKey = FreeImage_GetTransparentIndex(frame);
        
        RGBQUAD* palette = FreeImage_GetPalette(frame);
        
        IplImage* image = cvCreateImage(cvSize(canvasW, canvasH), IPL_DEPTH_8U, 4);
        if (!image->imageData) {
            cvReleaseImageHeader(&image);
            result->Error = IMP_ERROR_MALLOC_FAILED;
            return;
        }
        result->Frames[frameid].Image = image;
        
        if (isdestructive && !frameid) {
            master = ngx_palloc(pool, canvasH * canvasW * sizeof(int));
            if (!master) {
                result->Error = IMP_ERROR_MALLOC_FAILED;
            }
        }
        
        int x, y;
        for (y = 0; y < canvasH; y++) {
            unsigned char* row = FreeImage_GetScanLine(frame, canvasH - 1 - y);
            for (x = 0; x < canvasW; x++) {
                int coloridx = row[x];

                // if pixel out of frame bounds, just set it to transparent
                if (x < left || y < top || x > left + w || y > top + h) {
                    coloridx = result->Frames[frameid].TransparencyKey;
                }
                
                if (isdestructive) {
                    int offset   = y * canvasW + x;
                    
                    switch (result->Frames[frameid].Dispose) {
                        case GIF_DISPOSAL_BACKGROUND:
                            if (coloridx == result->Frames[frameid].TransparencyKey) {
                                coloridx = 0; // todo
                            } else {
                                master[offset] = coloridx;
                            }
                        break;
                        case GIF_DISPOSAL_LEAVE:
                        case GIF_DISPOSAL_PREVIOUS: // #todo: while it's probably rare case, it's better to be handled
                        case GIF_DISPOSAL_UNSPECIFIED:
                        default:
                            if (coloridx == result->Frames[frameid].TransparencyKey && frameid > 0) {
                                coloridx = master[offset];
                            } else {
                                master[offset] = coloridx;
                            }
                        break;
                    }
                }
                
                RGBQUAD color = palette[coloridx];
                cvSetComponent(image, x, y, CV_RGB_BLUE , color.rgbBlue);
                cvSetComponent(image, x, y, CV_RGB_GREEN, color.rgbGreen);
                cvSetComponent(image, x, y, CV_RGB_RED  , color.rgbRed);
                cvSetComponent(image, x, y, CV_ALPHA    , coloridx == result->Frames[frameid].TransparencyKey ? 0 : 255);
            }
        }
        
        FreeImage_UnlockPage(container, frame, 0);

        if (frameid == page) {
            break;
        }
    }
    if (master) {
        ngx_pfree(pool, master);
    }
    FreeImage_CloseMultiBitmap(container, 0);
    if (page != -1) {
        Frame requested = result->Frames[page];
        int i;
        for (i = 0; i < page -1; i++) {
            cvReleaseImage(&result->Frames[i].Image);
        }
        ngx_pfree(pool, result->Frames);
        result->Frames = ngx_palloc(pool, sizeof(Frame));
        result->Frames[0] = requested;
    }
}

static void LoadSingle(Album* result, FIMEMORY* mem, ngx_pool_t* pool, FREE_IMAGE_FORMAT format) {
    FIBITMAP* raw = FreeImage_LoadFromMemory(format, mem, 0);
    if (!raw) {
        result->Error = IMP_ERROR_DECODE_FAILED;
        return;
    }
    
    FIBITMAP* fullcolor;
    FREE_IMAGE_COLOR_TYPE type = FreeImage_GetColorType(raw);
    if (type != FIC_RGBALPHA) {
        fullcolor = FreeImage_ConvertTo32Bits(raw);
        FreeImage_Unload(raw);
    } else {
        fullcolor = raw;
    }
    
    int w = FreeImage_GetWidth(fullcolor);
    int h = FreeImage_GetHeight(fullcolor);
    
    IplImage* image = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 4);
    if (!image->imageData) {
        cvReleaseImageHeader(&image);
        result->Error = IMP_ERROR_MALLOC_FAILED;
        return;
    }
    
    result->Count = 1;
    result->Frames = ngx_palloc(pool, sizeof(Frame));
    result->Frames[0].Image = image;
    result->Frames[0].Time = result->Frames[0].TransparencyKey = result->Frames[0].Dispose = 0;
    
    unsigned char* data = FreeImage_GetBits(fullcolor);
    const int bytecount32 = 4;
    int x, y, c;
    for (y = 0; y < h; y++) {
        int row = h - 1 - y;
        for (x = 0; x < w; x++) {
            int offset = y * w * bytecount32 + x * bytecount32;
            for (c = 0; c < bytecount32; c++) {
                cvSetComponent(image, x, row, c, data[offset + c]);
            }
        }
    }
    
    FreeImage_Unload(fullcolor);
}

Album FiLoadFrames(DecodeRequest req) {
    Album result;
    result.Error = 0;
    result.Count = 0;
    
    FIMEMORY* mem = FreeImage_OpenMemory(req.Buffer, req.Length);
    
    if (req.Format == FIF_GIF) {
        LoadGIF(&result, mem, req.Pool, req.IsDestructive, req.Page);
    } else {
        LoadSingle(&result, mem, req.Pool, req.Format);
    }
    
    FreeImage_CloseMemory(mem);
    return result;
}

static void SaveGIF(Album* source, ngx_pool_t* pool, Memory* result, int params) {
    FIMEMORY* mem = FreeImage_OpenMemory(NULL, 0);
    FIMULTIBITMAP* container = FreeImage_LoadMultiBitmapFromMemory(FIF_GIF, mem, 0);
    
    int palettesize = 255;
    RGBQUAD* palette = NULL;
    
    int frameid;
    for (frameid = 0; frameid < source->Count; frameid++) {
        IplImage* image = source->Frames[frameid].Image;
        FIBITMAP* frame32b = IplToFI32(image);

        // Now we are fucked, FI doesn't support quantization of transparent bitmap
        FIBITMAP* frame24b = FreeImage_ConvertTo24Bits(frame32b);
        FIBITMAP* page = FreeImage_ColorQuantizeEx(frame24b, FIQ_NNQUANT, palettesize, palette == NULL ? 0 : palettesize, palette);
        if (palette == NULL && source->Count > 1) {
            RGBQUAD* globalpalette = FreeImage_GetPalette(page);
            int size = (palettesize + 1) * sizeof(RGBQUAD);
            palette = malloc(size);
            memcpy(palette, globalpalette, size);
        }
        
        FreeImage_SetTransparent(page, 1);
        FreeImage_SetTransparentIndex(page, palettesize);

        // Restore transparency
        unsigned char* frame32data = FreeImage_GetBits(frame32b);
        unsigned int   pitch32b = FreeImage_GetPitch(frame32b);
        unsigned char* frameIndexData = FreeImage_GetBits(page);
        unsigned int   pitchIndex = FreeImage_GetPitch(page);
        int x, y;
        for (y = 0; y < image->height; y++, frame32data += pitch32b, frameIndexData += pitchIndex) {
            RGBQUAD* src = (RGBQUAD*)frame32data;
            BYTE*    tgt = (BYTE*)frameIndexData;
            for (x = 0; x < image->width; x++) {
                if (src[x].rgbReserved == 0) {
                    tgt[x] = palettesize;
                }
            }
        }
        
        FITAG* frameTimeTag = FreeImage_CreateTag();
        FreeImage_SetTagKey(frameTimeTag, "FrameTime");
        FreeImage_SetTagType(frameTimeTag, FIDT_LONG);
        FreeImage_SetTagCount(frameTimeTag, 1);
        FreeImage_SetTagLength(frameTimeTag, 4); // sizeof(long) does not work since it probably returns 8
        FreeImage_SetTagValue(frameTimeTag, &source->Frames[frameid].Time);
        FreeImage_SetMetadata(FIMD_ANIMATION, page, FreeImage_GetTagKey(frameTimeTag), frameTimeTag);
        FreeImage_DeleteTag(frameTimeTag);
        
        FITAG* disposalTag = FreeImage_CreateTag();
        FreeImage_SetTagKey(disposalTag, "DisposalMethod");
        FreeImage_SetTagType(disposalTag, FIDT_BYTE);
        FreeImage_SetTagCount(disposalTag, 1);
        FreeImage_SetTagLength(disposalTag, 1);
        FreeImage_SetTagValue(disposalTag, &source->Frames[frameid].Dispose);
        FreeImage_SetMetadata(FIMD_ANIMATION, page, FreeImage_GetTagKey(disposalTag), disposalTag);
        FreeImage_DeleteTag(disposalTag);
        
        FreeImage_AppendPage(container, page);
        FreeImage_Unload(frame32b);
        FreeImage_Unload(frame24b);
        FreeImage_Unload(page);
    }
    
    free(palette);

    FIMEMORY* destination = FreeImage_OpenMemory(NULL, 0);
    int encoderes = FreeImage_SaveMultiBitmapToMemory(FIF_GIF, container, destination, params);
    
    if (encoderes) {
        unsigned char* buffer = NULL;
        unsigned int length = 0;
        FreeImage_AcquireMemory(destination, &buffer, &length);
        result->Buffer = ngx_palloc(pool, length);
        memcpy(result->Buffer, buffer, length);
        result->Length = length;
    } else {
        result->Error = IMP_ERROR_ENCODE_FAILED;
    }
    
    FreeImage_CloseMemory(mem);
    FreeImage_CloseMemory(destination);
    
    FreeImage_CloseMultiBitmap(container, 0);
}

static void SaveSingle(Album* source, ngx_pool_t* pool, Memory* result, FREE_IMAGE_FORMAT format, int params) {
    IplImage* image = source->Frames[0].Image;
    FIBITMAP* frame = FiSupports32bit(format) ? IplToFI32(image) : IplToFI24(image);
    
    FIMEMORY* destination = FreeImage_OpenMemory(NULL, 0);
    int encoderes = FreeImage_SaveToMemory(format, frame, destination, params);
    if (encoderes) {
        unsigned char* buffer = NULL;
        unsigned int length = 0;
        FreeImage_AcquireMemory(destination, &buffer, &length);
        result->Buffer = ngx_palloc(pool, length);
        memcpy(result->Buffer, buffer, length);
        result->Length = length;
    } else {
        result->Error = IMP_ERROR_ENCODE_FAILED;
    }
    
    FreeImage_CloseMemory(destination);
    FreeImage_Unload(frame);
}

Memory FiSaveFrames(EncodeRequest req) {
    Memory result;
    result.Error  = 0;
    result.Length = 0;
    
    if (req.Format == FIF_GIF) {
        SaveGIF(req.Album, req.Pool, &result, req.Flags);
    } else {
        SaveSingle(req.Album, req.Pool, &result, req.Format, req.Flags);
    }
    
    return result;
}

#endif