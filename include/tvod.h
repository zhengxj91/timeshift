/*
 * tvod.h
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#ifndef INCLUDE_TVOD_H_
#define INCLUDE_TVOD_H_

#include<vector>
#include "mfxdefs.h"
#include "IOProcessor.h"

#define VAL_CHECK(val, argIdx, argName) \
{ \
    if (val) \
    { \
        fprintf(stderr, "Input argument number %d \"%s\" require more parameters.\n", argIdx, argName); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}

namespace tvod {

class CTimeShift {
public:
	SafetyDataArea *m_pSafetyArea;

	CTimeShift();
	virtual ~CTimeShift();

	virtual int Init(sInputParams *pParams);
	virtual int Run(sInputParams *pParams);
	virtual int ProcessResult();

protected:
	virtual void Close();

	// handles
	std::vector<MSDKThread*> m_HDLArray;

private:
	DISALLOW_COPY_AND_ASSIGN(CTimeShift);

};

}

#endif /* INCLUDE_TVOD_H_ */
