#include <cassert>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <windows.h>
#include "framedata_load.h"
#include "misc.h"

TestInfo test;

void TestInfo::Print(const void *data, const void *data_end)
{
	std::cout <<filename<<" "<<seqName<<" "<<seqId<<":"<<frame<<"\t";
	std::cout <<"Offsets: "<<(uint64_t)((const char*)data_end - (const char*)data) << "\n";
}

//Attack data
unsigned int *fd_frame_AT_load(unsigned int *data, const unsigned int *data_end, Frame_AT *AT, TempInfo *info)
{
	AT->correction = 100;
	AT->damageProration = 100; // Default: no proration
	AT->minDamage = 0;
	AT->addHitStun = 0;
	AT->starterCorrection = 0;
	AT->hitStunDecay[0] = 0;
	AT->hitStunDecay[1] = 0;
	AT->hitStunDecay[2] = 0;
	
	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "ATGD", 4)) {
			//If absent it's UB
			AT->guard_flags = data[0];

			/* if(data[0] & 1<<15)
			{
				test.Print(data, data_end);
				std::cout << "ATGD "<<data[0]<<"\n";
			} */
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
				//Only old ckohamech has buggy flag values.
			}
			data += data[0]+1;
		} else if (!memcmp(buf, "ATHV", 4)) {
			//Same
			assert(data[0] <= 3);
			for(int i = 0; i < data[0]; i++)
			{
				AT->hitVector[i] = data[i+1] & 0xFF;
				AT->hVFlags[i] = data[i+1] >> 8;
			}
			data += data[0]+1;
		} else if (!memcmp(buf, "ATF1", 4)) {
			AT->otherFlags = data[0];

 			/* if(data[0] != 0)
			{
				test.Print(data, data_end);
				std::cout << "ATF1 "<< data[0]<<"\n";
			} */
			++data;
		} else if (!memcmp(buf, "ATHE", 4)) {
			AT->hitEffect = data[0];
			AT->soundEffect = data[1];
			data += 2;
		} else if (!memcmp(buf, "ATKK", 4)) {
			AT->addedEffect = data[0];
			//Uni uses higher values.
			data++;
		} else if (!memcmp(buf, "ATNG", 4)) {
			//Melty only uses 1. UNI uses higher values.
			AT->hitgrab = data[0];
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
		} else if (!memcmp(buf, "ATGN", 4)) {
			AT->blockStopTime = data[0];
			data++;
		} else if (!memcmp(buf, "ATV2", 4)) {
			// UNI combined vector format (standing, air, crouch HIT+GUARD vectors)
			assert(data[0] == 3 && data[1] == 2);
			data += 2;
			for(int i = 0; i < 3*4; i+=4)
			{
				AT->hVFlags[i/4] = data[i];
				AT->hitVector[i/4] = data[i+1];
				AT->gVFlags[i/4] = data[i+2];
				AT->guardVector[i/4] = data[i+3];
				assert(AT->hVFlags[i/4] <= 3 && AT->gVFlags[i/4] <= 3);
			}
			data += 12;
			// Mark this sequence as using UNI attack format
			if (info && info->seq) {
				info->seq->usedATV2 = true;
			}
		} else if (!memcmp(buf, "ATAT", 4)) {
			// UNI standalone damage (alternative to ATVV)
			AT->damage = data[0];
			++data;
		} else if (!memcmp(buf, "ATHH", 4)) {
			// UNI damage proration (percentage, 100 = no reduction)
			AT->damageProration = data[0];
			++data;
		} else if (!memcmp(buf, "ATAM", 4)) {
			// UNI minimum damage percentage
			AT->minDamage = data[0];
			++data;
		} else if (!memcmp(buf, "ATCA", 4)) {
			// UNI meter gain (separate from ATVV in UNI format)
			AT->meter_gain = data[0];
			++data;
		} else if (!memcmp(buf, "ATC0", 4)) {
			// UNI hit stun decay [reduction, combopoint_set, combopoint_SMP_modifier]
			memcpy(AT->hitStunDecay, data, sizeof(int)*3);
			data += 3;
		} else if (!memcmp(buf, "ATSA", 4)) {
			// UNI add hit stun (player stun time)
			AT->addHitStun = data[0];
			++data;
		} else if (!memcmp(buf, "ATSH", 4)) {
			// UNI starter correction
			AT->starterCorrection = data[0];
			++data;
		} else if (!memcmp(buf, "ATED", 4)) {
			break;
		} else {
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown AT tag: " << tag <<"\n";
		}

		//Unhandled: None, unless they're not vanilla melty files.
	}
	
	return data;
}

unsigned int *fd_frame_AS_load(unsigned int *data, const unsigned int *data_end, Frame_AS *AS)
{

	while (data < data_end) {
		unsigned int *buf = data;
		++data;
		
		if (!memcmp(buf, "ASV0", 4)) {
			AS->movementFlags = data[0];
			AS->speed[0] = data[1];
			AS->speed[1] = data[2];
			AS->accel[0] = data[3];
			AS->accel[1] = data[4];
			if((AS->movementFlags&~0x33) != 0)
			{
				test.Print(data, data_end);
				std::cout << "Unknown ASV0 flags: "<<AS->movementFlags<<"\n";
			}
			data += 5;
		} else if (!memcmp(buf, "ASVX", 4)) {
			AS->movementFlags = 0x11; //Set only
			AS->speed[0] = 0;
			AS->speed[1] = 0;
			AS->accel[0] = 0;
			AS->accel[1] = 0;
		} else if (!memcmp(buf, "ASMV", 4)) {
			AS->canMove = data[0];
			if(AS->canMove != 1)
			{
				test.Print(data, data_end);
				std::cout << "ASMV: "<<AS->canMove<<"\n";
			}
			++data;
		} else if (!memcmp(buf, "ASS1", 4)) {
			AS->stanceState = 1; // airborne
		} else if (!memcmp(buf, "ASS2", 4)) {
			AS->stanceState = 2; // crouching
		} else if (!memcmp(buf, "ASCN", 4)) {
			AS->cancelNormal = data[0];
			if(AS->cancelNormal > 3 || AS->cancelNormal < 1)
			{
				test.Print(data, data_end);
				std::cout << "Cancel N: "<<AS->cancelNormal<<"\n";
			}
			data++;
		} else if (!memcmp(buf, "ASCS", 4)) {
			AS->cancelSpecial = data[0];
			if(AS->cancelSpecial > 3 || AS->cancelSpecial < 1)
			{
				test.Print(data, data_end);
				std::cout << "Cancel S: "<<AS->cancelSpecial<<"\n";
			}
			data++;
		} else if (!memcmp(buf, "ASCT", 4)) {
			AS->counterType = data[0];
			if(AS->counterType > 3 || AS->counterType < 1)
			{
				test.Print(data, data_end);
				std::cout << "Counter T: "<<AS->counterType<<"\n";
			}
			data++;
		} else if (!memcmp(buf, "AST0", 4)) {
			AS->sineFlags = data[0] & 0xFF; //Other values have no effect.
			memcpy(AS->sineParameters, data+1, sizeof(int)*4);
			AS->sinePhases[0] = ((float*)data)[5];
			AS->sinePhases[1] = ((float*)data)[6];
			if((data[0]&~0x11) != 0)
			{
				test.Print(data, data_end);
				std::cout << "AST0 has nonstandard flags\n";
			}
			data += 7;
		} else if (!memcmp(buf, "ASMX", 4)) {
			AS->maxSpeedX = data[0];
			data++;
		} else if (!memcmp(buf, "ASAA", 4)) {
			AS->hitsNumber = data[0];
			
			data++;
		} else if (!memcmp(buf, "ASYS", 4)) {
			AS->invincibility = data[0];
			if(data[0] > 5)
			{
				test.Print(data, data_end);
				std::cout <<"\tUnknown ASYS value: " << data[0] <<"\n";
			}
			data++;
		} else if (!memcmp(buf, "ASCF", 4)) {
			// UNI counter/cancel flag
			AS->ascf = data[0];
			data++;
		} else if (!memcmp(buf, "ASF", 3)) {
			char t = ((char *)buf)[3];
			if(t != '0' && t !='1')
			{
				test.Print(data, data_end);
				std::cout <<"\tUnknown ASF suffix" << t <<"\n";
			}
			else
			{
				AS->statusFlags[t-'0'] = data[0];
			}
			data++;
		} else if (!memcmp(buf, "ASED", 4)) {
			break;
		} else {
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown AS tag: " << tag <<"\n";
		}
		
		//Unhandled: None, unless they're not in vanilla melty files.
	}
	
	return data;
}

unsigned int *fd_frame_EF_load(unsigned int *data, const unsigned int *data_end, Frame_EF *EF)
{
	// Initialize all parameters to 0 to prevent garbage data
	EF->type = 0;
	EF->number = 0;
	for (int i = 0; i < 12; ++i) {
		EF->parameters[i] = 0;
	}

	while (data < data_end) {
		unsigned int *buf = data;
		++data;

		if (!memcmp(buf, "EFTP", 4)) {
			EF->type = data[0];
			++data;
		} else if (!memcmp(buf, "EFNO", 4)) {
			EF->number = data[0];
			++data;
		} else if (!memcmp(buf, "EFPR", 4)) {
			int count = data[0];
			if (count <= 12) {
				for (int i = 0; i < count; ++i) {
					EF->parameters[i] = data[i+1];
				}
				/* if(EF->type == 1 )
				{
					if(maxCount < count)
						 maxCount = count;
					numberSet.insert(EF->parameters[1]);
					test.Print(data, data_end);
					std::cout <<"\tEFTP " << EF->type <<" NO "<<EF->number<<" Params:";
					for (int i = 0; i < count; ++i) {
						std::cout <<" "<<EF->parameters[i];
					}
					std::cout<<"\n";
				} */
			} else {
				test.Print(data, data_end);
				std::cout <<"\tUnhandled number of EF parameters: " << count <<"\n";
			}
			data += count + 1;
		} else if (!memcmp(buf, "EFED", 4)) {
			break;
		} else {
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown EF tag: " << tag <<"\n";
		}
	}
	
	return data;
}

unsigned int *fd_frame_IF_load(unsigned int *data, const unsigned int *data_end, Frame_IF *IF)
{
	// Initialize all parameters to 0 to prevent garbage data
	IF->type = 0;
	for (int i = 0; i < 9; ++i) {
		IF->parameters[i] = 0;
	}

	while (data < data_end) {
		unsigned int *buf = data;
		++data;

		if (!memcmp(buf, "IFTP", 4)) {
			IF->type = data[0];
			
			
			++data;
		} else if (!memcmp(buf, "IFPR", 4)) {
			int count = data[0];
			if (count <= 9) {
				for (int i = 0; i < count; ++i) {
					IF->parameters[i] = data[i+1];
				}
				/* if(IF->type == 3 && count > 2)
				{
					if(maxCount < count)
						 maxCount = count;
					numberSet.insert(IF->parameters[4]);
					test.Print(data, data_end);
					std::cout <<"\tIFTP " << IF->type <<" Params:";
					for (int i = 0; i < count; ++i) {
						std::cout <<" "<<IF->parameters[i];
					}
					std::cout<<"\n";
				} */
			} else {
				test.Print(data, data_end);
				std::cout <<"\tUnhandled number of IF parameters: " << count <<"\n";
			}
			data += count + 1;
		} else if (!memcmp(buf, "IFED", 4)) {
			break;
		} else {
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown IF tag: " << tag <<"\n";
		}
	}
	
	return data;
}

unsigned int *fd_frame_AF_load(unsigned int *data, const unsigned int *data_end, Frame *frame, bool *usedAFGX)
{
	// Track current layer for per-layer properties
	Layer_Type* currentLayer = nullptr;
	int currentLayerId = -1;

	while (data < data_end) {
		unsigned int *buf = data;
		++data;

		if (!memcmp(buf, "AFGP", 4)) {
			// MBAACC format - single layer
			if (frame->AF.layers.empty()) {
				frame->AF.layers.push_back({});
			}
			currentLayer = &frame->AF.layers[0];
			currentLayerId = 0;

			int *dt = (int *)data;
			currentLayer->usePat = dt[0];
			currentLayer->spriteId = dt[1];
			data += 2;
		} else if (!memcmp(buf, "AFGX", 4)) {
			// UNI multi-layer format
			int *dt = (int *)data;
			int layerId = dt[0];
			int usePat = dt[1];
			int spriteId = dt[2];

			// Ensure layers vector has enough space
			if (layerId >= (int)frame->AF.layers.size()) {
				frame->AF.layers.resize(layerId + 1);
			}

			Layer_Type& layer = frame->AF.layers[layerId];
			layer.usePat = usePat;
			layer.spriteId = spriteId;
			currentLayer = &layer;
			currentLayerId = layerId;

			if(usedAFGX) *usedAFGX = true; // Mark that this sequence uses UNI format
			data += 3;
		} else if (!memcmp(buf, "AFOF", 4)) {
			// Layer offset - only apply to current layer
			if (currentLayer) {
				int *dt = (int *)data;
				currentLayer->offset_x = dt[0];
				currentLayer->offset_y = dt[1];
			}
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
			// Overrides AFOF - layer property
			if (currentLayer) {
				currentLayer->offset_x = 0;
				char t = ((char *)buf)[3];
				if (t >= '0' && t <= '9') {
					int v = (t - '0');
					if (v < 4) {
						v += 10;
					}
					currentLayer->offset_y = v;
				} else if (t == 'X') {
					currentLayer->offset_y = 10;
				}
			}
		} else if (!memcmp(buf, "AFF", 3)) {
			char t = ((char *)buf)[3];
			if (t >= '1' && t <= '2') {
				//Only values 1 and 2 are used.
				frame->AF.aniType = t - '0';
			} else if (t == 'L') {
				// AFFL - long form with int32 value (typically 3 for animation end)
				frame->AF.aniType = data[0];
				++data;
			} else if (t == 'E') {
				frame->AF.aniFlag = data[0];
				++data;
			}
			else {
				test.Print(data, data_end);
				std::cout <<"\tAFF uses uknown value: " << t <<"\n";
			}

		} else if (!memcmp(buf, "AFAL", 4)) {
			// Layer blend mode and alpha
			if (currentLayer) {
				currentLayer->blend_mode = data[0];
				currentLayer->rgba[3] = ((float)data[1])/255.f;
				assert(data[0] >= 1 || data[0] <= 3 );
			}
			data += 2;
		} else if (!memcmp(buf, "AFRG", 4)) {
			// Layer color tint
			if (currentLayer) {
				currentLayer->rgba[0] = ((float)data[0])/255.f;
				currentLayer->rgba[1] = ((float)data[1])/255.f;
				currentLayer->rgba[2] = ((float)data[2])/255.f;
			}
			data += 3;
		} else if (!memcmp(buf, "AFAZ", 4)) {
			// Layer Z-axis rotation
			if (currentLayer) {
				currentLayer->rotation[2] = *(float *)data;
			}
			++data;
		} else if (!memcmp(buf, "AFAY", 4)) {
			// Layer Y-axis rotation
			if (currentLayer) {
				currentLayer->rotation[1] = *(float *)data;
			}
			++data;
		} else if (!memcmp(buf, "AFAX", 4)) {
			// Layer X-axis rotation
			if (currentLayer) {
				currentLayer->rotation[0] = *(float *)data;
			}
			++data;
		} else if (!memcmp(buf, "AFZM", 4)) {
			// Layer scale
			if (currentLayer) {
				currentLayer->scale[0] = ((float *)data)[0];
				currentLayer->scale[1] = ((float *)data)[1];
			}
			data += 2;
		} else if (!memcmp(buf, "AFJP", 4)) {
			frame->AF.jump = data[0];
			++data;
		} else if (!memcmp(buf, "AFHK", 4)) {
			frame->AF.interpolationType = data[0];
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
			// Overrides rotation - layer property
			if (currentLayer) {
				currentLayer->rotation[0] = data[0] ? 0.5f : 0.f;
				currentLayer->rotation[1] = data[1] ? 0.5f : 0.f;
			}
			data += 2;
		} else if (!memcmp(buf, "AFPL", 4)) {
			// UNI layer priority (AFGX only)
			if (currentLayer) {
				currentLayer->priority = data[0];
			}
			++data;
		} else if (!memcmp(buf, "AFRT", 4)) {
			//Some fucked up interaction with rotation and scale.
			frame->AF.AFRT = data[0];
			++data;
		} else if (!memcmp(buf, "AFID", 4)) {
			// UNI frame ID for Squirrel script reference
			frame->AF.frameId = data[0];
			++data;
		} else if (!memcmp(buf, "AFPA", 4)) {
			// UNI frame parameters for Squirrel script reference
			memcpy(frame->AF.param, data, sizeof(uint8_t)*4);
			++data;
		} else if (!memcmp(buf, "AFED", 4)) {
			break;
		} else {
			// Unknown AF tag - silently skip
			// char tag[5]{};
			// memcpy(tag,buf,4);
			// test.Print(data, data_end);
			// std::cout <<"\tUnknown AF tag: " << tag <<"\n";
		}
		//Unhandled: None, unless they're not in vanilla melty files.
	}

	return data;
}

unsigned int *fd_frame_load(unsigned int *data, const unsigned int *data_end, Frame *frame, TempInfo *info, bool *usedAFGX = nullptr)
{
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

				memcpy(hitbox->xy, data+1, sizeof(int)*4);
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
				data = fd_frame_AT_load(data, data_end, &frame->AT, info);
			
		} else if (!memcmp(buf, "ASST", 4)) {
			// start state block
			if (info->cur_AS < info->AS.size()) {
				info->AS[info->cur_AS] = &frame->AS;
				++info->cur_AS;

				data = fd_frame_AS_load(data, data_end, &frame->AS);
			}
		} else if (!memcmp(buf, "ASSM", 4)) {
			// reference state block
			unsigned int value = data[0];
			++data;
			
			// Might give trouble if it's a future reference but who gives a shit.
			if(value < info->cur_AS)
			{
				frame->AS = *info->AS[value];
			}
			else
			{
				test.Print(data, data_end);
				std::cout <<"\tASSM future reference: " << data[0] <<":"<<info->cur_AS<<"\n";
			}

		} else if (!memcmp(buf, "AFST", 4)) {
			// start animation block
			data = fd_frame_AF_load(data, data_end, frame, usedAFGX);
		} else if (!memcmp(buf, "EFST", 4)) {
			// start effect flags block
			//int n = data[0];
			frame->EF.push_back({});
			++data;
			data = fd_frame_EF_load(data, data_end, &frame->EF.back());

		} else if (!memcmp(buf, "IFST", 4)) {
			// start condition block
			//int n = data[0];

			frame->IF.push_back({});
			++data;
			data = fd_frame_IF_load(data, data_end, &frame->IF.back());
		} else if (!memcmp(buf, "FSNA", 4)) {
			//Max index of used attack boxes + 1
			++data;
		} else if (!memcmp(buf, "FSNH", 4)) {
			//Max index of used hantei boxes + 1
			++data;
		} else if (!memcmp(buf, "FSNE", 4)) {
			//Max index of used effects + 1
			++data;
		} else if (!memcmp(buf, "FSNI", 4)) {
			//Max index of used ifs + 1
			++data;
		} else if (!memcmp(buf, "FEND", 4)) {
			break;
		} else {
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown Frame level tag: " << tag <<"\n";
		}

		//Unhandled: None, unless they're not in vanilla melty files.
	}
	
	return data;
}

unsigned int *fd_sequence_load(unsigned int *data, const unsigned int *data_end, Sequence *seq, bool utf8)
{

	TempInfo temp_info;
	unsigned int frame_it = 0, nframes = 0;
	
	temp_info.seq = seq;
	temp_info.cur_hitbox = 0;
	temp_info.cur_AS = 0;
	
	std::string name, codename;
	int level = 0, psts = 0, flag = 0, pups = 0;
	
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
			
			codename = str;
			
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
			psts = *data;
			assert(*data <= 6 && *data >= 0); //known values
			++data;
		} else if (!memcmp(buf, "PLVL", 4)) {
			//Determines rebeat
			level = *data;
			assert(*data > 0);
			++data;
		} else if (!memcmp(buf, "PFLG", 4)) {
			//Unknown. Always 1?
			flag = *data;
			assert(*data == 1);
			++data;
		} else if (!memcmp(buf, "PUPS", 4)) {
			// UNI palette switching (0=default, 1=_p1.pal, 2=_p2.pal, etc.)
			pups = *data;
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

			// Convert Shift-JIS to UTF-8 for internal use
			// Modern HA6 files store strings as Shift-JIS
			// Legacy files (with UTF-8 flag) already have UTF-8 strings
			std::string originalStr = str;
			if(!utf8)
				name = sj2utf8(str);
			else
				name = str;
			test.seqName = name;

			data = (unsigned int *)(((unsigned char *)data)+len)+1;
		} else if (!memcmp(buf, "PTIT", 4)) {
			// fixed-length sequence title
			// Never used.
			char str[33];
			memcpy(str, data, 32);
			str[32] = '\0';

			name = str;
			// Convert Shift-JIS to UTF-8 if needed (backwards compatibility)
			if(!utf8)
				name = sj2utf8(name);

			data += 8;
		} else if (!memcmp(buf, "PDS2", 4)) {
			// this is an allocation call
			// format:
			// data[0] = byte count. Always 32
			// data[1] = frame count
			// data[2] = hitbox count
			// data[3] = EF count
			// data[4] = IF count
			// data[5] = AT count
			// data[6] = unused
			// data[7] = AS count
			// data[8] = frame count
			if (data[0] == 32) {
				seq->frames.clear();
				seq->frames.resize(data[1]);

				//Only AS and boxes have references.
				temp_info.boxesRefs.resize(data[2]);
				temp_info.AS.resize(data[7]);

				nframes = data[1];

				assert(data[8] == data[1]); //Just to make sure.

				seq->initialized = 1;

				seq->name = name;
				seq->codeName = codename;
				seq->psts = psts;
				seq->level = level;
				seq->flag = flag;
				seq->pups = pups;
			}
			else
				assert(0 && "PSD2 size is not 32");
			data += 1 + (data[0]/4);

		} else if (!memcmp(buf, "FSTR", 4)) {
			if (seq->initialized && frame_it < nframes) {
				Frame *frame = &seq->frames[frame_it];
				temp_info.cur_frame = frame_it;
				test.frame = frame_it;
				data = fd_frame_load(data, data_end, frame, &temp_info, &seq->usedAFGX);
						
				++frame_it;
			}
			else
			{
				assert(0 && "Actual frame number and PDS2 don't match");
			}
		} else if (!memcmp(buf, "PEND", 4)) {
			for(const auto &delayLoad : temp_info.delayLoadList)
			{
				Frame &frame = seq->frames[delayLoad.frameNo];
				frame.hitboxes[delayLoad.location] = *temp_info.boxesRefs[delayLoad.source];
			}
			test.seqName = "";
			break;
		} else
		{
			char tag[5]{};
			memcpy(tag,buf,4);
			test.Print(data, data_end);
			std::cout <<"\tUnknown Pattern level tag: " << tag <<"\n";
		}
	}
	
	if(seq->initialized)
		assert(frame_it == nframes);
	
	return data;
}

unsigned int *fd_main_load(unsigned int *data, const unsigned int *data_end, std::vector<Sequence> &sequences, unsigned int nsequences, bool utf8)
{
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
					test.seqId = seq_id;
					data = fd_sequence_load(data, data_end, &sequences[seq_id], utf8);
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