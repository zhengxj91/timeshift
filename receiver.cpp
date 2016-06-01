/*
 * receiver.cpp
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#include "receiver.h"
#include "IOProcessor.h"
using namespace std;

CReceiver::CReceiver() :
		m_bWaitFirstVPkt(true), m_pSafetyArea(NULL), m_M3u8ListSize(0), m_SegmentIndex(0), m_SegmentArraySize(
				0), m_SegmentWrap(0), m_PacketTime(0), m_SegmentNum(0), m_SegmentStartTime(0), m_SegmentTime(
				0), m_MeidaSequence(0), m_bKeyFrame(false), m_SegmentPrefix(NULL) {

}

CReceiver::~CReceiver() {
	Close();
}

void CReceiver::Close() {
	m_SegmentArray.clear();
}

int CReceiver::InitReceiver(sInputParams *pParams) {
	int ret = 0;
	InProcessor.reset(new CFFmpegIOProcessor);
	if ((ret = InProcessor->InitInputFormatContext(pParams->strSrcFile)) < 0) {
		return ret;
	}

	m_StreamTimeBase = InProcessor->GetStreamTimeBase();
	if (!m_StreamTimeBase.den && !m_StreamTimeBase.num)
		m_StreamTimeBase = (AVRational ) { 1, 90000 };

	if ((ret = InProcessor->InitOutputFormatContext(pParams, 0)) < 0) {
		return ret;
	}

	if (av_get_frame_filename(m_SegmentName, sizeof(m_SegmentName), InProcessor->ofmt_ctx->filename, 0) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Invalid segment filename template '%s'\n", InProcessor->ofmt_ctx->filename);
		return -1;
	}
	ret = avio_open(&InProcessor->ofmt_ctx->pb, m_SegmentName, AVIO_FLAG_WRITE);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", m_SegmentName);
		return ret;
	}

//	m_pSafetyArea = pParams->m_pSafetyArea;
	m_M3u8ListSize = pParams->nSegSize;
	m_SegmentArraySize = (int) (pParams->nShiftTime * 60 / pParams->nSegTime);
	m_SegmentWrap = m_SegmentArraySize + 4;
	if (m_SegmentArraySize < pParams->nSegWrap) {
		m_SegmentWrap = pParams->nSegWrap;
	}
	m_SegmentTime = (double) pParams->nSegTime;
	m_SegmentPrefix = pParams->strM3U8Prefix;
	sprintf(m_TmpM3u8ListName, "%s.tmp", pParams->strHLSM3U8);

	return 0;
}

int CReceiver::ResetInput(char *srcName) {

	if (InProcessor->ResetInputFormatContext(srcName) < 0) {
		return -1;
	}

	m_StreamTimeBase = InProcessor->GetStreamTimeBase();
	if (!m_StreamTimeBase.den && !m_StreamTimeBase.num)
		m_StreamTimeBase = (AVRational ) { 1, 90000 };

	return 0;
}

int CReceiver::ResetOutput(sInputParams *pParams, const int segment_start_num) {

	if (InProcessor->ResetOutputFormatContext(pParams, segment_start_num) < 0) {
		return -1;
	}

	return 0;
}

int CReceiver::WriteM3u8List(char *list_name) {
	FILE *list_fp;
	list_fp = fopen(m_TmpM3u8ListName, "w");
	if (!list_fp) {
		fprintf(stderr, "Could not open temporary m3u8 index file (%s), no index file will be created\n",
				m_TmpM3u8ListName);
		return -1;
	}
	double maxDuration = m_SegmentArray.front().duration;
	for (int i = 0; i < m_M3u8ListSize; i++) {
		if (m_SegmentArray[i].duration > maxDuration) {
			maxDuration = m_SegmentArray[i].duration;
		}
	}
	fprintf(list_fp,
			"#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-ALLOW-CACHE:NO\n#EXT-X-TARGETDURATION:%ld\n#EXT-X-MEDIA-SEQUENCE:%ld\n",
			(int64_t) ceil(maxDuration), m_MeidaSequence);
	for (int i = 0; i < m_M3u8ListSize; i++) {
		if (fprintf(list_fp, "#EXTINF:%f,\n%s\n", m_SegmentArray[i].duration, m_SegmentArray[i].tsName) < 0) {
			fprintf(stderr, "Failed to write to tmp m3u8 index file\n");
			return -1;
		}
	}
	fclose(list_fp);

	return rename(m_TmpM3u8ListName, list_name);
}

int CReceiver::ReceivingLoop(sInputParams *pParams) {

	int ret = 0, nSeg = 0, nLastSeg = 0;
	int inErrCount = 0, outErrCount = 0;
	int64_t dts;
	HLSSegment seg;
	AVPacket *pkt;
	int video_index = InProcessor->in_video_index;
	double dts2time = InProcessor->dts2time;
	double start_time = InProcessor->start_time;
	int size = strlen(av_basename(m_SegmentName)) + 1;
	if (m_SegmentPrefix)
		size += strlen(m_SegmentPrefix);

	if (InProcessor.get()) {
		while (1) {
			pkt = InProcessor->ReadOneAVPacket();
			if (!pkt) {
				break;
			}
			if (pkt->stream_index == video_index) {
				m_PacketTime = (double) pkt->dts * dts2time;
				if (m_bWaitFirstVPkt) {
					m_SegmentStartTime = m_PacketTime;
					m_bWaitFirstVPkt = false;
				}
				if (pkt->flags & AV_PKT_FLAG_KEY)
					m_bKeyFrame = true;
				else
					m_bKeyFrame = false;
				if (m_bKeyFrame && ((m_PacketTime - m_SegmentStartTime) >= (m_SegmentTime - 0.25))) {

					avio_flush(InProcessor->ofmt_ctx->pb);
					avio_close(InProcessor->ofmt_ctx->pb);

					m_bWaitFirstVPkt = true;
					m_SegmentNum++;
					if (m_SegmentNum % 180 == 0) {
						av_log(NULL, AV_LOG_INFO, "Segments: %ld\n", m_SegmentNum);
					}

					seg.duration = m_PacketTime - m_SegmentStartTime;
					seg.index = m_SegmentIndex;
					snprintf(seg.tsName, size, "%s%s", m_SegmentPrefix ? m_SegmentPrefix : "",
							av_basename(m_SegmentName));

					m_SegmentArray.push_back(seg);

					if (m_SegmentNum >= m_M3u8ListSize) {
						if (WriteM3u8List(pParams->strHLSM3U8) < 0)
							break;
						if (m_SegmentNum >= m_SegmentArraySize) {
							m_SegmentArray.erase(m_SegmentArray.begin());
							m_MeidaSequence++; //num of segments that deleted.
						}
					}

					m_SegmentIndex = (++m_SegmentIndex) % m_SegmentWrap;

					if (av_get_frame_filename(m_SegmentName, sizeof(m_SegmentName),
							InProcessor->ofmt_ctx->filename, m_SegmentIndex) < 0) {
						av_log(NULL, AV_LOG_ERROR, "Invalid segment filename template '%s'\n",
								InProcessor->ofmt_ctx->filename);
						break;
					}

					if (avio_open(&InProcessor->ofmt_ctx->pb, m_SegmentName, AVIO_FLAG_WRITE) < 0) {
						fprintf(stderr, "Could not open '%s'\n", m_SegmentName);
						break;
					}
				}
			}

			if ((InProcessor->WriteOneAVPacket(pkt)) < 0) {
				outErrCount++;
				if (++outErrCount > 20) {
					if (ResetInput(pParams->strSrcFile) < 0) {
						if (++inErrCount > 10)
							break;
					} else
						inErrCount = 0;
				}
				av_packet_unref(pkt);
				continue;
			}
			outErrCount = 0;
			av_packet_unref(pkt);
		}
	}
	if (pkt)
		av_packet_unref(pkt);

	return 0;
}

