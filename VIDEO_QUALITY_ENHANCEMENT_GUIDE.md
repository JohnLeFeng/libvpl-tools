# Intel VPL Video Quality Enhancement Methodologies

This document outlines comprehensive methodologies and best practices to improve video quality in Intel Video Processing Library (VPL) for both encoding and decoding operations.

---

## Table of Contents

1. [Encoding Quality Enhancement](#encoding-quality-enhancement)
2. [Decoding Quality Enhancement](#decoding-quality-enhancement)
3. [VPP (Video Post-Processing) Features](#vpp-video-post-processing-features)
4. [Codec Selection Strategies](#codec-selection-strategies)
5. [Advanced Quality Techniques](#advanced-quality-techniques)
6. [Quality Metrics and Measurement](#quality-metrics-and-measurement)
7. [Quality-First Pipeline Architecture](#quality-first-pipeline-architecture)

---

## Encoding Quality Enhancement

### 1. **Codec Selection for Quality**

#### H.264 vs H.265 (HEVC)

| Aspect | H.264 | H.265 |
|--------|-------|-------|
| **Compression Efficiency** | Standard baseline | 40-50% better ⭐ |
| **Quality at same bitrate** | Good | Excellent ⭐ |
| **Quality at same file size** | Fair | Excellent ⭐ |
| **Hardware Support** | Wider | Growing |
| **Encoding Speed** | Faster | Slower |
| **Best Use** | Compatible streaming | Quality-focused |

**Quality Improvement:** Switching from H.264 to H.265 provides **immediate 40-50% quality improvement** at the same bitrate.

**Recommendation:** Use H.265 for archival, on-demand content, and quality-critical scenarios.

**Example:**
```bash
# 40-50% better quality than H.264 at same bitrate
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 -b 4000
```

---

### 2. **Bitrate Optimization**

**Methodology:** Use quality-tier bitrates that match your content and resolution.

**Quality Tiers by Resolution:**

| Resolution | Low-Fi | Streaming | Good | High Quality | Archival |
|-----------|---------|-----------|------|-------------|----------|
| 480p | 500 | 1000 | 1500 | 2500 | 3500 |
| 720p | 1000 | 2000 | 3500 | 5000 | 7000 |
| 1080p | 1500 | 3000 | 5000 | 8000 | 12000 |
| 2K | 2500 | 5000 | 8000 | 12000 | 18000 |
| 4K | 5000 | 10000 | 15000 | 25000 | 40000 |

**Quality Characteristics:**

- **Low-Fi (40%):** Visible compression artifacts, acceptable for previews
- **Streaming (50-60%):** Good quality for streaming, some artifacts in complex scenes
- **Good (70-80%):** Very good quality, minimal visible artifacts
- **High Quality (85-95%):** Excellent quality, nearly transparent compression
- **Archival (95%+):** Near-lossless, suitable for storage and re-encoding

**Recommendation:** Start at "Good" or "High Quality" tier and adjust based on content type and viewer feedback.

**Example:**
```bash
# For 1080p archival quality (H.265)
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 -b 8000
```

---

### 3. **Rate Control Methods**

**Constant Bit Rate (CBR)** - Default
- **Quality:** Consistent, predictable file size
- **Best for:** Streaming with fixed bandwidth

```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -b 5000
```

**Lookahead Bitrate Control (LA-BRC)** - Advanced ⭐ BEST FOR QUALITY
- **Quality:** Better quality consistency, handles scene changes better
- **Improvement:** 10-20% better visual quality than CBR
- **Overhead:** Higher computation (20-30% slower encoding)
- **Requirements:** 4th Gen Intel Core and newer

```bash
# 10-20% better quality than CBR
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -la -lad 50
```

**Key LA-BRC Parameters:**
- `-la` → Enable Lookahead BRC
- `-lad <depth>` → Analysis depth (1-100 frames)
  - LAD=0: Auto-select (default)
  - LAD=20-30: Good balance of quality and speed
  - LAD=50-100: Maximum quality (slowest)

**Quality Comparison:**
```
CBR Only:           ████████░░ Quality
LA-BRC (LAD=30):    █████████░ Quality (+10%)
LA-BRC (LAD=50):    ██████████ Quality (+15-20%)
```

---

### 4. **Motion Estimation and Reference Frames**

**Multiple Reference Frames Strategy**

| Refs | Encoding Speed | Compression | Quality | Best For |
|------|----------------|-------------|---------|----------|
| 1 | Fastest | Lowest | Fair | Real-time |
| 2 | Fast | Low | Good | Streaming |
| 3 | Moderate | Moderate | Very Good | Balanced |
| 4 | Slow | High | Excellent | Archival |
| 5+ | Slowest | Highest | Best ⭐ | Quality-first |

**Quality Improvement Formula:**
- 1→3 refs: ~12-15% better compression
- 3→5 refs: ~8-12% better compression
- Total improvement (1→5): ~20-25% better compression at same visual quality

**Recommendation:** Use 4-5 references for quality-focused encoding.

**Example:**
```bash
# Maximum quality with 5 reference frames
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -x 5
```

---

### 5. **B-Frame Structure and Pyramid References**

**B-Pyramid Benefits:**
- **Quality Improvement:** 5-15% better compression
- **Mechanism:** B-frames can be used as references (not just P-frames)
- **Scene Handling:** Better adaptation to scene changes

**Quality Comparison:**
```
No B-frames (r=1):        ████████░░ Compression
Standard B-frames (r=3):  ████████░░ Compression
B-Pyramid (r=3 -bref):    █████████░ Compression (+8-12%)
```

**Example:**
```bash
# Enable B-pyramid for better quality
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -r 3 -bref
```

---

### 6. **Target Usage for Quality**

**Quality Optimization by Preset:**

| Preset | Encoding Speed | Compression | Quality | Encoding Time |
|--------|----------------|-------------|---------|----------------|
| veryfast | Slowest | Lowest | Fair | 1x |
| faster | Fast | Low | Good | 1.5-2x |
| fast | Moderate | Moderate | Very Good | 2-3x |
| medium | Balanced | Balanced | Excellent | 3-4x |
| slow | Slow | High | Excellent ⭐ | 5-8x |
| slower | Very Slow | Higher | Excellent ⭐ | 8-15x |
| veryslow | Slowest | Highest | Best ⭐ | 15-20x |

**Quality Improvement:**
- veryslow vs veryfast: 15-20% better compression efficiency
- veryslow vs fast: 12-15% better compression efficiency

**Recommendation:** Use `slow` or `slower` for quality-critical content.

**Example:**
```bash
# Maximum quality encoding (veryslow preset)
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -u veryslow
```

---

### 7. **GOP Structure Optimization**

**GOP (Group of Pictures) Strategy:**

| GOP Size | Compression | Random Access | Quality |
|----------|-------------|----------------|---------|
| 32 | Baseline | Excellent | Good |
| 64 | +2-3% | Excellent | Good |
| 128 | +3-5% | Good | Very Good |
| 256 | +5-8% | Fair | Excellent ⭐ |
| 512 | +8-12% | Poor | Best ⭐ |

**IDR Interval Strategy** (allows non-IDR I-frames):
- IDR=0: Every I-frame is IDR (maximum error resilience)
- IDR=2-4: Some I-frames can reference previous frames (+3-8% better compression)

**Recommendation:** For quality-focused scenarios, use GOP=256-512 and IDR=2-4.

**Example:**
```bash
# Optimized GOP for quality (larger GOP + non-IDR I-frames)
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -g 512 -idr_interval 2
```

---

### 8. **Preprocessing with VPP Before Encoding**

**VPP Denoise Preprocessing:**

Intel VPL can apply denoising before encoding, which improves compression efficiency by reducing noise entropy.

**Denoising Benefits:**
- Reduces encoder bit allocation to noise (saves 5-15% bitrate)
- Improves visual quality consistency
- Better detail preservation after compression

**Pipeline:**
```
Raw Input → VPP Denoise → Encoder → Encoded Output
```

**Quality Improvement:** 8-15% better compression with minimal visual difference.

**Configuration (programmatic):**
```c
mfxExtVPPDenoise denoise;
denoise.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
denoise.Header.BufferSz = sizeof(denoise);
denoise.DenoiseFactor = 16;  // 0-100, higher = more denoising
```

---

### 9. **Detail Enhancement**

**VPP Detail Enhancement (Sharpening):**

- **Purpose:** Enhance fine details and edges before encoding
- **Quality Impact:** Better detail preservation after compression
- **Improvement:** 5-10% perceived quality increase
- **Risk:** Over-enhancement can create artifacts

**Configuration (programmatic):**
```c
mfxExtVPPDetail detail;
detail.Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
detail.DetailFactor = 25;  // 0-64, higher = more enhancement
```

---

### 10. **Scene Detection and Adaptive Quantization**

**VPP Scene Analysis (Built-in):**
- Automatically detects scene cuts and transitions
- Helps encoder adapt quantization to scene complexity
- Improves quality consistency across transitions

**Quality Benefit:** Better handling of scene changes, reduced artifacts at cuts.

**Status:** Enabled by default in VPP pipeline.

---

## Decoding Quality Enhancement

### 1. **Output Format Selection**

**Color Precision Strategy:**

| Format | Bit Depth | Use Case | Quality |
|--------|-----------|----------|---------|
| NV12 | 8-bit | Real-time display | Standard |
| I420 | 8-bit | File storage | Standard |
| RGB4 | 8-bit | Color conversion | Fair |
| RGB4_FCR | 8-bit | Full color range | Good ⭐ |
| P010 | 10-bit | HDR, editing | Excellent ⭐ |
| P016 | 16-bit | Professional editing | Maximum ⭐ |
| Y216 | 16-bit | Professional editing | Maximum ⭐ |
| Y416 | 16-bit RGBA | Professional editing | Maximum ⭐ |

**Quality Gains:**
- 8-bit to 10-bit: Eliminates color banding (subtle but visible)
- 10-bit to 16-bit: Maximum precision for post-processing

**Recommendation:** Use 10-bit (P010) or 16-bit (P016/Y416) for color grading or professional workflows.

**Example:**
```bash
# Maximum quality 16-bit output for professional editing
sample_decode h264 -i encoded.h264 -y416 -async 4
```

---

### 2. **Deinterlacing Strategies**

**For Interlaced Content:**

| Method | Speed | Quality | Artifacts |
|--------|-------|---------|-----------|
| None | Fastest | Poor ⚠️ | High |
| BOB | Fast | Fair | Moderate |
| ADI (Adaptive) | Moderate | Good ⭐ | Minimal ⭐ |

**ADI Deinterlacing Benefits:**
- 20-30% better motion quality than BOB
- Better edge preservation
- Smoother motion in film content

**Example:**
```bash
# Adaptive deinterlacing for best quality
sample_decode mpeg2 -i interlaced.m2v -di adi
```

---

### 3. **VPP Post-Processing for Quality**

**Available VPP Filters:**

| Filter | Purpose | Quality Impact |
|--------|---------|-----------------|
| Denoise | Reduce compression artifacts | +5-10% clarity |
| Detail Enhancement | Sharpen details | +3-8% perceived quality |
| Color Conversion | Accurate color space | Prevents color shift |
| Scaling (quality mode) | High-quality resizing | +20-30% vs lowpower |
| Deinterlacing | Handle interlaced | +15-25% for interlaced |

**VPP Post-Processing Pipeline:**
```
Decoded Output → VPP Denoise → VPP Detail → VPP Scaling → Display/Storage
```

---

### 4. **Scaling Quality Optimization**

**Scaling Modes:**

| Mode | Algorithm | Use Case | Quality |
|------|-----------|----------|---------|
| lowpower | Bilinear | Mobile, embedded | Acceptable |
| quality | Bicubic/Lanczos | Professional | Excellent ⭐ |

**Quality Improvement:** 20-30% better visual quality with `quality` mode.

**Performance Tradeoff:**
- `quality` mode: 15-25% slower than `lowpower`
- Worthwhile for visible content, less critical for small thumbnails

**Example:**
```bash
# High-quality scaling output
sample_decode h264 -i encoded.h264 -scaling_mode quality -async 4
```

---

### 5. **Decoder Post-Processing**

**Decoder-Based Resizing (Faster Alternative):**

Some codecs support resizing directly in the decoder, bypassing VPP.

**Benefits:**
- More efficient pipeline
- Reduces memory bandwidth
- Faster overall throughput

**Usage:**
```bash
# Use decoder post-processing when available
sample_decode h264 -i encoded.h264 -dec_postproc force
```

---

## VPP (Video Post-Processing) Features

### VPP Capabilities in Intel VPL

| Feature | Purpose | Quality Impact | Codec |
|---------|---------|-----------------|-------|
| **Denoise** | Reduce compression artifacts | +5-10% | All |
| **Detail** | Enhance fine details/sharpness | +3-8% | All |
| **Procamp** | Color/brightness adjustment | Varies | All |
| **Scaling** | Resizing with quality control | +20-30% (quality mode) | All |
| **Color Space Conversion** | Format/color space changes | Critical | All |
| **Deinterlacing** | Handle interlaced content | +15-25% | Interlaced |
| **Scene Detection** | Automatic scene analysis | Improves encoding | Encode |

### Enabling/Disabling VPP Features

**By Default (Enabled):**
- Denoise: ON
- Detail Enhancement: ON
- Scene Analysis: ON
- Procamp: ON

**To Disable Specific Features (for performance):**
```c
auto vppParams = mfxParams.AddExtBuffer<mfxExtVPPDoNotUse>();
vppParams->AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;      // Disable denoise
vppParams->AlgList[1] = MFX_EXTBUFF_VPP_DETAIL;       // Disable detail
vppParams->NumAlg = 2;
```

**Recommendation:** Keep denoise and detail enhancement enabled for quality-focused scenarios.

---

## Codec Selection Strategies

### Best Codecs for Quality

**Ranked by Compression Efficiency:**

1. **AV1** (Very High Efficiency) ⭐⭐⭐
   - 30% better than H.265 (but slower encoding)
   - Royalty-free
   - Limited hardware support currently
   - Best for: Future-proof archival

2. **H.265 (HEVC)** (High Efficiency) ⭐⭐
   - 40-50% better than H.264
   - Good hardware support
   - Widely adopted
   - Best for: Quality-critical streaming

3. **H.264 (AVC)** (Baseline) ⭐
   - Industry standard
   - Widest compatibility
   - Less efficient
   - Best for: Maximum compatibility

### Content-Specific Recommendations

**Natural Video (Film/Live Action):**
- Best: H.265 at moderate-high bitrate
- Alternative: H.264 with higher bitrate

**Screen Capture (Screenshare, Presentations):**
- Best: VP9 or H.265 (sharp edges compress well)
- Alternative: H.264

**Animation/Synthetic Content:**
- Best: VP9 or AV1 (uniform regions compress better)
- Alternative: H.265

**High Motion Content:**
- Best: H.265 with higher reference frames
- Settings: `-x 4-5 -la -lad 30`

---

## Advanced Quality Techniques

### 1. **Two-Pass Encoding**

**Concept:** Analyze content in first pass, optimize in second pass.

**Benefits:**
- 5-15% better compression efficiency
- Better frame-level quality decisions
- Consistent quality across video

**Implementation:**
```bash
# First pass: analyze
sample_encode h265 -i input.yuv -o /dev/null -w 1920 -h 1080 \
  -b 5000 -u slow -pass 1

# Second pass: encode with optimizations
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -b 5000 -u slow -pass 2
```

**Drawback:** 2x encoding time. Worth it for one-time archival or high-value content.

---

### 2. **Content-Aware Quality Optimization**

**Strategy: Adjust parameters based on content complexity**

```bash
# Simple content (low motion, static scenes)
sample_encode h265 -i simple.yuv -o output.h265 -w 1920 -h 1080 \
  -b 3000 -u slow -r 2 -x 3

# Complex content (high motion, scene changes)
sample_encode h265 -i complex.yuv -o output.h265 -w 1920 -h 1080 \
  -b 6000 -u veryslow -r 4 -x 5 -la -lad 50
```

**Analysis Questions:**
- Is there high motion or scene cuts? → Use more references
- Are there fine details? → Use higher bitrate
- Are there gradual transitions? → Enable lookahead BRC

---

### 3. **Noise Filtering Before Encoding**

**Methodology:** Apply temporal or spatial filtering to reduce noise before compression.

**Benefits:**
- Encoder focuses on content, not noise
- 8-15% better compression
- Cleaner final output

**Pipeline:**
```
Noisy Input → Denoise Filter → Encoder → Better Quality Output
```

---

### 4. **Color Optimization**

**Procamp Adjustments (Color, Brightness, Saturation):**

```c
auto procamp = vppParams.AddExtBuffer<mfxExtVPPProcamp>();
procamp->Brightness = 0;      // -100 to 100
procamp->Contrast = 10;       // 0.1 to 10.0
procamp->Hue = 0;             // -180 to 180
procamp->Saturation = 1.0;    // 0.0 to 10.0
```

**Quality Impact:** Proper color correction can improve perceived quality by 10-15%.

---

### 5. **HDR Processing**

**For HDR Content:**
- Decode to 10-bit format (P010)
- Use HDR-aware scaling
- Preserve HDR metadata

**Benefits:** Full HDR range preserved, better quality on HDR displays.

```bash
# Decode HDR content to 10-bit output
sample_decode h265 -i hdr_video.h265 -p010 -async 4
```

---

## Quality Metrics and Measurement

### Visual Quality Metrics

**Subjective Metrics:**
- **DMOS (Differential Mean Opinion Score):** Viewer perception (0-5 scale)
- **SSCQE (Single Stimulus Continuous Quality Evaluation):** Real-time quality rating

**Objective Metrics:**

| Metric | Range | Interpretation |
|--------|-------|-----------------|
| **PSNR** | 20-50 dB | Higher = better (>40dB = visually lossless) |
| **SSIM** | 0-1.0 | Higher = better (>0.95 = excellent) |
| **VMAF** | 0-100 | Netflix's perceptual quality metric |
| **BRISQUE** | 0-100 | Lower = better (no-reference quality) |

### Measuring Encoding Quality

**Bitrate vs Quality Analysis:**
```bash
# Encode at different bitrates and compare
for bitrate in 2000 3000 4000 5000 6000; do
  sample_encode h265 -i input.yuv -o output_${bitrate}.h265 \
    -w 1920 -h 1080 -b $bitrate -u slow
  # Analyze file size and quality
done
```

**Find Quality Sweet Spot:**
- Identify bitrate where quality plateaus
- Below plateau: compression artifacts visible
- At plateau: diminishing returns

---

## Quality-First Pipeline Architecture

### Recommended Quality-Optimized Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│                    INPUT VIDEO                              │
└────────────────────────┬────────────────────────────────────┘
                         │
         ┌───────────────▼───────────────┐
         │   VPP Preprocessing            │
         │  ✓ Denoise (optional)          │
         │  ✓ Detail Enhancement          │
         │  ✓ Color Correction            │
         └───────────────┬───────────────┘
                         │
         ┌───────────────▼───────────────┐
         │   ENCODER SETTINGS             │
         │  ✓ Codec: H.265               │
         │  ✓ Target Usage: slow          │
         │  ✓ Reference Frames: 4-5       │
         │  ✓ B-Pyramid: enabled          │
         │  ✓ GOP: 256-512                │
         │  ✓ Bitrate Control: LA-BRC     │
         │  ✓ AsyncDepth: 4               │
         └───────────────┬───────────────┘
                         │
         ┌───────────────▼───────────────┐
         │  ENCODED OUTPUT (H.265)        │
         │  - Optimized for quality       │
         │  - Higher bitrate required     │
         │  - Better compression          │
         └───────────────┬───────────────┘
                         │
         ┌───────────────▼───────────────┐
         │   DECODER SETTINGS             │
         │  ✓ Hardware: enabled           │
         │  ✓ AsyncDepth: 4               │
         └───────────────┬───────────────┘
                         │
         ┌───────────────▼───────────────┐
         │   VPP Post-Processing          │
         │  ✓ Scaling (quality mode)      │
         │  ✓ Denoise (optional)          │
         │  ✓ Detail Enhancement          │
         │  ✓ Color Optimization          │
         └───────────────┬───────────────┘
                         │
         ┌───────────────▼───────────────┐
         │  HIGH-QUALITY OUTPUT           │
         │  - Best possible visual quality│
         │  - For professional use        │
         │  - Suitable for re-encoding    │
         └───────────────────────────────┘
```

### Complete Quality-Optimized Command

**Encoding:**
```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -u veryslow \           # Maximum quality preset
  -b 8000 \              # High bitrate (lossless region)
  -x 5 \                 # Maximum reference frames
  -r 4 \                 # Multiple B-frames
  -g 512 \               # Larger GOP
  -idr_interval 2 \      # Allow non-IDR I-frames
  -bref \                # B-pyramid
  -la -lad 50 \          # Lookahead BRC
  -async 4 \             # Balanced throughput
  -hw                    # Hardware acceleration
```

**Decoding with Quality Output:**
```bash
sample_decode h265 -i output.h265 \
  -y416 \               # 16-bit RGBA output (maximum precision)
  -scaling_mode quality \ # High-quality VPP scaling
  -async 4 \            # Balanced throughput
  -hw                   # Hardware acceleration
```

---

## Quick Reference: Quality Improvement Checklist

### Encoding Quality Improvements (Priority Order)

- [ ] **Switch to H.265** (+40-50% quality)
- [ ] **Use 10-bit codec** if available (+5-10% quality)
- [ ] **Enable Lookahead BRC** (+10-20% quality consistency)
- [ ] **Increase reference frames** to 4-5 (+8-12% compression)
- [ ] **Use `veryslow` preset** (+15-20% compression)
- [ ] **Enable B-pyramid** (+5-15% compression)
- [ ] **Increase GOP size** to 256-512 (+5-8% compression)
- [ ] **Increase bitrate** to quality tier (+subjective quality)
- [ ] **Apply VPP denoise** before encoding (+5-15% compression)

### Decoding Quality Improvements (Priority Order)

- [ ] **Use 10-bit or 16-bit output** format (+5-10% quality)
- [ ] **Enable quality scaling mode** (+20-30% visual quality)
- [ ] **Use adaptive deinterlacing** for interlaced (+15-25% quality)
- [ ] **Apply VPP post-processing** (+5-10% clarity)
- [ ] **Use high-quality output format** (Y416 for max precision)

---

## References

- Intel Video Processing Library (VPL) Official Documentation
- ITU-T Rec. H.264 / H.265 Specifications
- VQEG (Video Quality Experts Group) Guidelines
- Netflix VMAF Quality Assessment Methodology

