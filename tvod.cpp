/*
 * tvod.cpp
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#include "tvod.h"
#include "receiver.h"
#include "transmitter.h"

using namespace std;
using namespace tvod;

void PrintHelp(char *strAppName, const char *strErrorMessage) {

	if (strErrorMessage) {
		printf("Error: %s\n", strErrorMessage);
	}

	printf("Usage: %s [<options>] \n", strAppName);
	printf("\n");
	printf("Options:\n");
	printf(

	"   [-i]                         - input file/stream.\n");
	printf(

	"   [-o]                     	- output file/stream.\n");
	printf(

	"   [-ts]                         - tmp ts file name.\n");
}

int ParseInputString(char* strInput[], mfxU8 nArgNum, sInputParams* pParams) {
	if (1 == nArgNum) {
		PrintHelp(strInput[0], NULL);
		return MFX_ERR_UNSUPPORTED;
	}

	for (mfxU8 i = 1; i < nArgNum; i++) {
		if (0 == strcmp(strInput[i], "-i")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strSrcFile = strInput[++i];
		} else if (0 == strcmp(strInput[i], "-ts")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strTmpFile = strInput[++i];
		} else if (0 == strcmp(strInput[i], "-o")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strDstFile = strInput[++i];
		} else if (0 == strcmp(strInput[i], "-of")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strOutInterface = strInput[++i];
		} else if (0 == strcmp(strInput[i], "-m3u8")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strHLSM3U8 = strInput[++i];
		}  else if (0 == strcmp(strInput[i], "-prefix")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->strM3U8Prefix = strInput[++i];
		} else if (0 == strcmp(strInput[i], "-size")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->nSegTime = atoi(strInput[++i]);
		} else if (0 == strcmp(strInput[i], "-wrap")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->nSegWrap = atoi(strInput[++i]);
		} else if (0 == strcmp(strInput[i], "-delay")) {
			VAL_CHECK(i + 1 == nArgNum, i, strInput[i]);
			pParams->nShiftTime = atoi(strInput[++i]);
		} else {
			PrintHelp(strInput[0], "Unrecognized option.");
			exit(0);
		}
	}

	return 0;
}

CTimeShift::CTimeShift() :
		m_pSafetyArea(NULL) {
} // CTimeShift::CTimeShift()

CTimeShift::~CTimeShift() {
	Close();
} // CTimeShift::~CTimeShift()

int CTimeShift::Init(sInputParams *pParams) {

	return 0;
} // mfxStatus CTimeShift::Init()

int CTimeShift::ProcessResult() {

	return 0;
} // mfxStatus CTimeShift::ProcessResult()

mfxU32 MFX_STDCALL RunReceiver(void *params) {
	int ret = 0;
	sInputParams *pParams = (sInputParams*) params;

	printf("Receiver Thread: %d Started.\n", pParams->nThreadID);

	std::auto_ptr<CReceiver> pReceiverPipeline;

	pReceiverPipeline.reset(new CReceiver);

	pReceiverPipeline->InitReceiver(pParams);

	pReceiverPipeline->ReceivingLoop(pParams);

	printf("Receiver Thread: %d Finished.\n", pParams->nThreadID);

	return 0;
}

mfxU32 MFX_STDCALL RunTransmitter(void *params) {
	int ret = 0;
	sInputParams *pParams = (sInputParams*) params;

	printf("Transmitter Thread: %d Started.\n", pParams->nThreadID);

	std::auto_ptr<CTransmitter> pTransmitterPipeline;

	pTransmitterPipeline.reset(new CTransmitter);

	pTransmitterPipeline->InitTransmitter(pParams);

	pTransmitterPipeline->TransmittingLoop(pParams);

	printf("Transmitter Thread: %d Finished.\n", pParams->nThreadID);

	return 0;
}

int CTimeShift::Run(sInputParams *pParams) {
	mfxStatus sts;
	int n_Thread = 1;
	MSDKThread *pthread = NULL;

	m_pSafetyArea = new SafetyDataArea;

	/*Receiver Thread Created*/
	sInputParams pInThreadParams;
	pInThreadParams.strSrcFile = pParams->strSrcFile;
	pInThreadParams.strDstFile = pParams->strTmpFile;
	pInThreadParams.m_pSafetyArea = m_pSafetyArea;
	pInThreadParams.strHLSM3U8 = pParams->strHLSM3U8;
	pInThreadParams.strM3U8Prefix = pParams->strM3U8Prefix;
	pInThreadParams.nSegTime = pParams->nSegTime ? pParams->nSegTime : 10;
	pInThreadParams.nSegWrap = pParams->nSegWrap ? pParams->nSegWrap : 1800;
	pInThreadParams.nThreadID = 0;
	pthread = new MSDKThread(sts, RunReceiver, (void *) &pInThreadParams);
	m_HDLArray.push_back(pthread);

	/*Transmitter Thread Created*/
//	sInputParams pOutThreadParams;
//	pOutThreadParams.strDstFile = pParams->strDstFile;
//	pOutThreadParams.strSrcFile = pParams->strTmpFile;
//	pOutThreadParams.m_pSafetyArea = m_pSafetyArea;
//	if (pParams->nShiftTime)
//		pOutThreadParams.nShiftTime = pParams->nShiftTime * 60000;
//	else
//		pOutThreadParams.nShiftTime = SHIFT_TIME_IN_MILLISCOND;
//	pOutThreadParams.nSegWrap = pParams->nSegWrap ? pParams->nSegWrap : 1800;
//	pOutThreadParams.nThreadID = 1;
//	pthread = new MSDKThread(sts, RunTransmitter, (void *) &pOutThreadParams);
//	m_HDLArray.push_back(pthread);

	for (int i = 0; i < n_Thread; i++) {
		m_HDLArray[i]->Wait();
	}

	fprintf(stderr, "\nAll Thread Finished.\n");

	return sts;
}

void CTimeShift::Close() {
	while (m_HDLArray.size()) {
		delete m_HDLArray[m_HDLArray.size() - 1];
		m_HDLArray.pop_back();
	}
} // void CTimeShift::Close()

int main(int argc, char *argv[]) {

	mfxStatus sts = MFX_ERR_NONE;
	int ret = 0;
	sInputParams Params;   // input parameters from command line

	if (ParseInputString(argv, (mfxU8) argc, &Params) < 0)
		exit(0);

	av_register_all();
	avformat_network_init();
	av_log_set_level(AV_LOG_INFO);

	std::auto_ptr<CTimeShift> tvod;
	tvod.reset(new CTimeShift);

	tvod->Init(&Params);
	tvod->Run(&Params);

	return 0;
}

