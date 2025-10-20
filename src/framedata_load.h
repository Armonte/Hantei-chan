#ifndef FRAMEDATA_LOAD_H_GUARD
#define FRAMEDATA_LOAD_H_GUARD

#include "framedata.h"

struct TempInfo {
	struct DelayLoad
	{
		unsigned int frameNo;
		unsigned int location;
		unsigned int source;
	};
	Sequence	*seq;
	std::vector<Hitbox*> boxesRefs;
	std::vector<DelayLoad> delayLoadList;
	unsigned int cur_hitbox;
	unsigned int cur_AS;
	unsigned int cur_frame;

	std::vector<Frame_AS*> AS;
};

struct TestInfo{
	const char *filename;
	std::string seqName;
	int seqId;
	int frame;

	void Print(const void *data, const void *data_end);

};

extern TestInfo test;

unsigned int *fd_frame_AT_load(unsigned int *data, const unsigned int *data_end, Frame_AT *AT, TempInfo *info);
unsigned int *fd_frame_AS_load(unsigned int *data, const unsigned int *data_end, Frame_AS *AS);
unsigned int *fd_frame_EF_load(unsigned int *data, const unsigned int *data_end, Frame_EF *EF);
unsigned int *fd_frame_IF_load(unsigned int *data, const unsigned int *data_end, Frame_IF *IF);
unsigned int *fd_frame_AF_load(unsigned int *data, const unsigned int *data_end, Frame *frame);
unsigned int *fd_frame_load(unsigned int *data, const unsigned int *data_end, Frame *frame, TempInfo *info);
unsigned int *fd_sequence_load(unsigned int *data, const unsigned int *data_end, Sequence *seq, bool utf8);
unsigned int *fd_main_load(unsigned int *data, const unsigned int *data_end, std::vector<Sequence> &sequences, unsigned int nsequences, bool utf8);



#endif /* FRAMEDATA_LOAD_H_GUARD */
