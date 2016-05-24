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

	if ((ret = InProcessor->InitOutputFormatContext(pParams)) < 0) {
		return ret;
	}
	m_pSafetyArea = pParams->m_pSafetyArea;
	m_SegmentWrap = pParams->nSegWrap;

	return 0;
}

int CReceiver::ReceivingLoop() {
	int ret = 0, nSeg = 0, nLastSeg = 0;
	int64_t dts;
	AVPacket *pkt;
	if (InProcessor.get()) {
		while (1) {
			pkt = InProcessor->ReadOneAVPacket();
			if (!pkt)
				break;
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
			if ((nSeg = InProcessor->WriteOneAVPacket(pkt)) < 0)
				break;
			else if (nLastSeg != nSeg) {
				m_pSafetyArea->SetSegment(av_rescale_q(dts, m_StreamTimeBase,
				SHIFT_TIME_BASE_Q), nSeg);
				nLastSeg = nSeg;
				if (nLastSeg == (m_SegmentWrap -1))
				fprintf(stderr,
						"Segment Num: %d, start_time: %ld (ms), shift_time: %ld (ms).\n",
						nLastSeg, m_pSafetyArea->m_StartTime,
						m_pSafetyArea->m_ShiftTime);
			}
			av_packet_unref(pkt);
		}
	}
	if (pkt)
		av_packet_unref(pkt);

	return 0;
}

