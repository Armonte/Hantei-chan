// .HA6 loader:
//
// Hantei6 frame data load and storage.

#include "framedata.h"
#include "misc.h"
#include <cstring>
#include <cassert>
#include <sstream>
#include <iomanip>

Sequence::Sequence():
psts(0), level(0), flag(0),
empty(true),
initialized(false)
{}

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
	unsigned int cur_AT;
	unsigned int cur_AS;
	unsigned int cur_EF;
	unsigned int cur_IF;
	unsigned int cur_frame;
};

// local recursive frame data loader

static unsigned int *fd_frame_AT_load(unsigned int *data, const unsigned int *data_end,
				Frame_AT *AT) {
	AT->active = 1;
	AT->correction = 100;
	
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "ATGD", 4)) {
			//If absent it's UB
			AT->guard_flags = data[0];
			++data;
		} else if (!memcmp(buf, "ATHS", 4)) {
			AT->correction = data[0];
			++data;
		} else if (!memcmp(buf, "ATVV", 4)) {
			short *d = (short *)data;
			AT->red_damage = d[0];
			AT->damage = d[1];
			AT->guard_damage = d[2];
			AT->meter_gain = d[3];
			data += 2;
		} else if (!memcmp(buf, "ATHT", 4)) {
			AT->correction_type = data[0];
			assert(data[0] >= 1 && data[0] <= 2);
			data += 1;
		} else if (!memcmp(buf, "ATGV", 4)) {
			//First number can be different from 3. See CMHisui's 421C
			//Second byte is a flag. Extract it separately.
			assert(data[0] <= 3);
			for(int i = 0; i < data[0]; i++)
			{
				AT->guardVector[i] = data[i+1] & 0xFF;
				AT->gVFlags[i] = data[i+1] >> 8;
				assert(AT->gVFlags[i] <= 3);
			}
			data += data[0]+1;
		} else if (!memcmp(buf, "ATHV", 4)) {
			//Same
			assert(data[0] <= 3);
			for(int i = 0; i < data[0]; i++)
			{
				AT->hitVector[i] = data[i+1] & 0xFF;
				AT->hVFlags[i] = data[i+1] >> 8;
				assert(AT->hVFlags[i] <= 3);
			}
			data += data[0]+1;
		} else if (!memcmp(buf, "ATF1", 4)) {
			AT->otherFlags = data[0];
			++data;
		} else if (!memcmp(buf, "ATHE", 4)) {
			AT->hitEffect = data[0];
			AT->soundEffect = data[1];
			data += 2;
		} else if (!memcmp(buf, "ATKK", 4)) {
			AT->addedEffect = data[0];
			assert(data[0] < 5 && data[0] >= 0);
			data++;
		} else if (!memcmp(buf, "ATNG", 4)) {
			AT->hitgrab = data[0];
			assert(data[0] == 1);
			data++;
		} else if (!memcmp(buf, "ATUH", 4)) {
			AT->extraGravity = ((float*)data)[0];
			data++;
		} else if (!memcmp(buf, "ATBT", 4)) {
			AT->breakTime = data[0];
			data++;
		} else if (!memcmp(buf, "ATSN", 4)) {
			AT->hitStopTime = data[0];
			data++;
		} else if (!memcmp(buf, "ATSU", 4)) {
			AT->untechTime = data[0];
			data++;
		} else if (!memcmp(buf, "ATSP", 4)) {
			AT->hitStop = data[0];
			data++;
		} else if (!memcmp(buf, "ATED", 4)) {
			break;
		}
		

		
		// unhandled:
		// ATGN(1) Does it even exist? Can't find it.
		// More
	}
	
	return data;
}

static unsigned int *fd_frame_AS_load(unsigned int *data, const unsigned int *data_end,
				Frame_AS *AS) {
	AS->speed_flags = 0;
	AS->ASMV = -1;
	
	AS->stand_state = 0;
	
	AS->cancel_flags = 0;
		
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "ASV0", 4)) {
			int *d = (int *)data;
			AS->speed_flags = d[0];
			AS->speed_horz = d[1];
			AS->speed_vert = d[2];
			AS->accel_horz = d[3];
			AS->accel_vert = d[4];
			data += 5;
		} else if (!memcmp(buf, "ASVX", 4)) {
			AS->speed_flags = 0x33; // ?
			AS->speed_horz = 0;
			AS->speed_vert = 0;
			AS->accel_horz = 0;
			AS->accel_vert = 0;
		} else if (!memcmp(buf, "ASMV", 4)) {
			AS->ASMV = data[0];
			++data;
		} else if (!memcmp(buf, "ASS1", 4)) {
			AS->stand_state = 1; // airborne
		} else if (!memcmp(buf, "ASS2", 4)) {
			AS->stand_state = 2; // crouching
		} else if (!memcmp(buf, "ASCN", 4)) {
			AS->cancel_flags |= 1;
		} else if (!memcmp(buf, "ASCS", 4)) {
			AS->cancel_flags |= 2;
		} else if (!memcmp(buf, "ASCT", 4)) {
			AS->cancel_flags |= 4;
			/*
		} else if (!memcmp(buf, "AST0", 4)) {
			float *d = (float *)(data+5);
			printf("AST0: %d %d %d %d %d %1.10f %d\n",
				data[0], data[1], data[2], data[3],
				data[4], *d, data[6]);
			
			data += 7;
			 */
		} else if (!memcmp(buf, "ASED", 4)) {
			break;
		}
		
		// unhandled:
		// ASAA(1) - ???
		// ASMX(1) - ?
		// ASF0(1) - ?
		// ASF1(1) - ?
		// ASYS(1) - ?
		// AST0(7) - ??? Kohaku/Necos/Nero/Roa/Ryougi/Warakia only
		// (no others)
	}
	
	return data;
}

static unsigned int *fd_frame_EF_load(unsigned int *data, const unsigned int *data_end,
				Frame_EF *EF) {
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "EFTP", 4)) {
			EF->command = data[0];
			++data;
		} else if (!memcmp(buf, "EFNO", 4)) {
			EF->parameter = data[0];
			++data;
		} else if (!memcmp(buf, "EFPR", 4)) {
			int count = data[0];
			if (count <= 12) {
				for (int i = 0; i < count; ++i) {
					EF->values[i] = data[i+1];
				}
			} else {
				//printf("EFPR %d ???\n",count);
			}
			data += count + 1;
		} else if (!memcmp(buf, "EFED", 4)) {
			break;
		}
	}
	
	return data;
}

static unsigned int *fd_frame_IF_load(unsigned int *data, const unsigned int *data_end,
				Frame_IF *IF) {
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "IFTP", 4)) {
			IF->command = data[0];
			++data;
		} else if (!memcmp(buf, "IFPR", 4)) {
			int count = data[0];
			if (count <= 12) {
				for (int i = 0; i < count; ++i) {
					IF->values[i] = data[i+1];
				}
			} else {
				//printf("IFPR %d ???\n",count);
			}
			data += count + 1;
		} else if (!memcmp(buf, "IFED", 4)) {
			break;
		}
	}
	
	return data;
}

static unsigned int *fd_frame_AF_load(unsigned int *data, const unsigned int *data_end,
				Frame *frame) {
	frame->AF.active = 1;
	frame->AF.spriteId = -1;
	frame->AF.duration = 1;

	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "AFGP", 4)) {
			int *dt = (int *)data;
			frame->AF.spriteId = dt[1];
			frame->AF.usePat = dt[0];
			assert(dt[0] == 0 || dt[0] == 1);
			data += 2;
		} else if (!memcmp(buf, "AFOF", 4)) {
			int *dt = (int *)data;
			frame->AF.offset_y = dt[1];
			frame->AF.offset_x = dt[0];
			data += 2;
		} else if (!memcmp(buf, "AFD", 3)) {
			char t = ((char *)buf)[3];
			if (t >= '0' && t <= '9') {
				frame->AF.duration = t - '0';
			} else if (t == 'L') {
				frame->AF.duration = data[0];
				++data;
			}
		} else if (!memcmp(buf, "AFY", 3)) {
			// 7/8/9/X/1/2/3 -> 7/8/9/10/11/12/13
			// other numbers are not used.
			// Overrides AFOF
			frame->AF.offset_x = 0;
			char t = ((char *)buf)[3];
			if (t >= '0' && t <= '9') {
				int v = (t - '0');
				if (v < 4) {
					v += 10;
				}
				frame->AF.offset_y = v;
			} else if (t == 'X') {
				frame->AF.offset_y = 10;
			}
		} else if (!memcmp(buf, "AFF", 3)) {
			char t = ((char *)buf)[3];
			if (t >= '0' && t <= '9') {
				frame->AF.aniFlag = t - '0';
			} else if (t == 'E') {
				frame->AF.aniFlag = data[0];
				++data;
			}
			else
				assert(0 && "Unknown AFF suffix.");
		} else if (!memcmp(buf, "AFAL", 4)) {
			frame->AF.blend_mode = data[0];
			frame->AF.rgba[3] = ((float)data[1])/255.f;
			assert(data[0] >= 1 || data[0] <= 3 );
			data += 2;
		} else if (!memcmp(buf, "AFRG", 4)) {
			frame->AF.rgba[0] = ((float)data[0])/255.f;
			frame->AF.rgba[1] = ((float)data[1])/255.f;
			frame->AF.rgba[2] = ((float)data[2])/255.f;
			data += 3;
		} else if (!memcmp(buf, "AFAZ", 4)) {
			frame->AF.rotation[2] = *(float *)data;
			
			++data;
		} else if (!memcmp(buf, "AFAY", 4)) {
			frame->AF.rotation[1] = *(float *)data;
			
			++data;
		} else if (!memcmp(buf, "AFAX", 4)) {
			frame->AF.rotation[0] = *(float *)data;
			
			++data;
		} else if (!memcmp(buf, "AFZM", 4)) {
			frame->AF.scale[0] = ((float *)data)[0];
			frame->AF.scale[1] = ((float *)data)[1];
			
			data += 2;
		} else if (!memcmp(buf, "AFJP", 4)) {
			frame->AF.jump = data[0];
			assert(data[0] != 0);
			++data;
		} else if (!memcmp(buf, "AFHK", 4)) {
			//Effects uses values other than 1.
			//TODO: Look into it.
			frame->AF.interpolation = data[0];
			++data;
		} else if (!memcmp(buf, "AFPR", 4)) {
			frame->AF.priority = data[0];
			++data;
		} else if (!memcmp(buf, "AFCT", 4)) {
			frame->AF.loopCount = data[0];
			++data;
		} else if (!memcmp(buf, "AFLP", 4)) {
			frame->AF.loopEnd = data[0];
			++data;
		} else if (!memcmp(buf, "AFJC", 4)) {
			frame->AF.landJump = data[0];
			++data;
		} else if (!memcmp(buf, "AFTN", 4)) {
			//Overrides rotation
			frame->AF.rotation[0] = data[0] ? 0.5f : 0.f;
			frame->AF.rotation[1] = data[1] ? 0.5f : 0.f;
			data += 2;
		} else if (!memcmp(buf, "AFRT", 4)) {
			frame->AF.AFRT = data[0];
			++data;
		} else if (!memcmp(buf, "AFED", 4)) {
			break;
		}
		else
		{
			assert(0 && "Unknown AF tag");
		}
	}
	
	return data;
}

static unsigned int *fd_frame_load(unsigned int *data, const unsigned int *data_end,
				Frame *frame, TempInfo *info) {
	int boxesCount = 0;
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "HRNM", 4) || !memcmp(buf, "HRAT", 4)) {
			// read hitbox or attackbox
			unsigned int location = data[0];
			if (!memcmp(buf, "HRAT", 4)) {
				location += 25;
			}
			if (location <= 32 && info->cur_hitbox < info->boxesRefs.size()) {
				Hitbox *hitbox = info->boxesRefs[info->cur_hitbox] = &frame->hitboxes[location];
				++info->cur_hitbox;
				boxesCount++;

				hitbox->x1 = (data[1]);
				hitbox->y1 = (data[2]);
				hitbox->x2 = (data[3]);
				hitbox->y2 = (data[4]);
			}
			else
				assert(0);
			
			data += 5;
		} else if (!memcmp(buf, "HRNS", 4) || !memcmp(buf, "HRAS", 4)) {
			// read hitbox reference
			unsigned int location = data[0];
			unsigned int source = data[1];
			boxesCount++;
			
			if (!memcmp(buf, "HRAS", 4)) {
				location += 25;
			}
			
			if (location <= 32) {
				info->delayLoadList.push_back({info->cur_frame, location, source});
			}
			else
				assert(0);
			
			data += 2;
		} else if (!memcmp(buf, "ATST", 4)) {
			// start attack block
			if (info->cur_AT < info->seq->AT.size()) {
				frame->AT = &info->seq->AT[info->cur_AT];
				++info->cur_AT;

				data = fd_frame_AT_load(data, data_end, frame->AT);
			}
		} else if (!memcmp(buf, "ASST", 4)) {
			// start state block
			if (info->cur_AS < info->seq->AS.size()) {
				frame->AS = &info->seq->AS[info->cur_AS];
				++info->cur_AS;
				
				data = fd_frame_AS_load(data, data_end, frame->AS);
			}
		} else if (!memcmp(buf, "ASSM", 4)) {
			// reference previous state block
			unsigned int value = data[0];
			++data;
			
			if (value < info->cur_AS) {
				frame->AS = &info->seq->AS[value];
			}
		} else if (!memcmp(buf, "AFST", 4)) {
			// start animation flags block
			data = fd_frame_AF_load(data, data_end, frame);
		} else if (!memcmp(buf, "EFST", 4)) {
			// start effect flags block
			int n = data[0];
			++data;
			if (n < 8 && info->cur_EF < info->seq->EF.size()) {
				frame->EF[n] = &info->seq->EF[info->cur_EF];
				++info->cur_EF;
				
				data = fd_frame_EF_load(data, data_end, frame->EF[n]);
			}
		} else if (!memcmp(buf, "IFST", 4)) {
			// start condition block
			int n = data[0];
			++data;
			if (n < 8 && info->cur_IF < info->seq->IF.size()) {
				frame->IF[n] = &info->seq->IF[info->cur_IF];
				++info->cur_IF;
				
				data = fd_frame_IF_load(data, data_end, frame->IF[n]);
			}
		} else if (!memcmp(buf, "FSNA", 4)) {
			//Max index of used attack boxes + 1
			++data;
		} else if (!memcmp(buf, "FSNH", 4)) {
			//Max index of used hantei boxes + 1
			++data;
		} else if (!memcmp(buf, "FSNE", 4)) {
			frame->FSNE = data[0];
			++data;
		} else if (!memcmp(buf, "FSNI", 4)) {
			frame->FSNI = data[0];
			++data;
		} else if (!memcmp(buf, "FEND", 4)) {
			//assert(frame->FSNA + frame->FSNH == frame->nHitbox);
			break;
		}
		
		// unhandled:
		// (no others)
		frame->nHitbox=boxesCount;
	}
	
	return data;
}

static unsigned int *fd_sequence_load(unsigned int *data, const unsigned int *data_end,
				Sequence *seq) {

	TempInfo temp_info;
	unsigned int frame_it = 0, nframes = 0;
	
	temp_info.seq = seq;
	temp_info.cur_hitbox = 0;
	temp_info.cur_AT = 0;
	temp_info.cur_AS = 0;
	temp_info.cur_EF = 0;
	temp_info.cur_IF = 0;
	
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "PTCN", 4)) {
			//Name in code. Seems to be unused in melty.
			unsigned int len = data[0];
			data += 1;

			assert (len < 64);
			char str[65];
			memcpy(str, data, len);
			str[len] = '\0';
			
			seq->codeName = str;
			
			data = (unsigned int *)(((unsigned char *)data)+len);
		} else if (!memcmp(buf, "PSTS", 4)) {
			//Maybe 技情報. Has to do with the kind of move?
			//Doesn't appear = 0, Movement
			//1 Regular attacks
			//2 Throw. Only Arc (and b_arc) uses it?
			//3 必殺技? FRies 236C uses it
			//4 必殺投げ　Some kouma grab
			//5 他 Projectiles, but not effects.
			//6 NAC's 96. 超必殺技
			seq->psts = *data;
			assert(*data <= 6 && *data >= 0); //known values
			++data;
		} else if (!memcmp(buf, "PLVL", 4)) {
			//Determines rebeat
			seq->level = *data;
			assert(*data > 0);
			++data;
		} else if (!memcmp(buf, "PFLG", 4)) {
			//Unknown. Always 1?
			seq->flag = *data;
			assert(*data == 1);
			++data;
		} else if (!memcmp(buf, "PDST", 4)) {
			// PDST is only used on G_CHAOS
			// seems to be an older format with vague
			// allocations. On-demand maybe.
			++data;
		} else if (!memcmp(buf, "PTT2", 4)) {
			// variable-length sequence title
			unsigned int len = data[0];
			assert (len < 64);
			char str[65];
			memcpy(str, data+1, len);
			str[len] = '\0';
			
			seq->name = str;
			seq->name = sj2utf8(seq->name);

			data += 1 + ((len+3)/4);
		} else if (!memcmp(buf, "PTIT", 4)) {
			//TODO: verify and remove
			assert(0 && "PTIT");
			// fixed-length sequence title
			char str[33];
			memcpy(str, data, 32);
			str[32] = '\0';
			
			
			seq->name = str;
			seq->name = sj2utf8(seq->name);
			
			data += 8;
		} else if (!memcmp(buf, "PDS2", 4)) {
			// this is an allocation call
			// format:
			// data[0] = byte count // Always 32?
			// data[1] = frame count
			// data[2] = hitbox count
			// data[3] = EF count
			// data[4] = IF count
			// data[5] = AT count
			// data[6] = unused
			// data[7] = AS count
			// data[8] = frame count
			if (data[0] == 32) {

				//Ended up a lot more simple huh
				seq->frames.resize(data[1]);
				temp_info.boxesRefs.resize(data[2]);
				seq->EF.resize(data[3]);
				seq->IF.resize(data[4]);
				seq->AT.resize(data[5]);
				seq->AS.resize(data[7]);

				nframes = data[1];

				assert(data[8] == data[1]); //Just to make sure.

				seq->initialized = 1;
			}
			else
				assert(0 && "PSD2 size is not 32");
			data += 1 + (data[0]/4);

		} else if (!memcmp(buf, "FSTR", 4)) {
			if (seq->initialized && frame_it < nframes) {
				Frame *frame = &seq->frames[frame_it];
				temp_info.cur_frame = frame_it;
				data = fd_frame_load(data, data_end, frame, &temp_info);
				for(const auto &delayLoad : temp_info.delayLoadList)
				{
					Frame &frame = seq->frames[delayLoad.frameNo];
					frame.hitboxes[delayLoad.location] = *temp_info.boxesRefs[delayLoad.source];
				}
			
				++frame_it;
			}
			else
			{
				assert(0 && "Actual frame number and PDS2 don't match");
			}
		} else if (!memcmp(buf, "PEND", 4)) {
			break;
		} else
			assert(0);
	}
	
	if(seq->initialized)
		assert(frame_it == nframes);
	
	return data;
}

static unsigned int *fd_main_load(unsigned int *data, const unsigned int *data_end,
				Sequence *sequences,
				unsigned int nsequences) {
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "PSTR", 4)) {
			unsigned int seq_id = *data;
			++data;
			
			// make sure there's actually something here.
			if (memcmp(data, "PEND", 4)) {
				if (seq_id < nsequences) {
					sequences[seq_id].empty = false;
					data = fd_sequence_load(data, data_end, &sequences[seq_id]);
				}
			} else {
				++data;
			}
		} else if (!memcmp(buf, "_END", 4)) {
			break;
		}
	}
	
	return data;
}

bool FrameData::load(const char *filename) {
	// allow loading over existing data
	
	char *data;
	unsigned int size;
	
	if (!ReadInMem(filename, data, size)) {
		return 0;
	}
	
	// verify header
	if (memcmp(data, "Hantei6DataFile", 15)) {
		delete[] data;
		
		return 0;
	}
	
	// initialize the root
	unsigned int *d = (unsigned int *)(data + 0x20);
	unsigned int *d_end = (unsigned int *)(data + size);
	if (memcmp(d, "_STR", 4)) {
		delete[] data;
		return 0;
	}
	
	unsigned int sequence_count = d[1];
	
	Free();
	m_nsequences = sequence_count;
	m_sequences = new Sequence[m_nsequences];

	d += 2;	
	// parse and recursively store data
	d = fd_main_load(d, d_end, m_sequences, m_nsequences);
	
	// cleanup and finish
	delete[] data;
	
	m_loaded = 1;
	
	return 1;
}

static char *split_line(char **data) {
	char *start = *data;
	
	while (*start == ' ' || *start == '\t') {
		++start;
	}
	
	char *p = start;
	while (*p && *p != '\n' && *p != '\r') ++p;
	
	if (*p) {
		*p++ = '\0';
	}
	*data = p;
	
	return start;
}

void FrameData::Free() {
	delete[] m_sequences;
	m_sequences = nullptr;
	m_nsequences = 0;
	m_loaded = 0;
}

int FrameData::get_sequence_count() {
	if (!m_loaded) {
		return 0;
	}
	return m_nsequences;
}

Sequence* FrameData::get_sequence(int n) {
	if (!m_loaded) {
		return 0;
	}
	
	if (n < 0 || (unsigned int)n >= m_nsequences) {
		return 0;
	}
	
	return &m_sequences[n];
}

std::string FrameData::GetDecoratedName(int n)
{
		std::stringstream ss;
		ss.flags(std::ios_base::right);
		
		ss << std::setfill('0') << std::setw(3) << n << " ";
		
		if(!m_sequences[n].empty)
		{
			//Debug
			if(m_sequences[n].psts > 1 && m_sequences[n].psts !=5)
				ss << " (!) ";

			bool noFrames = m_sequences[n].frames.empty();
			if(noFrames)
				ss << u8"〇 ";

			ss << m_sequences[n].name;
			if(m_sequences[n].name.empty() && !noFrames)
			{
					ss << u8"Untitled";
			}
		}
		
		return ss.str();
}

FrameData::FrameData() {
	m_sequences = nullptr;
	
	m_nsequences = 0;
	
	m_loaded = 0;
}

FrameData::~FrameData() {
	Free();
}
