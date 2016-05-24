/*
 * transmitter.cpp
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#include "transmitter.h"
#include "IOProcessor.h"
using namespace std;

int parseURL(char* url, int url_len) {
	const char* ptr = NULL;
	int len, i;
	url_len = strlen(url);
	ptr = strchr(url, ':');
	if (!ptr) {
		fprintf(stderr, "url: %s unvalid. \n", url);
		return -1;
	}
	len = ptr - url;

	return len;
}

CTransmitter::CTransmitter() :
		m_pSafetyArea(NULL), m_pUdpIp(NULL), m_pUdpPort(NULL), UdpSocket(0), UdpAddr(NULL), m_SegmentWrap(0) {

}

CTransmitter::~CTransmitter() {

}

int CTransmitter::InitTransmitter(sInputParams *pParams) {
	int ret = 0, len = 0, pos = 0;

	len = strlen(pParams->strDstFile);
	if( (pos = parseURL(pParams->strDstFile, len)) < 0)
		return pos;

	m_pUdpIp = (char*) malloc(pos);
	memcpy(m_pUdpIp, pParams->strDstFile, pos);

	m_pUdpPort = (char*) malloc(len - pos);
	memcpy(m_pUdpPort, pParams->strDstFile + pos + 1, len - pos - 1);

	if ((UdpSocket = InitTransmitterSocket(m_pUdpIp, m_pUdpPort,
			pParams->strOutInterface, &UdpAddr)) == -1) {
		av_log(NULL, AV_LOG_ERROR, "Create Output Socket failed.");
		return -1;
	}

	m_pSafetyArea = pParams->m_pSafetyArea;
	m_SegmentWrap = pParams->nSegWrap;

	return 0;
}

int CTransmitter::TransmittingLoop(sInputParams *pParams) {
	int ret = 0, nSegmentIdx = 0;
	int64_t shiftTime = 0;
	char filename[1024];
	uint8_t buf[BUFFER_SIZE];
	AVIOContext *input = NULL;

	while (1) {
		if ((shiftTime = m_pSafetyArea->GetShiftTime()) < pParams->nShiftTime) {
//			fprintf(stderr, "Now : %ld, Shift: %ld, Sleep: %ld .\n", shiftTime, pParams->nShiftTime, pParams->nShiftTime - shiftTime);
			usleep((pParams->nShiftTime - shiftTime) * 1000);
			continue;
		}
		if( (nSegmentIdx = m_pSafetyArea->GetSegmentNum()) < 0)
			continue;
		fprintf(stderr, "Num: %d, Start : %ld, Shift : %ld.  ", nSegmentIdx, m_pSafetyArea->GetSegmentStartTime(), shiftTime);
		if ((ret = av_get_frame_filename(filename, sizeof(filename),
				pParams->strSrcFile, nSegmentIdx)) < 0) {
			av_log(NULL, AV_LOG_ERROR,
					"Invalid segment filename template '%s'\n",
					pParams->strSrcFile);
			break;
		}
		fprintf(stderr, "Open segment : %s.\n", filename);
		if ((ret = avio_open2(&input, filename, AVIO_FLAG_READ, NULL, NULL))
				< 0) {
			av_log(input, AV_LOG_ERROR, "Failed to open input: %s.\n", filename);
			break;
		}
		for (;;) {
			if ((ret = avio_read(input, buf, sizeof(buf))) < 0) {
				if (ret == AVERROR_EOF)
					break;
				else
					av_log(input, AV_LOG_ERROR, "Error reading from input: %s.\n", filename);
				break;
			}
			if (sendto(UdpSocket, buf, BUFFER_SIZE, 0, UdpAddr->ai_addr,
					UdpAddr->ai_addrlen) != BUFFER_SIZE) {
				av_log(NULL, AV_LOG_ERROR,
						"Socket transmitting unexpected bytes of datas ");
			}
		}

		m_pSafetyArea->FreeSegment();

	}
	return 0;
}
