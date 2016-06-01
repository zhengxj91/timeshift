/*
 * receiver.h
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#ifndef INCLUDE_RECEIVER_H_
#define INCLUDE_RECEIVER_H_

#include "IOProcessor.h"

class CReceiver {
public:

	CReceiver();
	virtual ~CReceiver();
	virtual void Close();
	virtual int InitReceiver(sInputParams *pParams);
	virtual int ResetInput(char *srcName);
	virtual int ResetOutput(sInputParams *pParams, const int segment_start_num);
	virtual int WriteM3u8List(char *list_name);
	virtual int ReceivingLoop(sInputParams *pParams);

protected:

	bool m_bWaitFirstVPkt;
	bool m_bKeyFrame;
	int m_SegmentArraySize;
	int m_M3u8ListSize;
	int m_SegmentWrap;
	int m_SegmentIndex;
	int64_t m_SegmentNum;
	int64_t m_MeidaSequence;

	double m_PacketTime;
	double m_SegmentStartTime;
	double m_SegmentTime;

	char *m_SegmentPrefix;
	char m_TmpM3u8ListName[MAX_FILE_LENGTH];
	char m_SegmentName[MAX_FILE_LENGTH];

	std::vector<HLSSegment> m_SegmentArray;

	AVRational m_StreamTimeBase;
	SafetyDataArea *m_pSafetyArea;
	std::auto_ptr<CFFmpegIOProcessor> InProcessor;

private:

};

#endif /* INCLUDE_RECEIVER_H_ */
