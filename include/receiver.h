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
	virtual int ReceivingLoop();

protected:

	bool m_bFirstVideoPacket;
	int m_SegmentWrap;

	AVRational m_StreamTimeBase;
	SafetyDataArea *m_pSafetyArea;
	std::auto_ptr<CFFmpegIOProcessor> InProcessor;

private:

};

#endif /* INCLUDE_RECEIVER_H_ */
