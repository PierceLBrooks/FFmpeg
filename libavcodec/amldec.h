#ifndef _AMLDEC_H_
#define _AMLDEC_H_

#include "amlqueue.h"
#include <codec.h>
#include <libavutil/buffer.h>

#include <time.h>

#define EXTERNAL_PTS    1
#define SYNC_OUTSIDE    2

#define PTS_FREQ       90000


#define MAX_HEADER_SIZE         4096

// header buffer, used prior each packet output to decoder
typedef struct {
  char data[MAX_HEADER_SIZE];
  int size;
} AMLHeader;


typedef struct {
  AVClass *av_class;

  // this is amcodec codec structure used to push data to the HW codec
  codec_para_t codec;

  // flag to indicate we're at first packet to send the
  // extradata to the codec
  int first_packet;

  // bitstream filter in case we need some annexb compliant stream
  AVBSFContext *bsf;

  // decoder buffer status, used to makes sure we have enough space
  // in the decoder buffer, filling it up too much seems to screw kernel
  struct buf_status buffer_status;

  // number of packets written to cedec for debug information
  int packets_written;

  // number of frames output, useful for debug
  int frame_count;

  // queue for frames returning. We need to queue input packets and dequeue them when we get a frame
  // kernel won't track pts for us, so we use that queue for that purpose
  PacketQueue framequeue;

  // amvideo device file descriptor, used for freerun frame ioctls
  int amv_fd;

  // decoder buffer pfilled ercentage, useful for debug
  double decoder_buffer_pc;
} AMLDecodeContext;

// Functions prototypes
int   ffmal_init_bitstream(AVCodecContext *avctx);
int   ffaml_write_codec_data(AVCodecContext *avctx, char *data, int size);
void  ffaml_checkin_packet_pts(AVCodecContext *avctx, AVPacket *avpkt);
void  ffaml_create_prefeed_header(AVCodecContext *avctx, AVPacket* pkt, AMLHeader *header, char *extradata, int extradatasize);
void  ffaml_get_packet_header(AVCodecContext *avctx, AMLHeader *header, AVPacket *pkt);
void  ffaml_log_decoder_info(AVCodecContext *avctx);

#endif /* _AMLDEC_H_ */
