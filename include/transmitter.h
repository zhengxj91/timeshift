/*
 * transmitter.h
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#ifndef INCLUDE_TRANSMITTER_H_
#define INCLUDE_TRANSMITTER_H_

#include "msock.h"
#include "IOProcessor.h"

#define BUFFER_SIZE 1316

class CTransmitter {
public:

	CTransmitter();
	virtual ~CTransmitter();
	virtual int InitTransmitter(sInputParams *pParams);
	virtual int TransmittingLoop(sInputParams *pParams);

protected:

	int m_SegmentWrap;
	char* m_pUdpIp;
	char* m_pUdpPort;

	SOCKET UdpSocket;
	struct addrinfo *UdpAddr; //unicast or multicast
	AVRational m_StreamTimeBase;
	SafetyDataArea* m_pSafetyArea;

private:

};

#endif /* INCLUDE_TRANSMITTER_H_ */
