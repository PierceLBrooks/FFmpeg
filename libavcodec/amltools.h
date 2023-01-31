
#ifndef _AMLTOOLS_H_
#define _AMLTOOLS_H_

#include "avcodec.h"
#include "amports/vformat.h"

// system IO operation to set kernel driver parameters
int amlsysfs_write_string(AVCodecContext *avctx, const char *path, const char *value);
int amlsysfs_write_int(AVCodecContext *avctx, const char *path, int value);
int64_t amlsysfs_read_int(AVCodecContext *avctx, const char *path, int base);

// amcodec format / decoder conversion utilities between amcodec & ffmpeg
vformat_t aml_get_vformat(AVCodecContext *avctx);
vdec_type_t aml_get_vdec_type(AVCodecContext *avctx);
const char *aml_get_vformat_name(vformat_t format);
const char *aml_get_vdec_name(vdec_type_t vdec);

#endif /* _AMLTOOLS_H_*/
