# VPP Pipeline Diagrams - Intel VPL Encode/Decode Architecture

This document provides visual representations of how Video Post-Processing (VPP) integrates with encoding and decoding pipelines in Intel VPL, showing data flow, feature interactions, and optimization strategies.

---

## 1. Basic Encoding Pipeline

```
┌─────────────────┐
│  Input Video    │ (RAW, YUV, NV12)
│  (File/Stream)  │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  Encoder                            │
│  ┌────────────────────────────────┐ │
│  │ Motion Estimation              │ │
│  │ Reference Frame Management     │ │
│  │ Rate Control (CBR/VBR/LA-BRC) │ │
│  │ B-Frame/P-Frame Decision       │ │
│  │ GOP Structure                  │ │
│  └────────────────────────────────┘ │
└────────┬────────────────────────────┘
         │
         ▼
┌─────────────────┐
│ Encoded Bitstream
│ (H.264/H.265/VP9)
└─────────────────┘
```

**Performance Metrics:**
- FPS (Frames Per Second)
- Bitrate (kbps)
- Latency (ms)

**Key Parameters:**
- `-preset` (veryfast → veryslow)
- `-b` (target bitrate)
- `-r` (GOP distance)

---

## 2. Encoding Pipeline with VPP Preprocessing

```
┌─────────────────┐
│  Input Video    │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  VPP Preprocessing                  │
│  ┌────────────────────────────────┐ │
│  │ 1. Denoise                     │ │ (Optional: -deinterlace)
│  │    └─ Reduce noise/grain       │ │
│  │                                │ │
│  │ 2. Detail Enhancement          │ │ (Optional: can disable)
│  │    └─ Sharpen edges            │ │
│  │                                │ │
│  │ 3. Scaling/Resize              │ │ (Optional: -sw/-sh)
│  │    └─ Resolution adjustment    │ │
│  │                                │ │
│  │ 4. Color Adjustment (Procamp)  │ │ (Optional: brightness/contrast)
│  │    └─ Saturation adjustment    │ │
│  │                                │ │
│  │ 5. Scene Analysis              │ │ (Informs encoder decisions)
│  │    └─ Content detection        │ │
│  └────────────────────────────────┘ │
└────────┬────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  Encoder                            │
│  (With scene-aware optimizations)   │
└────────┬────────────────────────────┘
         │
         ▼
┌─────────────────────────┐
│ Higher Quality Output   │
│ Lower Bitrate Needed    │
└─────────────────────────┘
```

**Quality Improvements:**
- Denoise: -5-10% bitrate, -2-3% quality loss
- Detail: +3-5% quality
- Scene Analysis: +5-8% adaptive compression

---

## 3. Basic Decoding Pipeline

```
┌──────────────────────┐
│ Encoded Bitstream    │
│ (H.264/H.265/VP9)    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────────────────┐
│ Decoder                          │
│ ┌──────────────────────────────┐ │
│ │ Entropy Decoding             │ │
│ │ Inverse Transform/Quantization
│ │ Reference Frame Reconstruction│ │
│ │ Motion Compensation          │ │
│ │ Loop Filtering               │ │
│ └──────────────────────────────┘ │
└──────────┬───────────────────────┘
           │
           ▼
┌──────────────────────┐
│ Decoded Video        │
│ (NV12/YUV/I420)      │
└──────────────────────┘
           │
           ▼
┌──────────────────────┐
│ Display/Output       │
│ (Screen/File)        │
└──────────────────────┘
```

**Performance Metrics:**
- FPS (Frames Per Second)
- Latency (single-frame)

**Key Parameters:**
- `-d` (device type: GPU/CPU)
- `-timeout` (async timeout)

---

## 4. Decoding Pipeline with VPP Post-Processing

```
┌──────────────────────┐
│ Encoded Bitstream    │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│ Decoder              │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────────────────────┐
│  VPP Post-Processing                 │
│  ┌──────────────────────────────────┐ │
│  │ 1. Deinterlacing (if needed)     │ │ (Optional: -deinterlace)
│  │    ├─ BOB (fast)                 │ │
│  │    └─ ADI (better quality, slow) │ │
│  │                                  │ │
│  │ 2. Scaling/Resize                │ │ (Optional: -sw/-sh)
│  │    ├─ Low-power mode             │ │
│  │    └─ High-quality mode          │ │
│  │                                  │ │
│  │ 3. Denoise Post-Processing       │ │ (Optional)
│  │    └─ Remove compression artifacts
│  │                                  │ │
│  │ 4. Format Conversion             │ │
│  │    ├─ 8-bit (NV12, I420)         │ │
│  │    ├─ 10-bit (P010)              │ │
│  │    └─ 16-bit (P016)              │ │
│  │                                  │ │
│  │ 5. Color Space Adjustment        │ │ (Optional)
│  │    └─ BT.601 ↔ BT.709 ↔ BT.2020  │ │
│  └──────────────────────────────────┘ │
└──────────┬───────────────────────────┘
           │
           ▼
┌──────────────────────┐
│ Display/Output       │
│ (Enhanced Quality)   │
└──────────────────────┘
```

**Quality Improvements:**
- Deinterlacing (ADI): +20-30% visual quality on interlaced content
- Scaling (high-quality): +5-15% vs low-power
- 10-bit output: Eliminates banding artifacts
- Color precision: 16-bit enables editing workflows

---

## 5. Full Quality-Optimized Encoding Pipeline

```
┌────────────────────────────────────────┐
│  Input Video (Low Quality/High Bitrate)│
└────────┬───────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────┐
│  VPP Preprocessing (Quality Focus)     │
│                                        │
│  ┌──────────────────────────────────┐ │
│  │ Scene Analysis                   │ │
│  │ └─ Detect cuts, fades, content  │ │
│  └──────────────────────────────────┘ │
│           │                            │
│           ▼                            │
│  ┌──────────────────────────────────┐ │
│  │ Denoise (if grain-heavy content) │ │
│  │ └─ Custom denoise factor         │ │
│  └──────────────────────────────────┘ │
│           │                            │
│           ▼                            │
│  ┌──────────────────────────────────┐ │
│  │ Optimal Scaling (if needed)      │ │
│  │ └─ Nearest multiple of 16        │ │
│  └──────────────────────────────────┘ │
│           │                            │
│           ▼                            │
│  ┌──────────────────────────────────┐ │
│  │ Detail Enhancement (preserve)    │ │
│  │ └─ Enhance edges for compression │ │
│  └──────────────────────────────────┘ │
└────────┬───────────────────────────────┘
         │
         ▼
┌───────────────────────────────────────┐
│  Encoder (Quality Settings)           │
│                                       │
│  ├─ Codec: H.265 (best quality)       │
│  ├─ Preset: veryslow (quality focus)  │
│  ├─ Target Bitrate: High tier         │
│  ├─ LA-BRC: Enabled (LAD=50-100)      │
│  ├─ References: 5-8 frames            │
│  ├─ B-Pyramid: Enabled                │
│  ├─ GOP Size: 300-600 frames          │
│  ├─ IDR Interval: Auto (key frames)   │
│  └─ Async Depth: 4-8 (balance)        │
│                                       │
│  ┌──────────────────────────────────┐ │
│  │ Two-Pass Encoding (optional)     │ │
│  │ Pass 1: Analyze content          │ │
│  │ Pass 2: Optimize bitrate         │ │
│  └──────────────────────────────────┘ │
└────────┬──────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────┐
│  Output: High-Quality H.265 Bitstream  │
│  ├─ 40-50% smaller vs H.264            │
│  ├─ Minimal visual artifacts           │
│  ├─ Suitable for archival              │
│  └─ Lower bandwidth requirements       │
└────────────────────────────────────────┘
```

**Quality Gains vs Baseline H.264 @ veryfast:**
- Codec switch (H.265): +40-50%
- Preset (veryfast → veryslow): +15-20%
- LA-BRC enabled: +10-15%
- References (1→5): +20-25%
- B-Pyramid: +5-15%
- **Cumulative: 80-95% quality improvement**

---

## 6. VPP Feature Enable/Disable Matrix

```
Feature               │ Encoding  │ Decoding  │ Hardware   │ Quality Impact
──────────────────────┼───────────┼───────────┼────────────┼─────────────────
Denoise              │ Pre-proc  │ Post-proc │ All        │ -5-10% bitrate
Detail Enhancement   │ Pre-proc  │ Post-proc │ All        │ +3-5% quality
Scaling              │ Pre-proc  │ Post-proc │ All        │ Variable
Deinterlacing        │ Pre-proc  │ Post-proc │ Gen7+      │ +20-30% interlaced
Scene Analysis       │ Informs   │ N/A       │ All        │ +5-8% adaptive
Procamp (Colors)     │ Pre-proc  │ Post-proc │ All        │ +2-5% color
HDR Support          │ Yes (I10) │ Yes       │ Gen11+     │ +20-30% HDR content
Format Conversion    │ Input     │ Output    │ All        │ 8/10/16-bit options
Color Space Conv.    │ Optional  │ Optional  │ All        │ Accuracy by space
Resize Quality       │ All       │ All       │ All        │ +5-15% vs low-power
```

**Compatibility Notes:**
- VPP features can be combined (Denoise + Detail + Scaling)
- Each additional filter adds ~10-30% processing overhead
- Scene Detection + Denoise + Scaling = ~50% more processing
- Hardware acceleration available on 7th Gen Intel Core+

---

## 7. Async Depth Buffering Visualization

```
Time →

Encoding with Async Depth = 1 (Blocking):
┌────┐
│ Enc│ Submit Frame 1    ┌────┐
└────┘ █████████████████│ Enc│ Submit Frame 2    ┌────┐
       (Output Wait)    └────┘ █████████████████│ Enc│ ...
                         (Output Wait)          └────┘

Throughput: ~30 FPS (if encode time = 33ms)


Encoding with Async Depth = 4 (Pipelined):
┌────┐
│ Enc│ Submit Frame 1
└────┘ │
       ├─ Encode in GPU/HW
       │
       ▼
┌────┐  (Parallel)  ┌────┐  (Parallel)  ┌────┐
│ Enc│ Submit F2    │ Enc│ Submit F3    │ Enc│ Submit F4  ┌────┐
└────┘              └────┘              └────┘           │Out │
                                                          └────┘
                                                       Output Ready (F1)

Throughput: ~120 FPS (4x improvement)

Async Depth vs Performance:
1  ▁▂▃▄▅▆▇█
2  ▁▂▃▄▅▆███
3  ▁▂▃▄▅███████
4  ▁▂▃▄████████ ◄─ Recommended
5  ▁▂▃▄████████
8  ▁▂▃▄████████
12 ▁▂▃▄███████ (diminishing returns, higher latency)
```

**Latency vs Throughput Trade-off:**
- Async Depth = 1: Low latency (1 frame), Low throughput
- Async Depth = 4: Balanced (good throughput, 4-frame latency)
- Async Depth = 8+: High throughput, High latency (batch processing)

---

## 8. Reference Frame Impact Visualization

```
Reference Frames: 1 (Baseline)
┌─────┐
│Frame│  (I-frame)
│  1  │
└─────┘
  ↓  (P-frame, references Frame 1 only)
┌─────┐
│Frame│  Motion vectors → [small range]
│  2  │  Compression: ~85%
└─────┘
  ↓  (P-frame, references Frame 1 only)
┌─────┐
│Frame│
│  3  │  Compression: ~85%
└─────┘

Quality: 70/100   |   FPS: 100   |   Bitrate: 8000 kbps


Reference Frames: 3 (Better Motion Prediction)
┌─────┐
│Frame│  (I-frame)
│  1  │
└─────┘
  ↓  (P-frame, can reference F1, F2, or F3)
┌─────┐
│Frame│  Motion vectors → [wider range]
│  2  │  Compression: ~91%
└─────┘
  ↓  (P-frame, can reference F1, F2, or F3)
┌─────┐
│Frame│  Better temporal prediction
│  3  │  Compression: ~91%
└─────┘

Quality: 82/100   |   FPS: 85   |   Bitrate: 6800 kbps (+12-15% quality)


Reference Frames: 5+ (Maximum Motion Analysis)
┌─────┐
│Frame│  (I-frame)
│  1  │
└─────┐
  ↓  (P-frame, references F1-F5)
┌─────┐
│Frame│  Motion vectors → [maximum analysis]
│  2  │  Compression: ~94%
└─────┘
  ↓  (P-frame, references F1-F5)
┌─────┐
│Frame│  Advanced motion compensation
│  3  │  Compression: ~94%
└─────┘

Quality: 88/100   |   FPS: 70   |   Bitrate: 6200 kbps (+20-25% quality)

Compression Improvement:
Refs: 1    3    5    8
      ├────┬────┬────┤
      0% +15% +25% +30% (diminishing returns)
```

---

## 9. B-Pyramid Reference Structure

```
Standard GOP (No B-Pyramid):
I-Frame → P → B → B → P → B → B → I-Frame
            ↑       ↑       ↑
         (refs I)  (refs I,P)  (refs I,P)

B-frames can only reference I and P frames
Maximum prediction flexibility: Low


B-Pyramid Enabled:
I-Frame → P → B → B → P → B → B → I-Frame
            ↑   ↑   ↑   ↑   ↑
            │   └─→B← │   │
            └──────P──┘   │
                    └─────P (can ref previous B!)

B-frames can reference other B-frames
Maximum prediction flexibility: High
Compression improvement: +5-15%

Temporal Hierarchy (4-level pyramid):
      I-Frame (Layer 0)
       ↓     ↓
      P1     P2 (Layer 1)
     ↙ ↘   ↙ ↘
    B B  B  B (Layer 2)

Multi-level references enable better temporal prediction
```

---

## 10. Rate Control Modes Performance Comparison

```
CBR (Constant Bitrate):
Bitrate
    │    ┌──┐  ┌──┐
8000├────┤  ├──┤  ├────
    │    │  │  │  │
    └────┴──┴──┴──┴──── → Time
         Quality inconsistent (varies per frame)
         Predictable bandwidth
         Use case: Streaming, broadcast

VBR (Variable Bitrate):
Bitrate
    │      ┌──┐
8000├────┬─┘  └─┬────
    │    │      │
    └────┴──────┴────→ Time
         Quality consistent
         Unpredictable bandwidth
         Use case: Archival, download

LA-BRC (Lookahead Rate Control):
Bitrate
    │   ┌───────┐
8000├───┤       ├────
    │   │       │
    └───┴───────┴────→ Time
         Quality consistent + optimal compression
         Lookahead window (LAD): 0-100 frames
         Use case: Quality-focused archival
         Overhead: -20-30% encoding speed, +10-15% quality
```

---

## 11. Encoding Preset Impact Timeline

```
Quality
   ↑
95 │
   │              veryslow ◆
90 │           verylow ◆
   │        slow ◆
85 │     medium ◆
   │   fast ◆
80 │ veryfast ◆
   │
75 │
   └──────────────────────→ Encoding Speed (FPS)
   0   30    60    90   120

veryfast:  ~3x faster, 15-20% worse quality
fast:      ~2x faster, 10-15% worse quality
medium:    ~1.2x faster, 5-8% worse quality
slow:      ~0.8x slower, 5-8% better quality
veryslow:  ~15-20x slower, 15-20% better quality

Decision Matrix:
Use veryfast:  Real-time streaming, live broadcasts
Use fast:      Balanced streaming applications
Use medium:    General-purpose encoding
Use slow:      High-quality applications
Use veryslow:  Archival, premium on-demand services
```

---

## 12. Hardware Acceleration Pipeline

```
Software-Only Encoding:
Input → CPU Processing → Output
         (Single core bottleneck)
         FPS: 30-60 (1080p)

Hardware-Accelerated Encoding (GPU/VE):
Input → ┌─────────────────────┐
        │ GPU/Media Engine    │
        │ ├─ Motion Est.     │
        │ ├─ Transform       │
        │ ├─ Quantization    │
        │ ├─ Entropy Coding  │
        │ └─ Loop Filtering  │
        └─────────────────────┘
              ↓
         Output (Async)

FPS: 200-1000+ (1080p)
Power: 5-10W vs 40-60W (CPU)

Hardware Support by Generation:
├─ 7th Gen (Kaby Lake):     H.264/H.265
├─ 8th Gen (Coffee Lake):   H.264/H.265 + VP9
├─ 10th Gen (Comet Lake):   H.264/H.265 + VP9
├─ 11th Gen (Tiger Lake):   H.264/H.265 + VP9 + AV1
└─ 12th Gen+ (Alder Lake):  H.264/H.265 + VP9 + AV1 + HDR
```

---

## 13. Quality vs Bitrate Curve by Codec

```
Visual Quality
    ↑
100 │        AV1 ▲
    │           /
 95 │         /  H.265 ▲
    │       /         /
 90 │     /         /  H.264 ▲
    │   /         /         /
 85 │ /         /         /
    │         /         /
 80 │       /         /
    ├─────────────────────────→ Bitrate (kbps)
    0  2000  4000  6000  8000

At 5000 kbps (1080p):
├─ H.264:  Quality ~80/100
├─ H.265:  Quality ~90/100  (+40-50% vs H.264)
└─ AV1:    Quality ~95/100  (+30% vs H.265, +80-100% vs H.264)

Trade-off:
H.264:  Fastest encoding, worst quality, best compatibility
H.265:  Medium speed, excellent quality, good compatibility
AV1:    Slowest encoding, best quality, limited device support
```

---

## 14. Multi-Pass Encoding Workflow

```
Input Video
     ↓
┌────────────────────────┐
│ Pass 1: Analysis Phase │
│                        │
│ Analyze statistics:    │
│ - Scene complexity     │
│ - Motion vectors       │
│ - Color distribution   │
│ - Bitrate peaks/troughs│
│ - Target CRF value     │
│                        │
│ Output: Stats file     │
│ Processing: ~1x speed  │
└────────────────────────┘
     ↓ (Stats file + original video)
┌────────────────────────┐
│ Pass 2: Encoding Phase │
│                        │
│ Use analysis data:     │
│ - Allocate bitrate     │
│ - Adaptive GOP sizing  │
│ - Dynamic preset adj.  │
│ - Reference optimization
│                        │
│ Output: Optimized      │
│ bitstream              │
│ Processing: ~1.5x speed│
└────────────────────────┘
     ↓
High-Quality Output
├─ 10-15% better quality vs 1-pass
├─ 2.5x slower than 1-pass
└─ Best for archival/on-demand

Trade-off:
1-Pass:  Fast, good quality
2-Pass:  10-15% better, 2.5x slower
```

---

## 15. Color Space and Bit Depth Preservation

```
Input: 10-bit HDR (BT.2020)
     ↓
Processing Decision:
├─ 8-bit Output (NV12):     Quality Loss ~30%
│                            (Color banding visible)
│
├─ 10-bit Output (P010):    No Loss
│                            (Preserves HDR quality)
│                            (File size: 1.25x larger)
│
└─ 16-bit Output (P016):    No Loss + Extra Precision
                             (Professional editing)
                             (File size: 2x larger)

Encoding Pipeline:
Internal:  10-bit ← Input
           ↓
       Processing (10-bit)
           ↓
Output: ┌─ 8-bit  (Streaming, playback)
        ├─ 10-bit (HDR, preservation)
        └─ 16-bit (Archival, editing)

Quality Preservation:
Input → 8-bit  → Display:  Color banding visible
        ↓
Input → 10-bit → Display:  No banding
        ↓
Input → 16-bit → Edit:     Professional precision
```

---

## Quick Reference: Parameter Impact on Pipeline

```
ENCODING:
├─ Preset (-preset)
│  └─ Affects: Speed vs Quality
│     veryfast: +3x FPS, -20% quality
│     veryslow: -15-20x FPS, +20% quality
│
├─ Bitrate (-b)
│  └─ Affects: Quality consistency
│     3000 kbps: Streaming quality
│     8000 kbps: High quality (1080p)
│     12000 kbps: Archival (near lossless)
│
├─ References (-ref)
│  └─ Affects: Compression ratio
│     1: Fast (-10% quality)
│     3: Balanced
│     5+: Quality-focused (-30% FPS)
│
├─ LA-BRC (Lookahead)
│  └─ Affects: Quality consistency
│     Enabled: +10-15% quality, -20-30% FPS
│
├─ B-Pyramid (-bref)
│  └─ Affects: Temporal prediction
│     Enabled: +5-15% compression

DECODING:
├─ Hardware Acceleration (-d)
│  └─ Affects: Decoding speed
│     GPU: +500% FPS
│
├─ Async Depth
│  └─ Affects: Throughput vs latency
│     1: Low latency, low FPS
│     4: Balanced
│     8+: High FPS, high latency
│
└─ Output Format (-of)
   └─ Affects: Output quality/size
      NV12: Standard (8-bit)
      P010: HDR quality (10-bit)
      P016: Professional (16-bit)
```

---

## Summary: When to Use VPP Features

| Scenario | VPP Features | Expected Gain |
|----------|--------------|---------------|
| **High Bitrate Streaming** | Denoise + Scene Detection | -10% bitrate, same quality |
| **Quality Archival** | Denoise + Detail + LA-BRC | +80-95% cumulative quality |
| **Real-time Broadcast** | Minimal (fast encode) | Low latency |
| **HDR Content** | Scene Analysis + 10-bit | +20-30% quality |
| **Interlaced Video** | Deinterlacing (ADI) | +20-30% on interlaced |
| **Low-bitrate Stream** | Denoise + Procamp | -5% bitrate, visible improvement |
| **Playback Optimization** | Scaling (quality mode) | +5-15% vs low-power |
| **Professional Editing** | 16-bit output + Detail | Maximum precision |

