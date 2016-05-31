/*
 * IOProcessor.h
 *
 *  Created on: 2016年5月23日
 *      Author: leo
 */

#ifndef INCLUDE_IOPROCESSOR_H_
#define INCLUDE_IOPROCESSOR_H_
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include<list>
#include <unistd.h>
#include <memory>

#include "vm/thread_defs.h"

#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include "libavutil/dict.h"
}
;
#endif

#define DEFAULT_SERVICE_NAME "TVOD"
#define DEFAULT_PROVIDER_NAME "CIK"
#define MAX_FILE_LENGTH 1024
#define SHIFT_TIME_BASE_Q (AVRational) {1, 1000}
#define SEGMENT_WRAP 1800
#define SHIFT_TIME_IN_SECOND 3600
#define SHIFT_TIME_IN_MILLISCOND 3600000
#define NEED_TO_RESTART -10
#define NEED_TO_BREAK 0

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)

typedef struct SegmentListEntry {
	int index;
	double start_time, end_time;
	int64_t start_pts;
	int64_t offset_pts;
	char *filename;
	struct SegmentListEntry *next;
	int64_t last_duration;
} SegmentListEntry;

typedef struct SegmentContext {
	const AVClass *aClass;
	/**< Class for private options. */
	int segment_idx;    ///< index of the segment file to write, starting from 0
	int segment_idx_wrap;  ///< number after which the index wraps
	int segment_idx_wrap_nb;  ///< number of time the index has wraped
	int segment_count;     ///< number of segment files already written
	AVOutputFormat *oformat;
	AVFormatContext *avf;
	char *format;              ///< format to use for output segment files
	char *format_options_str; ///< format options to use for output segment files
	AVDictionary *format_options;
	char *list;            ///< filename for the segment list file
	int list_flags;      ///< flags affecting list generation
	int list_size;       ///< number of entries for the segment list file

	int use_clocktime;    ///< flag to cut segments at regular clock time
	int64_t clocktime_offset; //< clock offset for cutting the segments at regular clock time
	int64_t clocktime_wrap_duration; //< wrapping duration considered for starting a new segment
	int64_t last_val;      ///< remember last time for wrap around detection
	int64_t last_cut;      ///< remember last cut
	int cut_pending;

	char *entry_prefix;    ///< prefix to add to list entry filenames
	int list_type;         ///< set the list type
	AVIOContext *list_pb;  ///< list file put-byte context
	char *time_str;        ///< segment duration specification string
	int64_t time;          ///< segment duration
	int use_strftime;      ///< flag to expand filename with strftime

	char *times_str;       ///< segment times specification string
	int64_t *times;        ///< list of segment interval specification
	int nb_times;          ///< number of elments in the times array

	char *frames_str;      ///< segment frame numbers specification string
	int *frames;           ///< list of frame number specification
	int nb_frames;         ///< number of elments in the frames array
	int frame_count;       ///< total number of reference frames
	int segment_frame_count; ///< number of reference frames in the segment

	int64_t time_delta;
	int individual_header_trailer; /**< Set by a private option. */
	int write_header_trailer; /**< Set by a private option. */
	char *header_filename;  ///< filename to write the output header to

	int reset_timestamps;  ///< reset timestamps at the begin of each segment
	int64_t initial_offset; ///< initial timestamps offset, expressed in microseconds
	char *reference_stream_specifier; ///< reference stream specifier
	int reference_stream_index;
	int break_non_keyframes;

	int use_rename;
	char temp_list_filename[1024];

	SegmentListEntry cur_entry;
	SegmentListEntry *segment_list_entries;
	SegmentListEntry *segment_list_entries_end;
} SegmentContext;

struct HLSSegment {
	char tsName[MAX_FILE_LENGTH];
	unsigned int index;
	unsigned int duration;
};

struct SafeSegment {
	int dts;
	int num;
};

class SafetyDataArea {
public:
	int64_t m_StartTime;
	int64_t m_ShiftTime;
	int m_SegmentNum;
	SafeSegment seg;
	std::list<SafeSegment> m_segArray;

	SafetyDataArea();
	virtual ~SafetyDataArea();

	virtual void SetStartTime(const int64_t t);
	virtual void SetSegment(const int64_t dts, const int n);
	virtual void FreeSegment();
	virtual int GetSegmentNum();
	virtual int GetFinalSegmentNum();
	virtual int64_t GetSegmentStartTime();
	virtual void CalculateShiftTime(const int64_t t);
	virtual bool CheckShiftTime();
	virtual int64_t GetShiftTime();
protected:
	MSDKMutex m_mutex;
private:
	DISALLOW_COPY_AND_ASSIGN(SafetyDataArea);
};

struct sInputParams {

	int nSrcFiles;
	int nDstFiles;
	int nSegTime;
	int nSegWrap;
	int64_t nShiftTime;

	char **pSrcFile;
	char **pDstFile;

	char *format;

	char *strTmpFile;
	char *strSrcFile;
	char *strDstFile;
	char *strHLSM3U8;
	char *strM3U8Prefix;
	char *strOutInterface;

	SafetyDataArea *m_pSafetyArea;

	int nThreadID;

	sInputParams() {
		memset(this, 0, sizeof(*this));
	}
};

class CFFmpegIOProcessor {
public:

	int in_video_index;
	int in_audio_index;
	double dts2time;
	double start_time;
	AVPacket pkt;
	AVFormatContext *ifmt_ctx;
	AVFormatContext *ofmt_ctx;

	CFFmpegIOProcessor();
	virtual ~CFFmpegIOProcessor();
	virtual void Close();

	virtual int InitInputFormatContext(const char *srcName);
	virtual int InitOutputFormatContext(sInputParams *pParams, const int segment_start_num);
	virtual int ResetInputFormatContext(const char *srcName);
	virtual int ResetOutputFormatContext(sInputParams *pParams, const int segment_start_num);

	virtual AVRational GetStreamTimeBase();
	virtual AVPacket* ReadOneAVPacket();
	virtual int WriteOneAVPacket(AVPacket *pkt);
	virtual int Run();

protected:

	int out_video_index;
	int out_audio_index;
	bool m_bInited;
	bool m_bWaitFirstVideoPacket;
	int64_t ts_offset;
	int64_t last_audio_dts;
	int64_t last_video_dts;

	AVBitStreamFilterContext* h264_in_bsf;
	AVBitStreamFilterContext* aac_bsf;

private:

};

#endif /* INCLUDE_IOPROCESSOR_H_ */
