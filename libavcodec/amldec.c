/*
 * MMAL Video Decoder
 * Copyright (c) 2016 Lionel Chazallon
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * AMLogic Video Decoder
 */

#include "avcodec.h"
#include "aml.h"
#include "internal.h"
#include "amltools.h"

#include "libavutil/atomic.h"
#include "libavutil/avassert.h"
#include "libavutil/buffer.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include <unistd.h>
#include "amldec.h"

#undef DEBUG
#define DEBUG (0)

void ffaml_log_decoder_info(AVCodecContext *avctx)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;

  av_log(avctx, AV_LOG_DEBUG, "Decoder buffer : filled %d bytes (%f%%), read=%d, write=%d, pkts written=%d\n",
        aml_context->buffer_status.data_len, 
        (double)(aml_context->buffer_status.data_len * 100) / (double)(aml_context->buffer_status.data_len + aml_context->buffer_status.free_len),
        aml_context->buffer_status.read_pointer, aml_context->buffer_status.write_pointer, aml_context->packets_written);

  //av_log(avctx, AV_LOG_DEBUG, "Decoder queue : %d packets\n", aml_context->writequeue.size);
}

int ffmal_init_bitstream(AVCodecContext *avctx)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
  int ret = 0;

  if (!aml_context->bsf)
  {
    const AVBitStreamFilter *bsf;

    // check if we need a bitstream filter
    switch(avctx->codec_id)
    {
      case AV_CODEC_ID_H264:
        bsf = av_bsf_get_by_name("h264_mp4toannexb");
      break;

      case AV_CODEC_ID_HEVC:
        bsf = av_bsf_get_by_name("hevc_mp4toannexb");
      break;

    default:
      av_log(avctx, AV_LOG_DEBUG, "Not using any bitstream filter\n");
      return 0;
    }

    if(!bsf)
        return AVERROR_BSF_NOT_FOUND;

    av_log(avctx, AV_LOG_DEBUG, "using bitstream filter %s\n", bsf->name);

    if ((ret = av_bsf_alloc(bsf, &aml_context->bsf)))
        return ret;

    if (((ret = avcodec_parameters_from_context(aml_context->bsf->par_in, avctx)) < 0) ||
        ((ret = av_bsf_init(aml_context->bsf)) < 0))
    {
        av_bsf_free(&aml_context->bsf);
        return ret;
    }
  }

  return 0;
}

int ffaml_write_codec_data(AVCodecContext *avctx, char *data, int size)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
  codec_para_t *pcodec  = &aml_context->codec;
  int bytesleft = size;
  int written = 0;

#if DEBUG
  av_log(avctx, AV_LOG_DEBUG, "codec -> (%d bytes) : %x %x %x %x\n", size, data[0], data[1], data[2], data[3]);
#endif

  while (bytesleft)
  {
    written = codec_write(pcodec, data, bytesleft);
    if (written < 0)
    {
      av_log(avctx, AV_LOG_ERROR, "failed to write data to codec (code = %d)\n", written);
      usleep(10);
    }
    else
    {
      data += written;
      bytesleft -= written;
    }
  }

  return 0;
}


//int ffaml_write_pkt_data(AVCodecContext *avctx, AVPacket *avpkt, AMLHeader *header)
//{
//  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
//  codec_para_t *pcodec  = &aml_context->codec;
//  int bytesleft = avpkt->size + header->size;
//  int written = 0;
//  int ret = 0;
//  struct buf_status vdec_stat;

//  // grab the decoder buff status, if we on't have enough buffer space
//  // we will sleep a bit, that way write never hangs up
//  do
//  {
//    ret = codec_get_vbuf_state(pcodec, &vdec_stat);
//    if (ret < 0)
//    {
//      av_log(avctx, AV_LOG_ERROR, "failed to query video decoder buffer state(code = %d)\n", ret);
//      return -1;
//    }

//    if (vdec_stat.free_len < bytesleft)
//    {
//      // video decoder buffer stuffed, lets sleeps and retry
//      usleep(10000);
//    }
//  } while (vdec_stat.free_len < bytesleft);

//  // check in the pts
//  ffaml_checkin_packet_pts(avctx, avpkt);

//  // write the header if any
//  if (header->size)
//    codec_write(pcodec, header->data, header->size);

//  av_log(avctx, AV_LOG_DEBUG, "Wrote header (%d bytes), %x %x %x %x\n", header->size, header->data[0], header->data[1], header->data[2], header->data[3]);
//  av_log(avctx, AV_LOG_DEBUG, "Wrote data (%d bytes), %x %x %x %x\n", avpkt->size, avpkt->data[0], avpkt->data[1], avpkt->data[2], avpkt->data[3]);
//  // actually write the packet data
//  while (bytesleft)
//  {
//    written = codec_write(pcodec, avpkt->data + avpkt->size - bytesleft, bytesleft);
//    if (written < 0)
//    {
//      av_log(avctx, AV_LOG_ERROR, "failed to write data to codec (code = %d)\n", written);
//      usleep(10);
//    }
//    else
//    {
//      bytesleft -= written;
//    }
//  }

//  aml_context->packets_written++;

//  return 0;
//}

void ffaml_create_prefeed_header(AVCodecContext *avctx, AVPacket* pkt, AMLHeader *header, char *extradata, int extradatasize)
{
  switch(aml_get_vformat(avctx))
  {
    case VFORMAT_VC1:
      memcpy(header->data, extradata+1, extradatasize-1);
      header->size = extradatasize-1;
    break;

    default:
      // just copy over the extradata for those
      memcpy(header->data, extradata, extradatasize);
      header->size = extradatasize;
    break;
  }
}

void ffaml_checkin_packet_pts(AVCodecContext *avctx, AVPacket *avpkt)
{
  int ret;
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;

  double pts = (avpkt->pts * PTS_FREQ) * av_q2d(avctx->time_base);
  //aml_context->last_checkin_pts = avpkt->pts * av_q2d(avctx->time_base);
  if ((ret = codec_checkin_pts(&aml_context->codec, pts)) < 0)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to checkin the pts (code = %d)\n", ret);
  }
}

static av_cold int ffaml_init_decoder(AVCodecContext *avctx)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
  codec_para_t *pcodec  = &aml_context->codec;
  int ret = 0;

  // reset the first packet attribute
  aml_context->first_packet = 1;
  aml_context->bsf = NULL;
  //aml_context->last_checkin_pts = 0;
  aml_context->packets_written = 0;
  //ffaml_init_queue(&aml_context->writequeue);

  // setup the codec structure for amcodec
  memset(pcodec, 0, sizeof(codec_para_t));
  memset(&aml_context->buffer_status, 0, sizeof(aml_context->buffer_status));
  memset(&aml_context->decoder_status, 0, sizeof(aml_context->decoder_status));
  //memset(&aml_context->prefeed_header, 0, sizeof(aml_context->prefeed_header));
  memset(&aml_context->ion_context, 0, sizeof(aml_context->ion_context));

  ret = aml_ion_open(avctx, &aml_context->ion_context);
  if (ret < 0)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to init ion driver\n");
    return -1;
  }

  pcodec->stream_type = STREAM_TYPE_ES_VIDEO;
  pcodec->has_video = 1;
  pcodec->video_type = aml_get_vformat(avctx);
  pcodec->am_sysinfo.format = aml_get_vdec_type(avctx);
  pcodec->am_sysinfo.param = (void*)(SYNC_OUTSIDE);
  pcodec->am_sysinfo.width = avctx->width;
  pcodec->am_sysinfo.height = avctx->height;

  // checks if codec formats and decoder have been properly setup
  if (pcodec->video_type == VFORMAT_UNSUPPORT)
  {
    av_log(avctx, AV_LOG_ERROR, "Cannot determine proper video type : Codec ID=%d\n", avctx->codec_id);
    return -1;
  }

  if (pcodec->am_sysinfo.format == VIDEO_DEC_FORMAT_UNKNOW)
  {
    av_log(avctx, AV_LOG_ERROR, "Cannot determine proper video decder : Codec TAG=0x%x\n", avctx->codec_tag);
    return -1;
  }

  ret = codec_init(pcodec);
  if (ret != CODEC_ERROR_NONE)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to init amcodec decoder\n");
    return -1;
  }

  codec_resume(pcodec);

  // Enable double write mode for H265, so that it outputs NV21
  amlsysfs_write_int(avctx, "/sys/module/amvdec_h265/parameters/double_write_mode", 1);

 // eventually create a bistream filter for formats tha require it
 ret = ffmal_init_bitstream(avctx);
  if (ret != 0)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to init AML bitstream\n");
    return -1;
  }

  av_log(avctx, AV_LOG_DEBUG, "amcodec intialized successfully (%s / %s)\n",
         aml_get_vformat_name(pcodec->video_type),
         aml_get_vdec_name(pcodec->am_sysinfo.format));

  return 0;
}

static av_cold int ffaml_close_decoder(AVCodecContext *avctx)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
  codec_para_t *pcodec  = &aml_context->codec;
  if (pcodec)
  {
    codec_close(pcodec);
  }

  // free bitstream
  if (aml_context->bsf)
    av_bsf_free(&aml_context->bsf);

  // close ion driver
  aml_ion_close(avctx, &aml_context->ion_context);

  av_log(avctx, AV_LOG_DEBUG, "amcodec closed successfully\n");
  return 0;
}

static void ffaml_release_frame(void *opaque, uint8_t *data)
{
  AMLBuffer *pbuffer = (AMLBuffer *)data;
  AMLDecodeContext *avctx = (AMLDecodeContext *)opaque;

  if (pbuffer)
  {
    av_log(avctx, AV_LOG_DEBUG, "Releasing Buffer #%d\n", pbuffer->index);
    pbuffer->requeue = 1;
  }
}

void ffaml_get_packet_header(AVCodecContext *avctx, AMLHeader *header, AVPacket *pkt)
{

  header->size = 0;

  switch(aml_get_vformat(avctx))
  {
    case VFORMAT_VC1:
      if ((pkt->data[0]==0x0) && (pkt->data[1]==0x0) && (pkt->data[2]==0x1) && \
          ((pkt->data[3]==0xD) || (pkt->data[3]==0xF)))
      {
        // then header is already there, we don't need it
      }
      else
      {
        // otherwise, add the header
        header->data[0] = 0x0;
        header->data[1] = 0x0;
        header->data[2] = 0x1;
        header->data[3] = 0xd;
        header->size = 4;
      }
    break;
  }
}

static int ffaml_decode(AVCodecContext *avctx, void *data, int *got_frame,
                         AVPacket *avpkt)
{
  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;
  AMLHeader header = { 0 } ;
  codec_para_t *pcodec  = &aml_context->codec;
  int ret = 0;
  AVPacket filter_pkt = {0};
  AVPacket filtered_packet = {0};
  //AVPacket *pkt;
  AVFrame *frame = data;
  uint8_t *extradata;
  int extradata_size;
  int bufferid;
  AMLBuffer *pbuffer;

#if DEBUG
  ffaml_log_decoder_info(avctx);
#endif

  int mpvcount= 0;
  int decount =0;
  for (int i=0; i < AML_BUFFER_COUNT; i++)
  {

    if (!aml_context->ion_context.buffers[i].queued)
    {
      if (aml_context->ion_context.buffers[i].requeue)
      {
        av_log(avctx, AV_LOG_DEBUG, "Requeuing Buffer #%d\n", i);
        aml_ion_queue_buffer(avctx, &aml_context->ion_context, &aml_context->ion_context.buffers[i]);
        decount++;
      }
      else mpvcount++;
    }
    else decount++;
  }

  av_log(avctx, AV_LOG_DEBUG, "Buffers : decoder %d / MPV : %d\n", decount, mpvcount);

  if ((avpkt) && (avpkt->data))
  {

    // first we bitstream the packet if it's required
    if (aml_context->bsf)
    {
      // if we use a bitstream filter, then use it
      if ((ret = av_packet_ref(&filter_pkt, avpkt)) < 0)
          return ret;

      if ((ret = av_bsf_send_packet(aml_context->bsf, &filter_pkt)) < 0) {
          av_packet_unref(&filter_pkt);
          return ret;
      }

      if ((ret = av_bsf_receive_packet(aml_context->bsf, &filtered_packet)) < 0)
          return ret;

      avpkt = &filtered_packet;
      extradata = aml_context->bsf->par_out->extradata;
      extradata_size = aml_context->bsf->par_out->extradata_size;
    }
    else
    {
      // otherwise, we shouldn't need it, just use plain extradata
      extradata = avctx->extradata;
      extradata_size = avctx->extradata_size;
    }

    // we need to write the prefeed header on first packet
    if (aml_context->first_packet)
    {
      // we need make a header from extradata to prefeed the decoder
      ffaml_create_prefeed_header(avctx, avpkt, &header, extradata, extradata_size);

      if (header.size > 0)
      {
        ret = ffaml_write_codec_data(avctx, header.data, header.size);
        if (ret < 0)
        {
          av_log(avctx, AV_LOG_ERROR, "Failed to write prefeed header\n");
          return -1;
        }
      }

      aml_context->first_packet = 0;
    }

    // now write the packet header if any
    ffaml_get_packet_header(avctx, &header, avpkt);
    if (header.size > 0)
    {
      ret = ffaml_write_codec_data(avctx, header.data, header.size);
      if (ret < 0)
      {
        av_log(avctx, AV_LOG_ERROR, "Failed to write packet header\n");
        return -1;
      }
    }

    // now write packet data
    if (avpkt->size > 0)
    {

      av_log(avctx, AV_LOG_DEBUG, "Writing frame with pts=%f, wpkt=%d, size=%d header=%d\n",
             avpkt->pts * av_q2d(avctx->time_base),
             aml_context->packets_written,
             avpkt->size,
             header.size);

      // check in the pts
      ffaml_checkin_packet_pts(avctx, avpkt);

      ret = ffaml_write_codec_data(avctx, avpkt->data, avpkt->size);
      if (ret < 0)
      {
        av_log(avctx, AV_LOG_ERROR, "Failed to write packet data\n");
        return -1;
      }

      aml_context->packets_written++;
    }
  }

  // grab the video decoder status
  ret = codec_get_vdec_state(pcodec, &aml_context->decoder_status);
  if (ret < 0)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to retrieve video decoder status (code=%d)\n", ret);
    return -1;
  }

  // Now that dodec has been fed, we need to check if we have an available frame to dequeue
  if (aml_context->packets_written >= MIN_DECODER_PACKETS)
    bufferid = aml_ion_dequeue_buffer(avctx, &aml_context->ion_context, got_frame, MAX_DEQUEUE_TIMEOUT_MS);

  if (frame->buf[0])
    av_log(avctx, AV_LOG_ERROR, "Frame Buf[0] is not NULL\n");

  if (*got_frame)
  {
    pbuffer =  &aml_context->ion_context.buffers[bufferid];

    frame->format = AV_PIX_FMT_AML;
    frame->width = pbuffer->width;
    frame->height = pbuffer->height;
    frame->linesize[0] = pbuffer->stride;
    frame->buf[0] = av_buffer_create((uint8_t *)pbuffer, 0, ffaml_release_frame, avctx, AV_BUFFER_FLAG_READONLY);
    frame->data[0] = (uint8_t*)pbuffer;
    frame->pkt_pts = pbuffer->pts;
    pbuffer->requeue = 0;

//#if DEBUG
    av_log(avctx, AV_LOG_DEBUG, "Sending Buffer %d (pts=%f) (%dx%d)!!!\n",
           bufferid, aml_context->ion_context.buffers[bufferid].pts * av_q2d(avctx->time_base), frame->width, frame->height);
//#endif

    //aml_ion_queue_buffer(avctx, &aml_context->ion_context, &aml_context->ion_context.buffers[bufferid]);
  }

   return 0;
}

static void ffaml_flush(AVCodecContext *avctx)
{
  int ret;

  AMLDecodeContext *aml_context = (AMLDecodeContext*)avctx->priv_data;

  av_log(avctx, AV_LOG_DEBUG, "Flushing ...\n");
  //ffaml_queue_clear(avctx, &aml_context->writequeue);

  // reset our codec to clean buffers
  ret = codec_reset(&aml_context->codec);
  if (ret < 0)
  {
    av_log(avctx, AV_LOG_ERROR, "failed to reset codec (code = %d)\n", ret);
    aml_context->first_packet = 1;
  }

  aml_context->packets_written = 0;

  // flush ion, so that all buffers get dequeued and requeued
  ret = aml_ion_flush(avctx, &aml_context->ion_context);
  if (ret < 0)
  {
    av_log(avctx, AV_LOG_ERROR, "Failed to flush ION V4L (code=%d)\n", ret);
    return;
  }

  av_log(avctx, AV_LOG_DEBUG, "Flushing done.\n");
}


#define FFAML_DEC_HWACCEL(NAME, ID) \
  AVHWAccel ff_##NAME##_aml_hwaccel = { \
      .name       = #NAME "_aml", \
      .type       = AVMEDIA_TYPE_VIDEO,\
      .id         = ID, \
      .pix_fmt    = AV_PIX_FMT_AML,\
  };

#define FFAML_DEC_CLASS(NAME) \
    static const AVClass ffaml_##NAME##_dec_class = { \
        .class_name = "aml_" #NAME "_dec", \
        .version    = LIBAVUTIL_VERSION_INT, \
    };

#define FFAML_DEC(NAME, ID) \
    FFAML_DEC_CLASS(NAME) \
    FFAML_DEC_HWACCEL(NAME, ID) \
    AVCodec ff_##NAME##_aml_decoder = { \
        .name           = #NAME "_aml", \
        .long_name      = NULL_IF_CONFIG_SMALL(#NAME " (aml)"), \
        .type           = AVMEDIA_TYPE_VIDEO, \
        .id             = ID, \
        .priv_data_size = sizeof(AMLDecodeContext), \
        .init           = ffaml_init_decoder, \
        .close          = ffaml_close_decoder, \
        .decode         = ffaml_decode, \
        .flush          = ffaml_flush, \
        .priv_class     = &ffaml_##NAME##_dec_class, \
        .capabilities   = AV_CODEC_CAP_DELAY, \
        .caps_internal  = FF_CODEC_CAP_SETS_PKT_DTS, \
        .pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P /*AV_PIX_FMT_AML*/, \
                                                         AV_PIX_FMT_NONE}, \
    };

FFAML_DEC(h264, AV_CODEC_ID_H264)

FFAML_DEC(hevc, AV_CODEC_ID_HEVC)

FFAML_DEC(mpeg4, AV_CODEC_ID_MPEG4)

FFAML_DEC(vc1, AV_CODEC_ID_VC1)
