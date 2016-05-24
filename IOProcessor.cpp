/*
 * IOProcessor.cpp
 *
 *  Created on: 2016年5月19日
 *      Author: leo
 */

#include "IOProcessor.h"
using namespace std;

/**
 * Convert an error code into a text message.
 * @param error Error code to be converted
 * @return Corresponding error text (not thread-safe)
 */
static const char *get_error_text(const int error) {
	static char error_buffer[255];
	av_strerror(error, error_buffer, sizeof(error_buffer));
	return error_buffer;
}

SafetyDataArea::SafetyDataArea() :
		m_StartTime(0), m_ShiftTime(0), m_SegmentNum(0) {
} // SafetyDataArea::SafetyDataArea

SafetyDataArea::~SafetyDataArea() {
} //SafetyDataArea::~SafetyDataArea()

void SafetyDataArea::SetStartTime(const int64_t t) {
	AutomaticMutex guard(m_mutex);
	m_StartTime = t;
}

void SafetyDataArea::SetSegment(const int64_t dts, const int n) {
	AutomaticMutex guard(m_mutex);
	seg.dts = dts;
	seg.num = n;
	m_segArray.push_back(seg);
	m_SegmentNum = n;
}

void SafetyDataArea::FreeSegment() {
	AutomaticMutex guard(m_mutex);
	int64_t new_StartTime = m_segArray.front().dts;
	m_ShiftTime -= new_StartTime - m_StartTime;
	m_StartTime = new_StartTime;
	m_segArray.pop_front();
}

int SafetyDataArea::GetSegmentNum() {
	AutomaticMutex guard(m_mutex);
	if (!m_segArray.empty()) {
		seg = m_segArray.front();
		return seg.num;
	}

	return -1;
}

int SafetyDataArea::GetFinalSegmentNum() {
	AutomaticMutex guard(m_mutex);
	if (!m_segArray.empty()) {
		seg = m_segArray.back();
		return seg.num;
	}

	return -1;
}

int64_t SafetyDataArea::GetSegmentStartTime() {
	AutomaticMutex guard(m_mutex);
	if (!m_segArray.empty()) {
		seg = m_segArray.front();
		return seg.dts;
	}

	return -1;
}

void SafetyDataArea::CalculateShiftTime(const int64_t t) {
	AutomaticMutex guard(m_mutex);
	m_ShiftTime = t - m_StartTime;
}

bool SafetyDataArea::CheckShiftTime() {
	AutomaticMutex guard(m_mutex);
	if (m_ShiftTime >= SHIFT_TIME_IN_SECOND)
		return true;

	return false;
}

int64_t SafetyDataArea::GetShiftTime() {
	AutomaticMutex guard(m_mutex);

	return m_ShiftTime;
}

CFFmpegIOProcessor::CFFmpegIOProcessor() :
		ofmt_ctx(NULL), ifmt_ctx(NULL), h264_in_bsf(NULL), aac_bsf(NULL), in_video_index(
				0), in_audio_index(0), out_video_index(0), out_audio_index(0) {

}

CFFmpegIOProcessor::~CFFmpegIOProcessor() {
	Close();
}

void CFFmpegIOProcessor::Close() {
	fprintf(stderr, "Close IOProcessor.\n");
	if (ifmt_ctx)
		avformat_close_input(&ifmt_ctx);
	if (h264_in_bsf)
		av_bitstream_filter_close(h264_in_bsf);
	if (aac_bsf)
		av_bitstream_filter_close(aac_bsf);
	if (ofmt_ctx) {
		av_write_trailer(ofmt_ctx);
		//		 Free the streams
		for (int i = 0; i < ofmt_ctx->nb_streams; i++) {
			avcodec_close(ofmt_ctx->streams[i]->codec);
		}
		if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}
}

int CFFmpegIOProcessor::InitInputFormatContext(const char *srcName) {
	int ret = 0;
	AVDictionary *format_opts = NULL;
	AVDictionary **opts;

	av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);

	if ((ret = avformat_open_input(&ifmt_ctx, srcName, NULL, &format_opts))
			< 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file .\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information .\n");
		return ret;
	}
	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *stream;
		AVCodecContext *codec_ctx;
		stream = ifmt_ctx->streams[i];
		codec_ctx = stream->codec;
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
			in_video_index = i;
			if (strstr(ifmt_ctx->iformat->name, "mp4")
					|| strstr(ifmt_ctx->iformat->name, "flv")) {
				h264_in_bsf = av_bitstream_filter_init("h264_mp4toannexb");
				if (!h264_in_bsf) {
					printf("Could not aquire h264_mp4toannexb filter.\n");
					return -1;
				}
			}
		} else if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			in_audio_index = i;
		}
	}

	av_dump_format(ifmt_ctx, 0, srcName, 0);

	return 0;
}

int CFFmpegIOProcessor::InitOutputFormatContext(sInputParams *pParams, const int segment_start_num) {
	int ret;
	AVStream *out_stream;
	AVStream *in_stream;
	AVDictionaryEntry *title = NULL, *provider = NULL;
	AVDictionary *options = NULL;
	const char *service_name = NULL, *provider_name = NULL;
	if (pParams->format) {
		avformat_alloc_output_context2(&ofmt_ctx, NULL, pParams->format,
				pParams->strDstFile);
	} else
		avformat_alloc_output_context2(&ofmt_ctx, NULL, "segment",
				pParams->strDstFile);

	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output format context\n");
		return -1;
	}
	for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
		out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
			return -1;
		}
		in_stream = ifmt_ctx->streams[i];
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			printf("Failed to copy context from input to output stream.\n");
			return -1;
		}
		if (((0 == strcmp(ofmt_ctx->oformat->name, "flv"))
				|| (0 == strcmp(ofmt_ctx->oformat->name, "mp4")))
				&& (out_stream->codec->codec_id == AV_CODEC_ID_AAC)) {
			aac_bsf = av_bitstream_filter_init("aac_adtstoasc");
			if (!aac_bsf) {
				printf("Could not aquire aac_adtstoasc filter.\n");
				return -1;
			}
		}
		out_stream->time_base = av_add_q(out_stream->codec->time_base,
				(AVRational ) { 0, 1 });
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	if (!h264_in_bsf) {
		title = av_dict_get(ifmt_ctx->programs[0]->metadata, "service_name",
		NULL, 0);
		provider = av_dict_get(ifmt_ctx->programs[0]->metadata,
				"service_provider",
				NULL, 0);
	}
	service_name =
			(title && (0 != strcmp(title->value, "Service01"))) ?
					title->value : DEFAULT_SERVICE_NAME;
	provider_name =
			(provider && (0 != strcmp(provider->value, "FFmpeg"))) ?
					provider->value : DEFAULT_PROVIDER_NAME;
	av_dict_set(&ofmt_ctx->metadata, "service_name", service_name, 0);
	av_dict_set(&ofmt_ctx->metadata, "service_provider", provider_name, 0);

	av_dump_format(ofmt_ctx, 0, pParams->strDstFile, 1);

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, pParams->strDstFile, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'",
					pParams->strDstFile);
			return ret;
		}
	}

	char val;
	sprintf(&val, "%d", pParams->nSegWrap);
	av_opt_set(ofmt_ctx->priv_data, "segment_wrap", &val, 0);


	sprintf(&val, "%d", segment_start_num);
	av_opt_set(ofmt_ctx->priv_data, "segment_start_number", &val, 0);

	ret = avformat_write_header(ofmt_ctx, &options);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR,
				"Error occurred when write format header.\n");
		return ret;
	}

	return 0;
}

int CFFmpegIOProcessor::ResetInputFormatContext(const char *srcName) {

	fprintf(stderr, "Reset Input FormatContext.\n");

	if (ifmt_ctx)
		avformat_close_input(&ifmt_ctx);
	if (h264_in_bsf)
		av_bitstream_filter_close(h264_in_bsf);

	if(InitInputFormatContext(srcName) < 0)
		return -1;

	return 0;
}

int CFFmpegIOProcessor::ResetOutputFormatContext(sInputParams *pParams , const int segment_start_num) {

	fprintf(stderr, "Reset Output FormatContext.\n");

	if (aac_bsf)
		av_bitstream_filter_close(aac_bsf);
	if (ofmt_ctx) {
		av_write_trailer(ofmt_ctx);
		//		 Free the streams
		for (int i = 0; i < ofmt_ctx->nb_streams; i++) {
			avcodec_close(ofmt_ctx->streams[i]->codec);
		}
		if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}

	if(InitOutputFormatContext(pParams, segment_start_num) < 0)
		return -1;

	return 0;
}

AVRational CFFmpegIOProcessor::GetStreamTimeBase() {

	return ifmt_ctx->streams[in_video_index]->time_base;
}

AVPacket* CFFmpegIOProcessor::ReadOneAVPacket() {
	int ret = 0;

	if ((ret = av_read_frame(ifmt_ctx, &pkt)) < 0) {
		if (ret == AVERROR_EOF) {
			av_log(NULL, AV_LOG_WARNING, "End of file.\n");
//				if ((ret = av_seek_frame(ifmt_ctx, in_video_index, 0, 0)) < 0) {
//					av_log(NULL, AV_LOG_ERROR,
//							"Couldn't seek to beginning of stream.\n");
//				}
		} else {
			get_error_text(ret);
		}
		return NULL;
	}

	return &pkt;
}

int CFFmpegIOProcessor::WriteOneAVPacket(AVPacket *pkt) {
	int ret = 0;
	SegmentContext *seg = (SegmentContext *) ofmt_ctx->priv_data;
	if ((ret = av_interleaved_write_frame(ofmt_ctx, pkt)) < 0) {
		get_error_text(ret);
		return ret;
	}

	return seg->segment_idx;
}

int CFFmpegIOProcessor::Run() {
	int ret = 0;
	AVPacket pkt;
	while (1) {
		if ((ret = av_read_frame(ifmt_ctx, &pkt)) < 0) {
			if (ret == AVERROR_EOF) {
				av_log(NULL, AV_LOG_WARNING, "End of file.\n");
//				if ((ret = av_seek_frame(ifmt_ctx, in_video_index, 0, 0)) < 0) {
//					av_log(NULL, AV_LOG_ERROR,
//							"Couldn't seek to beginning of stream.\n");
//				}
			} else {
				av_log(NULL, AV_LOG_ERROR,
						"%d: Error occurred when reading frame .\n", ret);
			}
			break;
		}
		if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Write AVPacket failed!\n");
			break;
		}
		av_packet_unref(&pkt);
	}

	av_packet_unref(&pkt);

	return ret;
}
