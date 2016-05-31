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
	virtual int WriteM3u8List(sInputParams *pParams);
	virtual int ReceivingLoop(sInputParams *pParams);

protected:

	bool m_bWaitFirstVPkt;
	bool m_bKeyFrame;
	int m_SegmentWrap;
	int64_t m_SegmentNum;

	double m_PacketTime;
	double m_SegmentStartTime;
	double m_SegmentTime;
	int m_SegmentIndex;

	unsigned int segment_duration_array[2048];
	char tmp_m3u8_file[MAX_FILE_LENGTH];
	char ts_name[MAX_FILE_LENGTH];

	std::list<HLSSegment> m_SegmentList;

	AVRational m_StreamTimeBase;
	SafetyDataArea *m_pSafetyArea;
	std::auto_ptr<CFFmpegIOProcessor> InProcessor;

private:

};

#endif /* INCLUDE_RECEIVER_H_ */
