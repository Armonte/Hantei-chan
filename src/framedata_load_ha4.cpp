#include <cassert>
#include <iostream>
#include <cstdint>
#include <cstring>
#include "framedata_load.h"
#include "framedata.h"
#include "ha4_format.h"
#include "misc.h"

// HA4 Format Loader - Melty Blood Act Cadenza
// Converts HA4 format to HA6 internal structures
// One-way import only (no export support needed)

// Load HA4 effect data and convert to HA6 Frame_EF
// HA4 effects map 1:1 to HA6 for types 1-37
unsigned int *fd_frame_EF_load_ha4(unsigned int *data, const unsigned int *data_end, Frame_EF *EF)
{
    // HA4 effect structure is 52 bytes:
    // - int16_t type (2 bytes)
    // - int16_t number (2 bytes)
    // - int32_t param[12] (48 bytes)

    if ((uintptr_t)(data_end) - (uintptr_t)(data) < HA4_EFFECT_SIZE) {
        std::cerr << "HA4 Error: Not enough data for effect structure\n";
        return data;
    }

    // Read 52-byte HA4 effect structure directly
    const HA4_Effect *ha4_ef = reinterpret_cast<const HA4_Effect*>(data);

    // Convert to HA6 format (types map 1:1)
    EF->type = ha4_ef->type;
    EF->number = ha4_ef->number;

    // Copy parameters (HA4 has 12, HA6 has 12)
    for (int i = 0; i < 12; ++i) {
        EF->parameters[i] = ha4_ef->param[i];
    }

    // Advance pointer by 52 bytes
    return (unsigned int*)((uint8_t*)data + HA4_EFFECT_SIZE);
}

// Load HA4 condition data and convert to HA6 Frame_IF
// HA4 IF types 001-037 map 1:1 to HA6 IF types 1-37
unsigned int *fd_frame_IF_load_ha4(unsigned int *data, const unsigned int *data_end, Frame_IF *IF)
{
    // HA4 condition structure is 52 bytes (same as effect):
    // - int16_t type (2 bytes)
    // - int16_t reserved (2 bytes)
    // - int32_t param[12] (48 bytes)

    if ((uintptr_t)(data_end) - (uintptr_t)(data) < HA4_CONDITION_SIZE) {
        std::cerr << "HA4 Error: Not enough data for condition structure\n";
        return data;
    }

    // Read 52-byte HA4 condition structure directly
    const HA4_Condition *ha4_if = reinterpret_cast<const HA4_Condition*>(data);

    // Convert to HA6 format (types map 1:1)
    IF->type = ha4_if->type;

    // Copy parameters (HA4 has 12, HA6 uses first 9)
    for (int i = 0; i < 9; ++i) {
        IF->parameters[i] = ha4_if->param[i];
    }

    // Advance pointer by 52 bytes
    return (unsigned int*)((uint8_t*)data + HA4_CONDITION_SIZE);
}

// Load HA4 animation frame data (AFST section)
// HA4 uses tag-based format like HA6, but with some differences:
// - Uses AFGP instead of AFGX (simpler glow system)
// - No AFID, AFPA (UNI additions)
unsigned int *fd_frame_AF_load_ha4(unsigned int *data, const unsigned int *data_end, Frame *frame)
{
    frame->AF.spriteId = -1;

    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "AFGP", 4)) {
            // HA4 format: AFGP contains [usePat, spriteId]
            // Same as HA6 MBAACC format
            int *dt = (int *)data;
            frame->AF.spriteId = dt[1];
            frame->AF.usePat = dt[0];
            data += 2;
        } else if (!memcmp(buf, "AFOF", 4)) {
            // Offset X, Y
            int *dt = (int *)data;
            frame->AF.offset_y = dt[1];
            frame->AF.offset_x = dt[0];
            data += 2;
        } else if (!memcmp(buf, "AFD", 3)) {
            // Frame duration
            char t = ((char *)buf)[3];
            if (t >= '0' && t <= '9') {
                frame->AF.duration = t - '0';
            } else if (t == 'L') {
                frame->AF.duration = data[0];
                ++data;
            }
        } else if (!memcmp(buf, "AFY", 3)) {
            // Y position shorthand (overrides AFOF)
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
            // Flow control
            char t = ((char *)buf)[3];
            if (t >= '1' && t <= '2') {
                frame->AF.aniType = t - '0';
            } else if (t == 'E') {
                frame->AF.aniFlag = data[0];
                ++data;
            }
        } else if (!memcmp(buf, "AFAL", 4)) {
            // Alpha/blend mode
            frame->AF.blend_mode = data[0];
            frame->AF.rgba[3] = ((float)data[1])/255.f;
            data += 2;
        } else if (!memcmp(buf, "AFRG", 4)) {
            // RGB color
            frame->AF.rgba[0] = ((float)data[0])/255.f;
            frame->AF.rgba[1] = ((float)data[1])/255.f;
            frame->AF.rgba[2] = ((float)data[2])/255.f;
            data += 3;
        } else if (!memcmp(buf, "AFAZ", 4)) {
            // Z rotation
            frame->AF.rotation[2] = *(float *)data;
            ++data;
        } else if (!memcmp(buf, "AFAY", 4)) {
            // Y rotation
            frame->AF.rotation[1] = *(float *)data;
            ++data;
        } else if (!memcmp(buf, "AFAX", 4)) {
            // X rotation
            frame->AF.rotation[0] = *(float *)data;
            ++data;
        } else if (!memcmp(buf, "AFZM", 4)) {
            // Scale X, Y
            frame->AF.scale[0] = ((float *)data)[0];
            frame->AF.scale[1] = ((float *)data)[1];
            data += 2;
        } else if (!memcmp(buf, "AFJP", 4)) {
            // Jump frame
            frame->AF.jump = data[0];
            ++data;
        } else if (!memcmp(buf, "AFHK", 4)) {
            // Interpolation type
            frame->AF.interpolationType = data[0];
            ++data;
        } else if (!memcmp(buf, "AFPR", 4)) {
            // Layer priority
            frame->AF.priority = data[0];
            ++data;
        } else if (!memcmp(buf, "AFCT", 4)) {
            // Loop count
            frame->AF.loopCount = data[0];
            ++data;
        } else if (!memcmp(buf, "AFLP", 4)) {
            // Loop end frame
            frame->AF.loopEnd = data[0];
            ++data;
        } else if (!memcmp(buf, "AFJC", 4)) {
            // Land jump
            frame->AF.landJump = data[0];
            ++data;
        } else if (!memcmp(buf, "AFTN", 4)) {
            // Flip flags (overrides rotation)
            frame->AF.rotation[0] = data[0] ? 0.5f : 0.f;
            frame->AF.rotation[1] = data[1] ? 0.5f : 0.f;
            data += 2;
        } else if (!memcmp(buf, "AFRT", 4)) {
            // AFRT flag (rotation/scale interaction)
            frame->AF.AFRT = data[0];
            ++data;
        } else if (!memcmp(buf, "AFED", 4)) {
            // End of animation block
            break;
        } else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cerr << "HA4 Warning: Unknown AF tag: " << tag << "\n";
        }
    }

    return data;
}

// Load HA4 state data (ASST section)
// HA4 state parameters are mostly identical to HA6
unsigned int *fd_frame_AS_load_ha4(unsigned int *data, const unsigned int *data_end, Frame_AS *AS)
{
    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "ASV0", 4)) {
            // Movement velocity
            AS->movementFlags = data[0];
            AS->speed[0] = data[1];
            AS->speed[1] = data[2];
            AS->accel[0] = data[3];
            AS->accel[1] = data[4];
            data += 5;
        } else if (!memcmp(buf, "ASVX", 4)) {
            // Clear velocity
            AS->movementFlags = 0x11;
            AS->speed[0] = 0;
            AS->speed[1] = 0;
            AS->accel[0] = 0;
            AS->accel[1] = 0;
        } else if (!memcmp(buf, "ASMV", 4)) {
            // Can move flag
            AS->canMove = data[0];
            ++data;
        } else if (!memcmp(buf, "ASS1", 4)) {
            // Airborne stance
            AS->stanceState = 1;
        } else if (!memcmp(buf, "ASS2", 4)) {
            // Crouching stance
            AS->stanceState = 2;
        } else if (!memcmp(buf, "ASCN", 4)) {
            // Normal cancel
            AS->cancelNormal = data[0];
            data++;
        } else if (!memcmp(buf, "ASCS", 4)) {
            // Special cancel
            AS->cancelSpecial = data[0];
            data++;
        } else if (!memcmp(buf, "ASCT", 4)) {
            // Counter type
            AS->counterType = data[0];
            data++;
        } else if (!memcmp(buf, "AST0", 4)) {
            // Sine wave motion
            AS->sineFlags = data[0] & 0xFF;
            memcpy(AS->sineParameters, data+1, sizeof(int)*4);
            AS->sinePhases[0] = ((float*)data)[5];
            AS->sinePhases[1] = ((float*)data)[6];
            data += 7;
        } else if (!memcmp(buf, "ASMX", 4)) {
            // Max speed X
            AS->maxSpeedX = data[0];
            data++;
        } else if (!memcmp(buf, "ASAA", 4)) {
            // Hit number
            AS->hitsNumber = data[0];
            data++;
        } else if (!memcmp(buf, "ASYS", 4)) {
            // Invincibility
            AS->invincibility = data[0];
            data++;
        } else if (!memcmp(buf, "ASF", 3)) {
            // Status flags
            char t = ((char *)buf)[3];
            if (t == '0' || t == '1') {
                AS->statusFlags[t-'0'] = data[0];
            }
            data++;
        } else if (!memcmp(buf, "ASED", 4)) {
            // End of state block
            break;
        } else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cerr << "HA4 Warning: Unknown AS tag: " << tag << "\n";
        }
    }

    return data;
}

// Load HA4 attack data (ATST section)
// HA4 uses ATHV/ATGV instead of HA6's ATV2
unsigned int *fd_frame_AT_load_ha4(unsigned int *data, const unsigned int *data_end, Frame_AT *AT)
{
    AT->correction = 100;

    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "ATGD", 4)) {
            // Guard flags
            AT->guard_flags = data[0];
            ++data;
        } else if (!memcmp(buf, "ATHS", 4)) {
            // Correction
            AT->correction = data[0];
            ++data;
        } else if (!memcmp(buf, "ATVV", 4)) {
            // Damage values
            short *d = (short *)data;
            AT->red_damage = d[0];
            AT->damage = d[1];
            AT->guard_damage = d[2];
            AT->meter_gain = d[3];
            data += 2;
        } else if (!memcmp(buf, "ATHT", 4)) {
            // Correction type
            AT->correction_type = data[0];
            data += 1;
        } else if (!memcmp(buf, "ATGV", 4)) {
            // Guard vector (HA4 format)
            for (int i = 0; i < data[0] && i < 3; i++) {
                AT->guardVector[i] = data[i+1] & 0xFF;
                AT->gVFlags[i] = data[i+1] >> 8;
            }
            data += data[0]+1;
        } else if (!memcmp(buf, "ATHV", 4)) {
            // Hit vector (HA4 format)
            for (int i = 0; i < data[0] && i < 3; i++) {
                AT->hitVector[i] = data[i+1] & 0xFF;
                AT->hVFlags[i] = data[i+1] >> 8;
            }
            data += data[0]+1;
        } else if (!memcmp(buf, "ATF1", 4)) {
            // Other flags
            AT->otherFlags = data[0];
            ++data;
        } else if (!memcmp(buf, "ATHE", 4)) {
            // Hit effect
            AT->hitEffect = data[0];
            AT->soundEffect = data[1];
            data += 2;
        } else if (!memcmp(buf, "ATKK", 4)) {
            // Added effect
            AT->addedEffect = data[0];
            data++;
        } else if (!memcmp(buf, "ATNG", 4)) {
            // Hit grab
            AT->hitgrab = data[0];
            data++;
        } else if (!memcmp(buf, "ATUH", 4)) {
            // Extra gravity
            AT->extraGravity = ((float*)data)[0];
            data++;
        } else if (!memcmp(buf, "ATBT", 4)) {
            // Break time
            AT->breakTime = data[0];
            data++;
        } else if (!memcmp(buf, "ATSN", 4)) {
            // Hit stop time
            AT->hitStopTime = data[0];
            data++;
        } else if (!memcmp(buf, "ATSU", 4)) {
            // Untech time
            AT->untechTime = data[0];
            data++;
        } else if (!memcmp(buf, "ATSP", 4)) {
            // Hit stop
            AT->hitStop = data[0];
            data++;
        } else if (!memcmp(buf, "ATGN", 4)) {
            // Block stop time
            AT->blockStopTime = data[0];
            data++;
        } else if (!memcmp(buf, "ATED", 4)) {
            // End of attack block
            break;
        } else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cerr << "HA4 Warning: Unknown AT tag: " << tag << "\n";
        }
    }

    return data;
}

// Load HA4 frame data
// HA4 uses same tag structure as HA6: AFST, ASST, ATST, EFST, IFST
unsigned int *fd_frame_load_ha4(unsigned int *data, const unsigned int *data_end, Frame *frame, TempInfo *info)
{
    int boxesCount = 0;

    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "HRNM", 4) || !memcmp(buf, "HRAT", 4)) {
            // Hitbox/attackbox (same format as HA6)
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
            data += 5;
        } else if (!memcmp(buf, "HRNS", 4) || !memcmp(buf, "HRAS", 4)) {
            // Hitbox reference
            unsigned int location = data[0];
            unsigned int source = data[1];
            boxesCount++;

            if (!memcmp(buf, "HRAS", 4)) {
                location += 25;
            }

            if (location <= 32) {
                info->delayLoadList.push_back({info->cur_frame, location, source});
            }
            data += 2;
        } else if (!memcmp(buf, "ATST", 4)) {
            // Attack data
            data = fd_frame_AT_load_ha4(data, data_end, &frame->AT);
        } else if (!memcmp(buf, "ASST", 4)) {
            // State data
            if (info->cur_AS < info->AS.size()) {
                info->AS[info->cur_AS] = &frame->AS;
                ++info->cur_AS;
                data = fd_frame_AS_load_ha4(data, data_end, &frame->AS);
            }
        } else if (!memcmp(buf, "ASSM", 4)) {
            // State reference
            unsigned int value = data[0];
            ++data;
            if (value < info->cur_AS) {
                frame->AS = *info->AS[value];
            }
        } else if (!memcmp(buf, "AFST", 4)) {
            // Animation data
            data = fd_frame_AF_load_ha4(data, data_end, frame);
        } else if (!memcmp(buf, "EFST", 4)) {
            // Effect data (HA4 uses different format)
            // HA4: 52-byte binary structure
            // HA6: tag-based (EFTP, EFNO, EFPR, EFED)

            // Skip EFST index
            ++data;

            // Check if this is tag-based (HA6-style) or binary (HA4-style)
            unsigned int *peek = data;
            if (!memcmp(peek, "EFTP", 4)) {
                // Tag-based format (shouldn't happen in pure HA4 files)
                frame->EF.push_back({});
                data = fd_frame_EF_load(data, data_end, &frame->EF.back());
            } else {
                // Binary format (HA4 native)
                frame->EF.push_back({});
                data = fd_frame_EF_load_ha4(data, data_end, &frame->EF.back());
            }
        } else if (!memcmp(buf, "IFST", 4)) {
            // Condition data (HA4 uses different format)
            // HA4: 52-byte binary structure
            // HA6: tag-based (IFTP, IFPR, IFED)

            // Skip IFST index
            ++data;

            // Check if this is tag-based (HA6-style) or binary (HA4-style)
            unsigned int *peek = data;
            if (!memcmp(peek, "IFTP", 4)) {
                // Tag-based format (shouldn't happen in pure HA4 files)
                frame->IF.push_back({});
                data = fd_frame_IF_load(data, data_end, &frame->IF.back());
            } else {
                // Binary format (HA4 native)
                frame->IF.push_back({});
                data = fd_frame_IF_load_ha4(data, data_end, &frame->IF.back());
            }
        } else if (!memcmp(buf, "FSNA", 4)) {
            // Max attack box index + 1
            ++data;
        } else if (!memcmp(buf, "FSNH", 4)) {
            // Max hantei box index + 1
            ++data;
        } else if (!memcmp(buf, "FSNE", 4)) {
            // Max effect index + 1
            ++data;
        } else if (!memcmp(buf, "FSNI", 4)) {
            // Max IF index + 1
            ++data;
        } else if (!memcmp(buf, "FEND", 4)) {
            // End of frame
            break;
        } else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cerr << "HA4 Warning: Unknown frame tag: " << tag << "\n";
        }
    }

    return data;
}

// Load HA4 sequence data
// HA4 uses same sequence structure as HA6: PTCN, PTT2, PSTS, PLVL, etc.
unsigned int *fd_sequence_load_ha4(unsigned int *data, const unsigned int *data_end, Sequence *seq)
{
    TempInfo temp_info;
    unsigned int frame_it = 0, nframes = 0;

    temp_info.seq = seq;
    temp_info.cur_hitbox = 0;
    temp_info.cur_AS = 0;

    std::string name, codename;
    int level = 0, psts = 0, flag = 0;

    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "PTCN", 4)) {
            // Code name
            unsigned int len = data[0];
            data += 1;

            if (len < 64) {
                char str[65];
                memcpy(str, data, len);
                str[len] = '\0';
                codename = str;
            }

            data = (unsigned int *)(((unsigned char *)data)+len);
        } else if (!memcmp(buf, "PSTS", 4)) {
            // Pattern status
            psts = *data;
            ++data;
        } else if (!memcmp(buf, "PLVL", 4)) {
            // Pattern level
            level = *data;
            ++data;
        } else if (!memcmp(buf, "PFLG", 4)) {
            // Pattern flag
            flag = *data;
            ++data;
        } else if (!memcmp(buf, "PTT2", 4)) {
            // Sequence title (Shift-JIS)
            unsigned int len = data[0];
            if (len < 64) {
                char str[65];
                memcpy(str, data+1, len);
                str[len] = '\0';
                // Convert Shift-JIS to UTF-8
                name = sj2utf8(str);
            }
            data = (unsigned int *)(((unsigned char *)data)+len)+1;
        } else if (!memcmp(buf, "PDS2", 4)) {
            // Allocation block
            if (data[0] == 32) {
                seq->frames.clear();
                seq->frames.resize(data[1]);

                temp_info.boxesRefs.resize(data[2]);
                temp_info.AS.resize(data[7]);

                nframes = data[1];
                seq->initialized = 1;

                seq->name = name;
                seq->codeName = codename;
                seq->psts = psts;
                seq->level = level;
                seq->flag = flag;
            }
            data += 1 + (data[0]/4);
        } else if (!memcmp(buf, "FSTR", 4)) {
            // Frame start
            if (seq->initialized && frame_it < nframes) {
                Frame *frame = &seq->frames[frame_it];
                temp_info.cur_frame = frame_it;
                data = fd_frame_load_ha4(data, data_end, frame, &temp_info);
                ++frame_it;
            }
        } else if (!memcmp(buf, "PEND", 4)) {
            // End of sequence - resolve hitbox references
            for (const auto &delayLoad : temp_info.delayLoadList) {
                Frame &frame = seq->frames[delayLoad.frameNo];
                frame.hitboxes[delayLoad.location] = *temp_info.boxesRefs[delayLoad.source];
            }
            break;
        } else {
            char tag[5]{};
            memcpy(tag, buf, 4);
            std::cerr << "HA4 Warning: Unknown sequence tag: " << tag << "\n";
        }
    }

    if (seq->initialized) {
        assert(frame_it == nframes);
    }

    return data;
}

// Main HA4 loader entry point
// Parses PSTR blocks and loads sequences
unsigned int *fd_main_load_ha4(unsigned int *data, const unsigned int *data_end, std::vector<Sequence> &sequences, unsigned int nsequences)
{
    while (data < data_end) {
        unsigned int *buf = data;
        ++data;

        if (!memcmp(buf, "PSTR", 4)) {
            // Pattern start
            unsigned int seq_id = *data;
            ++data;

            if (memcmp(data, "PEND", 4)) {
                if (seq_id < nsequences) {
                    sequences[seq_id].empty = false;
                    data = fd_sequence_load_ha4(data, data_end, &sequences[seq_id]);
                }
            } else {
                ++data;
            }
        } else if (!memcmp(buf, "_END", 4)) {
            // End of file
            break;
        }
    }

    return data;
}
