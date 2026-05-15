/*############################################################################
  # Copyright (C) 2005 Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#include "mp4_writer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "sample_defs.h"
#include "vpl/mfxstructures.h"

CSmplBitstreamMp4Writer::CSmplBitstreamMp4Writer()
        : m_fMp4(NULL),
          m_codecId(0),
          m_width(0),
          m_height(0),
          m_frameRate(30.0),
          m_timescale(90000),
          m_mdatPayloadOffset(0),
          m_mdatSize(0),
          m_bParametersParsed(false),
          m_bMp4Inited(false) {}

CSmplBitstreamMp4Writer::~CSmplBitstreamMp4Writer() {
    Close();
}

// --- Big-endian write helpers (to file) ---
void CSmplBitstreamMp4Writer::WriteBE16(mfxU8* p, mfxU16 v) {
    p[0] = (mfxU8)(v >> 8);
    p[1] = (mfxU8)(v);
}
void CSmplBitstreamMp4Writer::WriteBE32(mfxU8* p, mfxU32 v) {
    p[0] = (mfxU8)(v >> 24);
    p[1] = (mfxU8)(v >> 16);
    p[2] = (mfxU8)(v >> 8);
    p[3] = (mfxU8)(v);
}
void CSmplBitstreamMp4Writer::WriteBE64(mfxU8* p, mfxU64 v) {
    WriteBE32(p, (mfxU32)(v >> 32));
    WriteBE32(p + 4, (mfxU32)(v));
}

void CSmplBitstreamMp4Writer::WriteU8(mfxU8 v) {
    fwrite(&v, 1, 1, m_fMp4);
}
void CSmplBitstreamMp4Writer::Write16(mfxU16 v) {
    mfxU8 buf[2];
    WriteBE16(buf, v);
    fwrite(buf, 1, 2, m_fMp4);
}
void CSmplBitstreamMp4Writer::Write32(mfxU32 v) {
    mfxU8 buf[4];
    WriteBE32(buf, v);
    fwrite(buf, 1, 4, m_fMp4);
}
void CSmplBitstreamMp4Writer::Write64(mfxU64 v) {
    mfxU8 buf[8];
    WriteBE64(buf, v);
    fwrite(buf, 1, 8, m_fMp4);
}
void CSmplBitstreamMp4Writer::WriteBytes(const mfxU8* data, size_t len) {
    fwrite(data, 1, len, m_fMp4);
}

// --- Vector helpers ---
void CSmplBitstreamMp4Writer::VecWrite32(std::vector<mfxU8>& v, mfxU32 val) {
    mfxU8 buf[4];
    WriteBE32(buf, val);
    v.insert(v.end(), buf, buf + 4);
}
void CSmplBitstreamMp4Writer::VecWrite16(std::vector<mfxU8>& v, mfxU16 val) {
    mfxU8 buf[2];
    WriteBE16(buf, val);
    v.insert(v.end(), buf, buf + 2);
}
void CSmplBitstreamMp4Writer::VecWrite64(std::vector<mfxU8>& v, mfxU64 val) {
    mfxU8 buf[8];
    WriteBE64(buf, val);
    v.insert(v.end(), buf, buf + 8);
}
void CSmplBitstreamMp4Writer::VecWriteBytes(std::vector<mfxU8>& v,
                                            const mfxU8* data,
                                            size_t len) {
    v.insert(v.end(), data, data + len);
}
void CSmplBitstreamMp4Writer::VecWriteAtom(std::vector<mfxU8>& v,
                                           const char* type,
                                           const std::vector<mfxU8>& payload) {
    mfxU32 atomSize = (mfxU32)(8 + payload.size());
    VecWrite32(v, atomSize);
    VecWriteBytes(v, (const mfxU8*)type, 4);
    VecWriteBytes(v, payload.data(), payload.size());
}

mfxStatus CSmplBitstreamMp4Writer::InitMp4(const char* strFileName,
                                            mfxU32 codecId,
                                            mfxU16 width,
                                            mfxU16 height,
                                            mfxF64 frameRate) {
    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    if (codecId != MFX_CODEC_AVC && codecId != MFX_CODEC_HEVC && codecId != MFX_CODEC_AV1) {
        printf("ERROR: MP4 output is only supported for H.264, H.265, and AV1 codecs.\n");
        return MFX_ERR_UNSUPPORTED;
    }

    m_codecId   = codecId;
    m_width     = width;
    m_height    = height;
    m_frameRate = (frameRate > 0) ? frameRate : 30.0;
    m_timescale = 90000;

    MSDK_FOPEN(m_fMp4, strFileName, "wb+");
    MSDK_CHECK_POINTER(m_fMp4, MFX_ERR_NULL_PTR);

    m_sFileName = strFileName;

    // Write ftyp box
    {
        // ftyp: major_brand=isom, minor_version=512, compatible=[isom, iso2, avc1/hev1/av01, mp41]
        const char* codecBrand = "isom";
        if (m_codecId == MFX_CODEC_AVC)
            codecBrand = "avc1";
        else if (m_codecId == MFX_CODEC_HEVC)
            codecBrand = "hev1";
        else if (m_codecId == MFX_CODEC_AV1)
            codecBrand = "av01";

        mfxU32 ftypSize = 8 + 8 + 4 * 4; // header + major+minor + 4 compatible brands
        Write32(ftypSize);
        WriteBytes((const mfxU8*)"ftyp", 4);
        WriteBytes((const mfxU8*)"isom", 4); // major brand
        Write32(0x200);                       // minor version = 512
        WriteBytes((const mfxU8*)"isom", 4);
        WriteBytes((const mfxU8*)"iso2", 4);
        WriteBytes((const mfxU8*)codecBrand, 4);
        WriteBytes((const mfxU8*)"mp41", 4);
    }

    // Write mdat header with 64-bit extended size (we'll fill actual size on Close)
    // Format: [4 bytes: 1][4 bytes: "mdat"][8 bytes: actual_size_64bit]
    // Using wide-box format: size=1 means extended size follows
    Write32(1);                                 // size=1 -> extended size
    WriteBytes((const mfxU8*)"mdat", 4);        // box type
    Write64(0);                                 // placeholder for 64-bit size (filled on Close)
    m_mdatPayloadOffset = (mfxU64)ftell(m_fMp4); // mdat payload starts here
    m_mdatSize          = 0;

    m_bMp4Inited       = true;
    m_bParametersParsed = false;
    m_nProcessedFramesNum = 0;

    return MFX_ERR_NONE;
}

// --- NAL unit finder for Annex-B streams ---
std::vector<CSmplBitstreamMp4Writer::NalUnit> CSmplBitstreamMp4Writer::FindNalUnits(
    const mfxU8* data,
    mfxU32 len) {
    std::vector<NalUnit> nalus;
    mfxU32 i = 0;

    while (i < len) {
        // Find start code: 00 00 01 or 00 00 00 01
        if (i + 2 < len && data[i] == 0 && data[i + 1] == 0) {
            mfxU32 scLen = 0;
            if (data[i + 2] == 1) {
                scLen = 3;
            }
            else if (i + 3 < len && data[i + 2] == 0 && data[i + 3] == 1) {
                scLen = 4;
            }
            if (scLen > 0) {
                mfxU32 naluStart = i + scLen;
                // Find next start code
                mfxU32 naluEnd = naluStart;
                while (naluEnd + 2 < len) {
                    if (data[naluEnd] == 0 && data[naluEnd + 1] == 0 &&
                        (data[naluEnd + 2] == 1 ||
                         (naluEnd + 3 < len && data[naluEnd + 2] == 0 &&
                          data[naluEnd + 3] == 1))) {
                        break;
                    }
                    naluEnd++;
                }
                if (naluEnd == naluStart) {
                    naluEnd = len;
                }
                // Remove trailing zeros before next start code
                while (naluEnd > naluStart && data[naluEnd - 1] == 0)
                    naluEnd--;

                NalUnit nalu;
                nalu.data = data + naluStart;
                nalu.size = naluEnd - naluStart;
                if (nalu.size > 0) {
                    if (m_codecId == MFX_CODEC_AVC) {
                        nalu.type = nalu.data[0] & 0x1F;
                    }
                    else if (m_codecId == MFX_CODEC_HEVC) {
                        nalu.type = (nalu.data[0] >> 1) & 0x3F;
                    }
                    else {
                        nalu.type = 0;
                    }
                    nalus.push_back(nalu);
                }
                i = naluEnd;
                continue;
            }
        }
        i++;
    }
    return nalus;
}

// Convert Annex-B to MP4 length-prefixed format (4-byte length prefix)
std::vector<mfxU8> CSmplBitstreamMp4Writer::AnnexBToMp4(const mfxU8* data, mfxU32 len) {
    std::vector<mfxU8> result;
    auto nalus = FindNalUnits(data, len);

    for (auto& nalu : nalus) {
        // Skip parameter set NALUs (they go in the sample description box)
        bool isParamSet = false;
        if (m_codecId == MFX_CODEC_AVC) {
            // SPS=7, PPS=8
            isParamSet = (nalu.type == 7 || nalu.type == 8);
        }
        else if (m_codecId == MFX_CODEC_HEVC) {
            // VPS=32, SPS=33, PPS=34
            isParamSet = (nalu.type == 32 || nalu.type == 33 || nalu.type == 34);
        }
        if (isParamSet)
            continue;

        // Write 4-byte big-endian length + NALU data
        VecWrite32(result, nalu.size);
        VecWriteBytes(result, nalu.data, nalu.size);
    }
    return result;
}

void CSmplBitstreamMp4Writer::ParseAvcParameterSets(const mfxU8* data, mfxU32 len) {
    auto nalus = FindNalUnits(data, len);
    for (auto& nalu : nalus) {
        if (nalu.type == 7 && m_sps.empty()) { // SPS
            m_sps.push_back(std::vector<mfxU8>(nalu.data, nalu.data + nalu.size));
        }
        else if (nalu.type == 8 && m_pps.empty()) { // PPS
            m_pps.push_back(std::vector<mfxU8>(nalu.data, nalu.data + nalu.size));
        }
    }
}

void CSmplBitstreamMp4Writer::ParseHevcParameterSets(const mfxU8* data, mfxU32 len) {
    auto nalus = FindNalUnits(data, len);
    for (auto& nalu : nalus) {
        if (nalu.type == 32 && m_vps.empty()) { // VPS
            m_vps.push_back(std::vector<mfxU8>(nalu.data, nalu.data + nalu.size));
        }
        else if (nalu.type == 33 && m_sps.empty()) { // SPS
            m_sps.push_back(std::vector<mfxU8>(nalu.data, nalu.data + nalu.size));
        }
        else if (nalu.type == 34 && m_pps.empty()) { // PPS
            m_pps.push_back(std::vector<mfxU8>(nalu.data, nalu.data + nalu.size));
        }
    }
}

void CSmplBitstreamMp4Writer::ParseAv1SequenceHeader(const mfxU8* data, mfxU32 len) {
    // For AV1: find sequence header OBU (obu_type = 1)
    // AV1 bitstream is OBU-based, first byte: forbidden(1) | obu_type(4) | obu_extension_flag(1) | obu_has_size_field(1) | reserved(1)
    if (m_av1SeqHeader.empty() && len > 0) {
        // Simple: store entire first frame's data as config if we find seq header OBU
        mfxU32 i = 0;
        while (i < len) {
            mfxU8 obuType = (data[i] >> 3) & 0x0F;
            bool hasExtension = (data[i] >> 2) & 1;
            bool hasSize      = (data[i] >> 1) & 1;
            i++; // skip header byte
            if (hasExtension && i < len)
                i++; // skip extension byte
            mfxU32 obuSize = 0;
            if (hasSize) {
                // LEB128 decoding
                mfxU32 shift = 0;
                while (i < len) {
                    mfxU8 byte = data[i++];
                    obuSize |= ((mfxU32)(byte & 0x7F)) << shift;
                    if (!(byte & 0x80))
                        break;
                    shift += 7;
                }
            }
            else {
                obuSize = len - i;
            }
            if (obuType == 1) { // Sequence Header OBU
                // Store the full OBU including header
                mfxU32 obuStart = i - (hasSize ? 0 : 0); // data starts at i after size
                // We need the full OBU with its header for av1C
                // Reconstruct: go back to find where this OBU header started
                mfxU32 headerStart = i - 1 - (hasExtension ? 1 : 0) - 1;
                // Actually let's just store the payload for av1C configOBUs
                // The av1C box wants the sequence header OBU (full OBU)
                mfxU32 fullObuStart = i - obuSize; // wrong approach
                // Simpler: just save raw bytes from OBU payload position
                // For av1C we need: marker(1) | version(1) | seq_profile(3)|seq_level_idx_0(5) | flags(1) | ... + configOBUs
                // Let's just store the sequence header data payload
                if (obuSize <= len - (i - obuSize)) {
                    m_av1SeqHeader.assign(data + (i - obuSize), data + i);
                }
                break;
            }
            if (hasSize)
                i = i + obuSize - obuSize + obuSize; // advance past OBU data (i is already past the size field, pointing at data)
            else
                break;
        }
        // Fallback: if we didn't find a proper seq header, store first 128 bytes
        if (m_av1SeqHeader.empty() && len > 2) {
            // Store entire first frame for parameter extraction
            m_av1SeqHeader.assign(data, data + std::min(len, (mfxU32)256));
        }
    }
}

mfxStatus CSmplBitstreamMp4Writer::WriteNextFrame(mfxBitstream* pMfxBitstream,
                                                   bool isPrint,
                                                   bool isCompleteFrame) {
    if (!m_bMp4Inited)
        return MFX_ERR_NOT_INITIALIZED;
    MSDK_CHECK_POINTER(pMfxBitstream, MFX_ERR_NULL_PTR);

    if (!isCompleteFrame || pMfxBitstream->DataLength == 0)
        return MFX_ERR_NONE;

    const mfxU8* data = pMfxBitstream->Data + pMfxBitstream->DataOffset;
    mfxU32 dataLen    = pMfxBitstream->DataLength;

    // Parse parameter sets from first frames
    if (!m_bParametersParsed) {
        if (m_codecId == MFX_CODEC_AVC) {
            ParseAvcParameterSets(data, dataLen);
            if (!m_sps.empty() && !m_pps.empty())
                m_bParametersParsed = true;
        }
        else if (m_codecId == MFX_CODEC_HEVC) {
            ParseHevcParameterSets(data, dataLen);
            if (!m_vps.empty() && !m_sps.empty() && !m_pps.empty())
                m_bParametersParsed = true;
        }
        else if (m_codecId == MFX_CODEC_AV1) {
            ParseAv1SequenceHeader(data, dataLen);
            if (!m_av1SeqHeader.empty())
                m_bParametersParsed = true;
        }
    }

    // Determine if this is a sync (key) frame
    bool isSync = false;
    if (pMfxBitstream->FrameType &
        (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xIDR)) {
        isSync = true;
    }

    // Convert and write frame data
    mfxU32 sampleSize = 0;

    if (m_codecId == MFX_CODEC_AV1) {
        // AV1 in MP4: write raw OBU stream (no conversion needed, already in OBU format from encoder)
        // But skip sequence header OBU from samples (it goes in av1C)
        fwrite(data, 1, dataLen, m_fMp4);
        sampleSize = dataLen;
    }
    else {
        // H.264/H.265: convert Annex-B to length-prefixed
        std::vector<mfxU8> mp4Data = AnnexBToMp4(data, dataLen);
        if (!mp4Data.empty()) {
            fwrite(mp4Data.data(), 1, mp4Data.size(), m_fMp4);
            sampleSize = (mfxU32)mp4Data.size();
        }
    }

    if (sampleSize > 0) {
        m_sampleSizes.push_back(sampleSize);
        m_mdatSize += sampleSize;

        m_nProcessedFramesNum++;

        if (isSync) {
            m_syncSamples.push_back(m_nProcessedFramesNum); // 1-based
        }

        if (isPrint && (1 == m_nProcessedFramesNum || (0 == (m_nProcessedFramesNum % 100)))) {
            printf("Frame number: %u\r", (unsigned int)m_nProcessedFramesNum);
        }
    }

    // Mark bitstream as consumed
    pMfxBitstream->DataLength = 0;
    pMfxBitstream->DataOffset = 0;

    return MFX_ERR_NONE;
}

// --- Build AVC Decoder Configuration Record (avcC) ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildAvcDecoderConfigRecord() {
    std::vector<mfxU8> r;

    mfxU8 profileIdc = 0x42; // Baseline profile default
    mfxU8 profileCompat = 0xC0;
    mfxU8 levelIdc = 0x1E; // Level 3.0 default

    if (!m_sps.empty() && m_sps[0].size() >= 4) {
        profileIdc    = m_sps[0][1];
        profileCompat = m_sps[0][2];
        levelIdc      = m_sps[0][3];
    }

    r.push_back(1);            // configurationVersion
    r.push_back(profileIdc);
    r.push_back(profileCompat);
    r.push_back(levelIdc);
    r.push_back(0xFF);         // lengthSizeMinusOne = 3 (4-byte NALU lengths) | reserved 6 bits

    // SPS
    r.push_back((mfxU8)(0xE0 | (m_sps.size() & 0x1F))); // reserved 3 bits | numSPS
    for (auto& sps : m_sps) {
        VecWrite16(r, (mfxU16)sps.size());
        VecWriteBytes(r, sps.data(), sps.size());
    }

    // PPS
    r.push_back((mfxU8)(m_pps.size() & 0xFF));
    for (auto& pps : m_pps) {
        VecWrite16(r, (mfxU16)pps.size());
        VecWriteBytes(r, pps.data(), pps.size());
    }

    return r;
}

// --- Build HEVC Decoder Configuration Record (hvcC) ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildHevcDecoderConfigRecord() {
    std::vector<mfxU8> r;

    // Minimal HEVCDecoderConfigurationRecord
    mfxU8 generalProfileSpace = 0;
    mfxU8 generalTierFlag     = 0;
    mfxU8 generalProfileIdc   = 1; // Main profile
    mfxU8 generalLevelIdc     = 93; // Level 3.1

    if (!m_sps.empty() && m_sps[0].size() >= 12) {
        // Parse profile_tier_level from SPS NAL
        // SPS NAL: nal_header(2 bytes) + sps_video_parameter_set_id(4bits) + sps_max_sub_layers_minus1(3bits) + ...
        // profile_tier_level starts at byte 2
        generalProfileSpace = (m_sps[0][2] >> 6) & 0x3;
        generalTierFlag     = (m_sps[0][2] >> 5) & 0x1;
        generalProfileIdc   = m_sps[0][2] & 0x1F;
        if (m_sps[0].size() > 13)
            generalLevelIdc = m_sps[0][12];
    }

    r.push_back(1); // configurationVersion
    r.push_back((mfxU8)((generalProfileSpace << 6) | (generalTierFlag << 5) | generalProfileIdc));
    // general_profile_compatibility_flags (4 bytes)
    VecWrite32(r, 0x60000000);
    // general_constraint_indicator_flags (6 bytes)
    for (int i = 0; i < 6; i++)
        r.push_back(0);
    r.push_back(generalLevelIdc);
    VecWrite16(r, 0xF000); // min_spatial_segmentation_idc with reserved
    r.push_back(0xFC);      // parallelismType with reserved
    r.push_back(0xFC | 1);  // chromaFormat = 1 (4:2:0) with reserved
    r.push_back(0xF8);      // bitDepthLumaMinus8 with reserved
    r.push_back(0xF8);      // bitDepthChromaMinus8 with reserved
    VecWrite16(r, 0);       // avgFrameRate
    r.push_back(0x0F);      // constantFrameRate(2) | numTemporalLayers(3) | temporalIdNested(1) | lengthSizeMinusOne(2) = 3

    // numOfArrays
    mfxU8 numArrays = 0;
    if (!m_vps.empty())
        numArrays++;
    if (!m_sps.empty())
        numArrays++;
    if (!m_pps.empty())
        numArrays++;
    r.push_back(numArrays);

    // VPS array
    if (!m_vps.empty()) {
        r.push_back(0x20 | 32); // array_completeness=1 | nal_unit_type=32 (VPS)
        VecWrite16(r, (mfxU16)m_vps.size());
        for (auto& vps : m_vps) {
            VecWrite16(r, (mfxU16)vps.size());
            VecWriteBytes(r, vps.data(), vps.size());
        }
    }

    // SPS array
    if (!m_sps.empty()) {
        r.push_back(0x20 | 33); // array_completeness=1 | nal_unit_type=33 (SPS)
        VecWrite16(r, (mfxU16)m_sps.size());
        for (auto& sps : m_sps) {
            VecWrite16(r, (mfxU16)sps.size());
            VecWriteBytes(r, sps.data(), sps.size());
        }
    }

    // PPS array
    if (!m_pps.empty()) {
        r.push_back(0x20 | 34); // array_completeness=1 | nal_unit_type=34 (PPS)
        VecWrite16(r, (mfxU16)m_pps.size());
        for (auto& pps : m_pps) {
            VecWrite16(r, (mfxU16)pps.size());
            VecWriteBytes(r, pps.data(), pps.size());
        }
    }

    return r;
}

// --- Build AV1 Codec Configuration Record (av1C) ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildAv1CodecConfigRecord() {
    std::vector<mfxU8> r;

    // Minimal av1C: marker(1)|version(1) = 0x81
    r.push_back(0x81); // marker=1, version=1

    // seq_profile(3) | seq_level_idx_0(5)
    mfxU8 seqProfile    = 0;
    mfxU8 seqLevelIdx   = 8; // Level 4.0
    mfxU8 seqTier       = 0;
    mfxU8 highBitdepth  = 0;
    mfxU8 twelveBit     = 0;
    mfxU8 monochrome    = 0;
    mfxU8 subsamplingX  = 1;
    mfxU8 subsamplingY  = 1;
    mfxU8 chromaSample  = 0;

    r.push_back((mfxU8)((seqProfile << 5) | seqLevelIdx));
    r.push_back((mfxU8)((seqTier << 7) | (highBitdepth << 6) | (twelveBit << 5) |
                         (monochrome << 4) | (subsamplingX << 3) | (subsamplingY << 2) |
                         chromaSample));
    r.push_back(0); // initial_presentation_delay (not present)

    // Append sequence header OBU as configOBUs
    if (!m_av1SeqHeader.empty()) {
        VecWriteBytes(r, m_av1SeqHeader.data(), m_av1SeqHeader.size());
    }

    return r;
}

// --- Build sample table (stbl) ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildStbl() {
    std::vector<mfxU8> stbl;

    // stsd (sample description)
    {
        std::vector<mfxU8> stsd;
        // Full box: version=0, flags=0
        VecWrite32(stsd, 0); // version + flags
        VecWrite32(stsd, 1); // entry_count

        // Sample entry
        std::vector<mfxU8> sampleEntry;
        // reserved (6 bytes)
        for (int i = 0; i < 6; i++)
            sampleEntry.push_back(0);
        VecWrite16(sampleEntry, 1); // data_reference_index

        // Visual sample entry fields
        VecWrite16(sampleEntry, 0); // pre_defined
        VecWrite16(sampleEntry, 0); // reserved
        VecWrite32(sampleEntry, 0); // pre_defined[0]
        VecWrite32(sampleEntry, 0); // pre_defined[1]
        VecWrite32(sampleEntry, 0); // pre_defined[2]
        VecWrite16(sampleEntry, m_width);
        VecWrite16(sampleEntry, m_height);
        VecWrite32(sampleEntry, 0x00480000); // horizresolution = 72 dpi
        VecWrite32(sampleEntry, 0x00480000); // vertresolution = 72 dpi
        VecWrite32(sampleEntry, 0);          // reserved
        VecWrite16(sampleEntry, 1);          // frame_count
        // compressorname (32 bytes)
        for (int i = 0; i < 32; i++)
            sampleEntry.push_back(0);
        VecWrite16(sampleEntry, 0x0018); // depth = 24
        VecWrite16(sampleEntry, 0xFFFF); // pre_defined = -1

        // Codec-specific box inside sample entry
        if (m_codecId == MFX_CODEC_AVC) {
            auto avcC = BuildAvcDecoderConfigRecord();
            VecWriteAtom(sampleEntry, "avcC", avcC);
        }
        else if (m_codecId == MFX_CODEC_HEVC) {
            auto hvcC = BuildHevcDecoderConfigRecord();
            VecWriteAtom(sampleEntry, "hvcC", hvcC);
        }
        else if (m_codecId == MFX_CODEC_AV1) {
            auto av1C = BuildAv1CodecConfigRecord();
            VecWriteAtom(sampleEntry, "av1C", av1C);
        }

        // Wrap sample entry in its box
        const char* sampleEntryType = "avc1";
        if (m_codecId == MFX_CODEC_HEVC)
            sampleEntryType = "hev1";
        else if (m_codecId == MFX_CODEC_AV1)
            sampleEntryType = "av01";

        VecWriteAtom(stsd, sampleEntryType, sampleEntry);
        VecWriteAtom(stbl, "stsd", stsd);
    }

    // stts (decoding time to sample) - constant duration
    {
        std::vector<mfxU8> stts;
        VecWrite32(stts, 0); // version + flags
        VecWrite32(stts, 1); // entry_count
        VecWrite32(stts, (mfxU32)m_sampleSizes.size()); // sample_count
        mfxU32 sampleDuration = (mfxU32)(m_timescale / m_frameRate);
        VecWrite32(stts, sampleDuration);
        VecWriteAtom(stbl, "stts", stts);
    }

    // stss (sync sample table) - only if not all frames are sync
    if (!m_syncSamples.empty() && m_syncSamples.size() < m_sampleSizes.size()) {
        std::vector<mfxU8> stss;
        VecWrite32(stss, 0); // version + flags
        VecWrite32(stss, (mfxU32)m_syncSamples.size());
        for (auto s : m_syncSamples)
            VecWrite32(stss, s);
        VecWriteAtom(stbl, "stss", stss);
    }

    // stsz (sample sizes)
    {
        std::vector<mfxU8> stsz;
        VecWrite32(stsz, 0); // version + flags
        VecWrite32(stsz, 0); // sample_size = 0 (variable)
        VecWrite32(stsz, (mfxU32)m_sampleSizes.size());
        for (auto sz : m_sampleSizes)
            VecWrite32(stsz, sz);
        VecWriteAtom(stbl, "stsz", stsz);
    }

    // stsc (sample-to-chunk) - all samples in one chunk
    {
        std::vector<mfxU8> stsc;
        VecWrite32(stsc, 0); // version + flags
        VecWrite32(stsc, 1); // entry_count
        VecWrite32(stsc, 1); // first_chunk
        VecWrite32(stsc, (mfxU32)m_sampleSizes.size()); // samples_per_chunk
        VecWrite32(stsc, 1); // sample_description_index
        VecWriteAtom(stbl, "stsc", stsc);
    }

    // co64 (64-bit chunk offset) - single chunk starting at mdat payload
    {
        std::vector<mfxU8> co64;
        VecWrite32(co64, 0); // version + flags
        VecWrite32(co64, 1); // entry_count
        VecWrite64(co64, m_mdatPayloadOffset);
        VecWriteAtom(stbl, "co64", co64);
    }

    return stbl;
}

// --- Build trak atom ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildTrak() {
    std::vector<mfxU8> trak;

    mfxU32 totalSamples = (mfxU32)m_sampleSizes.size();
    mfxU32 sampleDuration = (mfxU32)(m_timescale / m_frameRate);
    mfxU64 trackDuration = (mfxU64)totalSamples * sampleDuration;

    // tkhd (track header)
    {
        std::vector<mfxU8> tkhd;
        VecWrite32(tkhd, 0x00000003); // version=0, flags=track_enabled|track_in_movie
        VecWrite32(tkhd, 0);          // creation_time
        VecWrite32(tkhd, 0);          // modification_time
        VecWrite32(tkhd, 1);          // track_ID
        VecWrite32(tkhd, 0);          // reserved
        VecWrite32(tkhd, (mfxU32)(trackDuration * 1000 / m_timescale)); // duration in movie timescale (1000)
        VecWrite32(tkhd, 0);          // reserved
        VecWrite32(tkhd, 0);          // reserved
        VecWrite16(tkhd, 0);          // layer
        VecWrite16(tkhd, 0);          // alternate_group
        VecWrite16(tkhd, 0);          // volume (0 for video)
        VecWrite16(tkhd, 0);          // reserved
        // identity matrix (36 bytes)
        VecWrite32(tkhd, 0x00010000);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0x00010000);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0);
        VecWrite32(tkhd, 0x40000000);
        // width, height (16.16 fixed point)
        VecWrite32(tkhd, (mfxU32)m_width << 16);
        VecWrite32(tkhd, (mfxU32)m_height << 16);
        VecWriteAtom(trak, "tkhd", tkhd);
    }

    // mdia (media box)
    {
        std::vector<mfxU8> mdia;

        // mdhd (media header)
        {
            std::vector<mfxU8> mdhd;
            VecWrite32(mdhd, 0); // version=0, flags=0
            VecWrite32(mdhd, 0); // creation_time
            VecWrite32(mdhd, 0); // modification_time
            VecWrite32(mdhd, m_timescale);
            VecWrite32(mdhd, (mfxU32)trackDuration); // duration
            VecWrite16(mdhd, 0x55C4); // language = 'und'
            VecWrite16(mdhd, 0);      // pre_defined
            VecWriteAtom(mdia, "mdhd", mdhd);
        }

        // hdlr (handler)
        {
            std::vector<mfxU8> hdlr;
            VecWrite32(hdlr, 0);               // version + flags
            VecWrite32(hdlr, 0);               // pre_defined
            VecWriteBytes(hdlr, (const mfxU8*)"vide", 4); // handler_type
            VecWrite32(hdlr, 0);               // reserved
            VecWrite32(hdlr, 0);               // reserved
            VecWrite32(hdlr, 0);               // reserved
            // name (null-terminated string)
            const char* name = "VideoHandler";
            VecWriteBytes(hdlr, (const mfxU8*)name, strlen(name) + 1);
            VecWriteAtom(mdia, "hdlr", hdlr);
        }

        // minf (media information)
        {
            std::vector<mfxU8> minf;

            // vmhd (video media header)
            {
                std::vector<mfxU8> vmhd;
                VecWrite32(vmhd, 0x00000001); // version=0, flags=1
                VecWrite16(vmhd, 0);          // graphicsmode
                VecWrite16(vmhd, 0);          // opcolor
                VecWrite16(vmhd, 0);
                VecWrite16(vmhd, 0);
                VecWriteAtom(minf, "vmhd", vmhd);
            }

            // dinf + dref
            {
                std::vector<mfxU8> dinf;
                std::vector<mfxU8> dref;
                VecWrite32(dref, 0); // version + flags
                VecWrite32(dref, 1); // entry_count
                // url entry (self-contained)
                std::vector<mfxU8> url;
                VecWrite32(url, 0x00000001); // version=0, flags=self_contained
                mfxU32 urlSize = (mfxU32)(8 + url.size());
                VecWrite32(dref, urlSize);
                VecWriteBytes(dref, (const mfxU8*)"url ", 4);
                VecWriteBytes(dref, url.data(), url.size());
                VecWriteAtom(dinf, "dref", dref);
                VecWriteAtom(minf, "dinf", dinf);
            }

            // stbl
            {
                auto stblData = BuildStbl();
                VecWriteAtom(minf, "stbl", stblData);
            }

            VecWriteAtom(mdia, "minf", minf);
        }

        VecWriteAtom(trak, "mdia", mdia);
    }

    return trak;
}

// --- Build moov atom ---
std::vector<mfxU8> CSmplBitstreamMp4Writer::BuildMoov() {
    std::vector<mfxU8> moov;

    mfxU32 totalSamples = (mfxU32)m_sampleSizes.size();
    mfxU32 sampleDuration = (mfxU32)(m_timescale / m_frameRate);
    mfxU64 trackDuration = (mfxU64)totalSamples * sampleDuration;
    mfxU32 movieDuration = (mfxU32)(trackDuration * 1000 / m_timescale); // in movie timescale (1000)

    // mvhd (movie header)
    {
        std::vector<mfxU8> mvhd;
        VecWrite32(mvhd, 0);          // version=0, flags=0
        VecWrite32(mvhd, 0);          // creation_time
        VecWrite32(mvhd, 0);          // modification_time
        VecWrite32(mvhd, 1000);       // timescale (movie timescale)
        VecWrite32(mvhd, movieDuration); // duration
        VecWrite32(mvhd, 0x00010000); // rate = 1.0
        VecWrite16(mvhd, 0x0100);     // volume = 1.0
        VecWrite16(mvhd, 0);          // reserved
        VecWrite32(mvhd, 0);          // reserved
        VecWrite32(mvhd, 0);          // reserved
        // identity matrix (36 bytes)
        VecWrite32(mvhd, 0x00010000);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0x00010000);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 0x40000000);
        // pre_defined (24 bytes)
        for (int i = 0; i < 6; i++)
            VecWrite32(mvhd, 0);
        VecWrite32(mvhd, 2); // next_track_ID
        VecWriteAtom(moov, "mvhd", mvhd);
    }

    // trak
    {
        auto trakData = BuildTrak();
        VecWriteAtom(moov, "trak", trakData);
    }

    return moov;
}

void CSmplBitstreamMp4Writer::Close() {
    if (!m_fMp4 || !m_bMp4Inited) {
        if (m_fMp4) {
            fclose(m_fMp4);
            m_fMp4 = NULL;
        }
        return;
    }

    // Fix mdat size: seek to mdat extended-size field and write actual size
    // mdat starts at ftyp_size (40 bytes for our ftyp)
    // Extended box: [4:size=1][4:type][8:extended_size]  total header = 16
    mfxU64 mdatTotalSize = 16 + m_mdatSize; // header + payload
    mfxU64 mdatBoxOffset = m_mdatPayloadOffset - 16; // back to start of mdat box

    // Seek to extended size field (8 bytes after box start)
#if defined(_WIN32) || defined(_WIN64)
    _fseeki64(m_fMp4, (long long)(mdatBoxOffset + 8), SEEK_SET);
#else
    fseeko(m_fMp4, (off_t)(mdatBoxOffset + 8), SEEK_SET);
#endif
    mfxU8 sizeBuf[8];
    WriteBE64(sizeBuf, mdatTotalSize);
    fwrite(sizeBuf, 1, 8, m_fMp4);

    // Seek to end and write moov
#if defined(_WIN32) || defined(_WIN64)
    _fseeki64(m_fMp4, 0, SEEK_END);
#else
    fseeko(m_fMp4, 0, SEEK_END);
#endif

    auto moovData = BuildMoov();
    // Write moov atom
    mfxU32 moovSize = (mfxU32)(8 + moovData.size());
    Write32(moovSize);
    WriteBytes((const mfxU8*)"moov", 4);
    WriteBytes(moovData.data(), moovData.size());

    printf("\nMP4 file written: %s (%u frames)\n", m_sFileName.c_str(), m_nProcessedFramesNum);

    fclose(m_fMp4);
    m_fMp4      = NULL;
    m_bMp4Inited = false;

    m_sampleSizes.clear();
    m_syncSamples.clear();
    m_sps.clear();
    m_pps.clear();
    m_vps.clear();
    m_av1SeqHeader.clear();
}
