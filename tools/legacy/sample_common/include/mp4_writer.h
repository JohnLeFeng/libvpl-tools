/*############################################################################
  # Copyright (C) 2005 Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MP4_WRITER_H__
#define __MP4_WRITER_H__

#include "sample_utils.h"

#include <cstring>
#include <string>
#include <vector>

// Simple MP4 (ISO Base Media File Format) muxer for H.264/H.265/AV1 bitstreams.
// Writes ftyp + mdat (streamed) then appends moov on Close().
// Supports codecs: H.264 (AVC), H.265 (HEVC), AV1.

class CSmplBitstreamMp4Writer : public CSmplBitstreamWriter {
public:
    CSmplBitstreamMp4Writer();
    virtual ~CSmplBitstreamMp4Writer();

    // codecId: MFX_CODEC_AVC, MFX_CODEC_HEVC, or MFX_CODEC_AV1
    // width/height: video dimensions
    // frameRate: frames per second
    mfxStatus InitMp4(const char* strFileName,
                      mfxU32 codecId,
                      mfxU16 width,
                      mfxU16 height,
                      mfxF64 frameRate);

    virtual mfxStatus WriteNextFrame(mfxBitstream* pMfxBitstream,
                                     bool isPrint         = true,
                                     bool isCompleteFrame = true) override;
    virtual void Close() override;

private:
    // Helpers to write big-endian values
    void WriteBE16(mfxU8* p, mfxU16 v);
    void WriteBE32(mfxU8* p, mfxU32 v);
    void WriteBE64(mfxU8* p, mfxU64 v);

    void WriteU8(mfxU8 v);
    void Write16(mfxU16 v);
    void Write32(mfxU32 v);
    void Write64(mfxU64 v);
    void WriteBytes(const mfxU8* data, size_t len);
    void WriteAtom(const char* type, const std::vector<mfxU8>& payload);

    // Build the moov atom
    std::vector<mfxU8> BuildMoov();
    std::vector<mfxU8> BuildTrak();
    std::vector<mfxU8> BuildStbl();

    // Codec-specific: extract SPS/PPS (AVC), VPS/SPS/PPS (HEVC), or OBU sequence header (AV1)
    void ParseAvcParameterSets(const mfxU8* data, mfxU32 len);
    void ParseHevcParameterSets(const mfxU8* data, mfxU32 len);
    void ParseAv1SequenceHeader(const mfxU8* data, mfxU32 len);

    // Build codec config record for sample description box
    std::vector<mfxU8> BuildAvcDecoderConfigRecord();
    std::vector<mfxU8> BuildHevcDecoderConfigRecord();
    std::vector<mfxU8> BuildAv1CodecConfigRecord();

    // Convert Annex-B to length-prefixed NALUs
    std::vector<mfxU8> AnnexBToMp4(const mfxU8* data, mfxU32 len);

    // Find NAL units in Annex-B stream
    struct NalUnit {
        const mfxU8* data;
        mfxU32 size;
        mfxU8 type; // NAL unit type
    };
    std::vector<NalUnit> FindNalUnits(const mfxU8* data, mfxU32 len);

    // Append bytes to a vector
    void VecWrite32(std::vector<mfxU8>& v, mfxU32 val);
    void VecWrite16(std::vector<mfxU8>& v, mfxU16 val);
    void VecWrite64(std::vector<mfxU8>& v, mfxU64 val);
    void VecWriteBytes(std::vector<mfxU8>& v, const mfxU8* data, size_t len);
    void VecWriteAtom(std::vector<mfxU8>& v,
                      const char* type,
                      const std::vector<mfxU8>& payload);

    FILE* m_fMp4;
    mfxU32 m_codecId;
    mfxU16 m_width;
    mfxU16 m_height;
    mfxF64 m_frameRate;
    mfxU32 m_timescale;

    // Track sample (frame) sizes for stbl
    std::vector<mfxU32> m_sampleSizes;
    // Track sync (keyframe) sample numbers (1-based)
    std::vector<mfxU32> m_syncSamples;

    // Offset where mdat payload starts
    mfxU64 m_mdatPayloadOffset;
    mfxU64 m_mdatSize; // total mdat payload bytes written

    // Codec parameter sets
    std::vector<std::vector<mfxU8>> m_sps;
    std::vector<std::vector<mfxU8>> m_pps;
    std::vector<std::vector<mfxU8>> m_vps; // HEVC only
    std::vector<mfxU8> m_av1SeqHeader;     // AV1 sequence header OBU

    bool m_bParametersParsed;
    bool m_bMp4Inited;
    std::string m_sFileName;
};

#endif // __MP4_WRITER_H__
