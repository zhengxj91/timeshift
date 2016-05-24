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
		m_bFirstVideoPacket(true), m_pSafetyArea(NULL), m_SegmentWrap(0) {

}

CReceiver::~CReceiver() {
	Close();
}

void CReceiver::Close() {
	if (InProcessor.get()) {

	}
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
	m_pSafetyArea = pParams->m_pSafetyArea;
	m_SegmentWrap = pParams->nSegWrap;

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

int CReceiver::ReceivingLoop(sInputParams *pParams) {

	int ret = 0, nSeg = 0, nLastSeg = 0;
	int inErrCount = 0, outErrCount = 0;
	int64_t dts;
	AVPacket *pkt;
	if (InProcessor.get()) {
		while (1) {
			pkt = InProcessor->ReadOneAVPacket();
			if (!pkt) {
				if(ResetInput(pParams->strSrcFile) < 0) {
					if(++inErrCount > 10)
						break;
				}
				else
					inErrCount = 0;
				continue;
			}
			dts = pkt->dts;
			if (pkt->stream_index == InProcessor->in_video_index) {
				if (m_bFirstVideoPacket) {
					m_pSafetyArea->SetStartTime(
							av_rescale_q(dts, m_StreamTimeBase,
							SHIFT_TIME_BASE_Q));
					m_pSafetyArea->SetSegment(av_rescale_q(dts, m_StreamTimeBase,
					SHIFT_TIME_BASE_Q), nSeg);
					m_bFirstVideoPacket = false;
				}
				m_pSafetyArea->CalculateShiftTime(
						av_rescale_q(dts, m_StreamTimeBase,
						SHIFT_TIME_BASE_Q));
			}
			if ((nSeg = InProcessor->WriteOneAVPacket(pkt)) < 0) {
				int start_num = m_pSafetyArea->GetFinalSegmentNum() + 1;
				if(ResetOutput(pParams, start_num) < 0) {
					if(++outErrCount > 10)
						break;
				}
				else
					outErrCount = 0;
				continue;
			}
			else if (nLastSeg != nSeg) {
				m_pSafetyArea->SetSegment(av_rescale_q(dts, m_StreamTimeBase,
				SHIFT_TIME_BASE_Q), nSeg);
				nLastSeg = nSeg;
				if (nLastSeg == (m_SegmentWrap -1))
				fprintf(stderr, "Segment Num: %d, shift_time: %ld (ms).\n", nLastSeg, m_pSafetyArea->m_ShiftTime / 60000);
			}
			av_packet_unref(pkt);
		}
	}
	if (pkt)
		av_packet_unref(pkt);

	return ret;
}

