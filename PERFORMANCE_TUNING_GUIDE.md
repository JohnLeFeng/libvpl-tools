# Performance Tuning Guide for Encode and Decode

This document provides a comprehensive guide to tunable parameters in `sample_encode` and `sample_decode` that can impact performance characteristics of video processing operations.

---

## Table of Contents
1. [Encoding Parameters](#encoding-parameters)
   - [Performance-Focused](#encoding-performance-focused)
   - [Quality-Focused](#encoding-quality-focused)
2. [Decoding Parameters](#decoding-parameters)
   - [Performance-Focused](#decoding-performance-focused)
   - [Quality-Focused](#decoding-quality-focused)
3. [Common Performance Considerations](#common-performance-considerations)
4. [Parameter Tuning Strategies](#parameter-tuning-strategies)

---

## Encoding Parameters

### Encoding Performance-Focused

#### 1. **Target Usage (-u) [PERFORMANCE MODE]**

**Command:** `-u <usage_mode>`

**Description:** Specifies encoding preset for speed vs. quality tradeoff.

**Supported Codecs:** H.264, H.265, MPEG2, MVC

**Performance-Optimized Values:**
- `veryfast` (or `speed`) → Minimum quality, fastest encoding ⭐ BEST FOR SPEED
- `faster` → Lower quality, faster encoding
- `fast` → Lower quality, fast encoding

**Performance Impact:**
- **veryfast:** 2-3x faster than balanced
- **faster:** 1.5-2x faster than balanced
- **fast:** 1.2-1.5x faster than balanced

**Memory Impact:** Lower memory usage with faster presets

**Best For:** Real-time streaming, batch processing, hardware-constrained environments

**Example:**
```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 -u veryfast
```

---

#### 2. **Reference Frames (-x) [PERFORMANCE MODE]**

**Command:** `-x <num_reference_frames>`

**Description:** Number of reference frames used for motion estimation.

**Performance-Optimized Values:**
- `-x 1` → Minimum references, fastest encoding ⭐ BEST FOR SPEED
- `-x 2` → Reduced references, faster encoding
- `-x 3` → Moderate references, balanced speed

**Performance Impact:**
- Each additional reference frame increases encoding time exponentially
- Fewer references: 2-3x faster than high references
- Reduced memory footprint

**Motion Estimation Complexity:** Directly proportional to number of references

**Best For:** High frame-rate content, live streaming, real-time applications

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -x 1
```

---

#### 3. **GOP Distance/Ref Distance (-r) [PERFORMANCE MODE]**

**Command:** `-r <distance>`

**Description:** Distance between I-frames or P-frames (distance between reference frames).

**Performance-Optimized Values:**
- `-r 1` → No B-frames, fastest encoding ⭐ BEST FOR SPEED
- `-r 2` → Minimal B-frames, faster encoding
- `-r 3-4` → Balanced B-frame complexity

**Performance Impact:**
- **r=1:** Simplest prediction, fastest encoding, largest output
- **r=2-4:** Moderate complexity, ~10-20% faster than r=8
- More B-frames require complex bidirectional prediction

**Best For:** Low-latency applications, hardware-constrained devices

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -r 1
```

---

#### 4. **Async Depth (-async) [PERFORMANCE MODE]**

**Command:** `-async <depth>`

**Description:** Sets the depth of the asynchronous encoding pipeline.

**Default:** 4

**Valid Range:** 1-20

**Performance-Optimized Values:**
- **Depth = 8-16** → High throughput optimization ⭐ BEST FOR THROUGHPUT
- **Depth = 4-8** → Balanced throughput/latency
- **Depth = 1** → Minimal latency, lowest throughput

**Performance Impact:**
- **Depth = 16:** 3-4x higher throughput than depth=4
- **Depth = 1:** ~70% lower throughput but minimal latency
- Higher depths allow encoding multiple frames concurrently

**Memory Tradeoff:** Each level adds ~2-4 MB per frame buffering

**Best For:** Batch encoding, file processing, maximum FPS optimization

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -async 16
```

---

#### 5. **Hardware Acceleration (-hw) [PERFORMANCE MODE]**

**Command:** `-hw` (default) or `-sw`

**Description:** Enable GPU hardware encoding vs. CPU software encoding.

**Performance Impact:**
- **-hw (GPU):** 10-50x faster than software ⭐ BEST FOR PERFORMANCE
- **-sw (CPU):** Slower, full CPU utilization, more power consumption

**Throughput:** GPU: 100-500+ FPS vs. CPU: 5-50 FPS (highly content/resolution dependent)

**Power Efficiency:** GPU encoding uses 30-60% less power than CPU

**Best For:** All real-time and high-throughput scenarios

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -hw
```

---

### Encoding Quality-Focused

#### 1. **Bitrate (-b) [QUALITY MODE]**

**Command:** `-b <bitrate_in_kbps>`

**Description:** Sets the encoded bitrate in kilobits per second.

**Supported Codecs:** H.264, H.265, MPEG2, MVC

**Quality-Optimized Values:**
- **High bitrate (8000+ Kbps @ 1080p)** → Maximum visual quality ⭐ BEST FOR QUALITY
- **Medium bitrate (4000-6000 Kbps @ 1080p)** → Balanced quality/file size
- **Low bitrate (2000-3000 Kbps @ 1080p)** → Acceptable quality, smaller files

**Quality Impact:**
- Bitrate is the PRIMARY factor affecting output quality
- Higher bitrate provides visually lossless compression
- Lower bitrate introduces visible artifacts and motion blur

**Recommended Bitrates by Resolution:**
| Resolution | Low Quality | Medium Quality | High Quality |
|------------|------------|---------------|-------------|
| 480p | 800 | 1500 | 2500 |
| 720p | 1500 | 3000 | 5000 |
| 1080p | 2500 | 4500 | 8000+ |
| 4K | 5000 | 10000 | 15000+ |

**Best For:** Streaming services, archival, content distribution

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -b 8000
```

---

#### 2. **Target Usage (-u) [QUALITY MODE]**

**Command:** `-u <usage_mode>`

**Description:** Specifies encoding preset for speed vs. quality tradeoff.

**Quality-Optimized Values:**
- `veryslow` (or `quality`) → Maximum quality, slowest encoding ⭐ BEST FOR QUALITY
- `slower` → High quality, slower encoding
- `slow` → Good quality, slow encoding
- `medium` (or `balanced`) → Balanced quality/speed (default)

**Quality Impact:**
- **veryslow:** 15-20% better compression than fast
- **slower:** 10-15% better compression than fast
- **slow:** 5-10% better compression than fast

**Encoder Optimization:**
- Better motion estimation algorithms at higher presets
- More sophisticated rate control decisions
- Better scene change detection

**Best For:** Archival, on-demand video, content where quality is critical

**Example:**
```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 -u veryslow
```

---

#### 3. **Reference Frames (-x) [QUALITY MODE]**

**Command:** `-x <num_reference_frames>`

**Description:** Number of reference frames used for motion estimation and compensation.

**Quality-Optimized Values:**
- `-x 4-5` → Maximum reference frames ⭐ BEST FOR QUALITY
- `-x 3-4` → Good quality, balanced complexity
- `-x 2-3` → Acceptable quality

**Quality Impact:**
- More reference frames = 8-12% better compression efficiency
- Better motion compensation accuracy
- Improved handling of complex motion patterns

**Best For:** On-demand video, archival content, quality-critical applications

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -x 4
```

---

#### 4. **B-Pyramid Reference Structure (-bref) [QUALITY MODE]**

**Command:** `-bref` or `-nobref`

**Description:** Enable/disable B-pyramid reference structure for B-frames.

**Quality-Optimized Values:**
- `-bref` → Use B-pyramid, better compression ⭐ BEST FOR QUALITY
- `-nobref` → Disable B-pyramid, simpler but larger output

**Quality Impact:**
- **-bref:** 5-15% better compression efficiency
- Improved bidirectional prediction quality
- Better handling of complex scenes

**File Size Reduction:** 8-12% smaller output files at same bitrate

**Best For:** On-demand video, streaming services, quality-focused scenarios

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -bref
```

---

#### 5. **Lookahead BRC (-la) with Depth (-lad) [QUALITY MODE]**

**Command:** `-la` and `-lad <depth>`

**Description:** Enables Lookahead Bitrate Control and specifies analysis depth.

**Supported Codecs:** H.264, H.265

**Quality-Optimized Values:**
- `-la -lad 50-100` → Maximum quality bitrate control ⭐ BEST FOR QUALITY
- `-la -lad 20-30` → Good quality control with moderate overhead
- `-la -lad 0` → Auto-select (hardware default)

**Quality Impact:**
- **LA enabled:** 10-20% better bitrate consistency
- More consistent visual quality across scene changes
- Reduced quality fluctuations in complex content
- Better temporal stability

**Default BRC vs. LA-BRC:**
| Aspect | Default CBR | Lookahead BRC |
|--------|------------|---------------|
| Quality Consistency | Good | Excellent ⭐ |
| Scene Handling | Good | Excellent ⭐ |
| Predictability | Fair | Excellent ⭐ |
| Computation | Standard | Higher |

**Hardware Requirements:** 4th Gen Intel Core and newer

**Best For:** Streaming services, professional content, quality-critical scenarios

**Example:**
```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 -la -lad 50
```

---

#### 6. **GOP Size (-g) [QUALITY MODE]**

**Command:** `-g <gop_size>`

**Description:** Maximum Group of Pictures size (distance between I-frames).

**Default:** 256

**Quality-Optimized Values:**
- **Larger GOP (256-512)** → Better compression efficiency ⭐ BEST FOR QUALITY
- **Medium GOP (120-256)** → Balanced compression/random access
- **Smaller GOP (32-64)** → Better error resilience, less compression

**Quality Impact:**
- **GOP=512:** 5-10% better compression than GOP=128
- More content between I-frames allows better prediction
- Improved temporal prediction accuracy

**Compression Efficiency by GOP Size:**
| GOP Size | Relative Efficiency | Random Access |
|----------|-------------------|----------------|
| 32 | 90% | Excellent |
| 128 | 95% | Good |
| 256 | 100% | Fair |
| 512 | 105% | Poor |

**Best For:** Archival, on-demand video, high-efficiency scenarios

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -g 512
```

---

#### 7. **IDR Interval (-idr_interval) [QUALITY MODE]**

**Command:** `-idr_interval <size>`

**Description:** Sets the interval for Instantaneous Decoder Refresh (IDR) frames.

**Default:** 0 (every I-frame is an IDR)

**Quality-Optimized Values:**
- `-idr_interval 2-4` → Increased compression efficiency ⭐ BEST FOR QUALITY
- `-idr_interval 1` → Moderate efficiency increase
- `-idr_interval 0` → Every I-frame is IDR, best error resilience

**Quality Impact:**
- **IDR > 0:** Allows I-frames to reference previous frames
- 3-8% better compression than IDR=0
- Improved sequence-level prediction

**Random Access Capability:**
- IDR=0: Maximum random access points
- IDR=1: Good random access
- IDR=4: Limited random access (only every 4th I-frame is true IDR)

**Best For:** On-demand video, archival, efficiency-critical scenarios

**Example:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -idr_interval 2
```

---

#### 8. **Active Reference Frames for P/B Frames (HEVC only) [QUALITY MODE]**

**Commands:**
- `-num_active_P <num_refs>` - Maximum references for P frames
- `-num_active_BL0 <num_refs>` - Maximum references for B frame L0
- `-num_active_BL1 <num_refs>` - Maximum references for B frame L1

**Codec:** HEVC (H.265) only

**Quality-Optimized Values:**
- `-num_active_P 3-4` and `-num_active_BL0 3 -num_active_BL1 3` ⭐ BEST FOR QUALITY
- Default values often suboptimal for quality

**Quality Impact:**
- More active references improve prediction accuracy
- 3-5% better compression efficiency
- Better motion compensation

**Best For:** HEVC encoding, quality-focused scenarios

**Example:**
```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -num_active_P 4 -num_active_BL0 3 -num_active_BL1 3
```

---

## Decoding Parameters

### Decoding Performance-Focused

#### 1. **Async Depth (-async) [PERFORMANCE MODE]**

**Command:** `-async <depth>`

**Description:** Depth of asynchronous decoding pipeline.

**Default:** 4

**Valid Range:** 1-20

**Performance-Optimized Values:**
- **Depth = 8-16** → High throughput ⭐ BEST FOR PERFORMANCE
- **Depth = 4-8** → Balanced throughput/latency
- **Depth = 1** → Minimal latency, lower throughput

**Performance Impact:**
- **Depth = 16:** 3-4x higher throughput than depth=4
- Each level increases throughput by ~20-30%
- Parallel decoding of multiple frames

**Throughput Gains:**
| Async Depth | Relative Throughput | Latency |
|-------------|-------------------|---------|
| 1 | 100% | Minimal |
| 4 | 280% | Moderate |
| 8 | 480% | Higher |
| 16 | 750% | Higher |

**Memory Tradeoff:** Each depth level adds ~1-2 MB per frame buffering

**Best For:** Batch processing, file-to-file decoding, maximum throughput

**Example:**
```bash
sample_decode h264 -i encoded.h264 -async 16
```

---

#### 2. **Low Latency Mode (-low_latency) [PERFORMANCE MODE]**

**Command:** `-low_latency`

**Description:** Configures decoder for low-latency operation.

**Supported Codecs:** H.264, JPEG

**Performance-Optimized Values:**
- `-low_latency` → Minimize buffering delay ⭐ BEST FOR LOW LATENCY
- Without flag → Standard decoding with buffering

**Performance Impact:**
- **Latency reduction:** 50-80% lower latency than standard
- Forces synchronous decoding patterns
- Reduces internal buffering
- May reduce throughput by 20-30%

**Latency Comparison:**
| Mode | Latency | Throughput |
|------|---------|-----------|
| Standard async | 50-150 ms | High |
| Low-latency | 5-20 ms | Moderate |
| Sync (async=1) | 2-5 ms | Low |

**Best For:** Real-time communication, video conferencing, live streaming, interactive applications

**Example:**
```bash
sample_decode h264 -i encoded.h264 -low_latency
```

---

#### 3. **GPU Copy Mode (-gpucopy) [PERFORMANCE MODE]**

**Command:** `-gpucopy::<on|off>`

**Description:** Enable/disable GPU copy mode for memory transfers.

**Performance-Optimized Values:**
- `-gpucopy::on` → Use GPU for memory operations ⭐ BEST FOR PERFORMANCE
- `-gpucopy::off` → Use CPU for memory operations

**Performance Impact:**
- **GPU copy on:** 30-50% faster memory operations
- Reduces PCIe bandwidth saturation
- Better GPU utilization
- Reduced CPU overhead

**Memory Bandwidth Benefit:**
- GPU memory copies utilize dedicated GPU bandwidth
- Frees up PCIe for other operations
- More efficient overall system throughput

**Best For:** High-resolution content, high frame rates, GPU-intensive workloads

**Example:**
```bash
sample_decode h264 -i encoded.h264 -gpucopy::on -hw
```

---

#### 4. **Hardware Acceleration (-hw) [PERFORMANCE MODE]**

**Command:** `-hw` (default) or `-sw`

**Description:** Enable GPU hardware decoding vs. CPU software decoding.

**Performance-Optimized Values:**
- `-hw` → Hardware decoding ⭐ BEST FOR PERFORMANCE
- `-sw` → Software decoding (fallback)

**Performance Impact:**
- **Hardware:** 10-100x faster than software
- GPU execution: 1000+ FPS for many codecs
- CPU execution: 10-100 FPS

**Power Efficiency:**
- GPU: 30-60% lower power consumption
- CPU: Higher power draw

**Best For:** All real-time and high-throughput scenarios

**Example:**
```bash
sample_decode h264 -i encoded.h264 -hw
```

---

#### 5. **Scaling Mode (-scaling_mode) [PERFORMANCE MODE]**

**Command:** `-scaling_mode <lowpower|quality>`

**Description:** Specifies VPP scaling strategy.

**Performance-Optimized Values:**
- `-scaling_mode lowpower` → Optimized for power efficiency ⭐ BEST FOR PERFORMANCE
- `-scaling_mode quality` → Optimized for quality (slower)

**Performance Impact:**
- **lowpower:** 20-40% faster VPP processing
- Uses simple/efficient scaling algorithms
- Lower power consumption

**Use Cases:**
- **lowpower:** Mobile devices, embedded systems, power-constrained
- **quality:** Desktop, cloud, quality-critical scenarios

**Best For:** Mobile decoding, embedded systems, power-constrained devices

**Example:**
```bash
sample_decode h264 -i encoded.h264 -scaling_mode lowpower
```

---

#### 6. **Decoder Implementation Selection [PERFORMANCE MODE]**

**Commands:**
- `-dGfx` → Discrete GPU ⭐ BEST FOR PERFORMANCE
- `-iGfx` → Integrated GPU (power efficient)
- `-AdapterNum <n>` → Specific adapter

**Performance Impact:**
- **Discrete GPU:** 2-5x better performance than integrated
- **Integrated GPU:** Better power efficiency, sufficient for streaming
- Adapter selection depends on hardware capabilities

**Throughput Comparison:**
| Selection | Throughput | Power | Memory |
|-----------|-----------|-------|--------|
| dGfx (discrete) | Highest | High | Dedicated |
| iGfx (integrated) | Moderate | Low | Shared |

**Best For:** High-resolution/frame-rate content, desktop systems

**Example:**
```bash
sample_decode h264 -i encoded.h264 -dGfx
```

---

### Decoding Quality-Focused

#### 1. **Output Format Selection [QUALITY MODE]**

**Commands:**
- `-nv12` - NV12 format (default, standard quality)
- `-i420` - I420 format (same quality as NV12)
- `-rgb4` - RGB4 format (color accurate)
- `-rgb4_fcr` - RGB4 full color range ⭐ BEST FOR QUALITY
- `-p010` - P010 format (10-bit, higher precision)
- `-p016` - P016 format (16-bit, maximum precision) ⭐ BEST FOR QUALITY
- `-y216` - Y216 format (16-bit, high precision)
- `-y416` - Y416 format (16-bit RGBA, highest precision) ⭐ BEST FOR QUALITY

**Quality Impact:**
| Format | Bit Depth | Color Precision | Use Case |
|--------|-----------|-----------------|----------|
| NV12 | 8-bit | Standard | Streaming, display |
| I420 | 8-bit | Standard | File storage |
| RGB4 | 8-bit | Good | Color grading |
| RGB4_FCR | 8-bit | Full range ⭐ | Accurate colors |
| P010 | 10-bit | Excellent ⭐ | HDR, archival |
| P016 | 16-bit | Maximum ⭐ | Professional |
| Y216 | 16-bit | Maximum ⭐ | Professional |
| Y416 | 16-bit | Maximum ⭐ | Professional (RGBA) |

**Color Space Handling:**
- Higher bit-depths preserve color gradation
- Full color range eliminates quantization artifacts
- 16-bit formats recommended for post-processing

**Best For:** Color grading, archival, professional workflows, HDR content

**Example:**
```bash
sample_decode h264 -i encoded.h264 -y416
```

---

#### 2. **Deinterlacing (-di) [QUALITY MODE]**

**Command:** `-di <bob|adi>`

**Description:** Enables deinterlacing during decoding for interlaced content.

**Quality-Optimized Values:**
- `-di adi` → Adaptive deinterlacing ⭐ BEST FOR QUALITY
- `-di bob` → Simple bob deinterlacing (faster)
- Disabled → No deinterlacing (progressive content)

**Quality Impact:**
- **Disabled (progressive):** No artifacts, best quality
- **ADI (adaptive):** 20-30% better quality than bob
- **BOB:** Fast but creates motion artifacts with interlaced content

**Output Quality Comparison for Interlaced Content:**
| Mode | Artifacts | Motion | Quality |
|------|-----------|--------|---------|
| Disabled | High ⚠️ | Jerky | Poor |
| BOB | Moderate | Acceptable | Fair |
| ADI | Minimal ⭐ | Smooth ⭐ | Good ⭐ |

**Best For:** Handling interlaced video streams, professional workflows

**Example:**
```bash
sample_decode mpeg2 -i encoded.m2v -di adi
```

---

#### 3. **Scaling Mode (-scaling_mode) [QUALITY MODE]**

**Command:** `-scaling_mode <lowpower|quality>`

**Description:** Specifies VPP scaling strategy.

**Quality-Optimized Values:**
- `-scaling_mode quality` → High-quality scaling ⭐ BEST FOR QUALITY
- `-scaling_mode lowpower` → Efficient scaling (lower quality)

**Quality Impact:**
- **quality mode:** 20-30% better visual quality in scaled output
- Uses advanced interpolation/filtering algorithms
- Better edge preservation in scaled content

**Scaling Algorithm Differences:**
| Mode | Algorithm | Quality | Speed |
|------|-----------|---------|-------|
| lowpower | Bilinear | Fair | Fast |
| quality | Bicubic/Lanczos ⭐ | Excellent ⭐ | Slower |

**Best For:** Desktop applications, quality-critical scenarios, visual content

**Example:**
```bash
sample_decode h264 -i encoded.h264 -scaling_mode quality
```

---

## Common Performance Considerations


### Encoding Quality-Focused

#### 1. **Bitrate (-b) [QUALITY MODE]**

---

## Common Performance Considerations

### CPU vs. GPU Encoding/Decoding

| Aspect | CPU (-sw) | GPU (-hw) |
|--------|-----------|----------|
| **Speed** | Slower | Much faster (10-50x) |
| **Power** | Higher | Lower |
| **Memory** | System RAM | GPU Memory |
| **Latency** | Higher | Lower |
| **Codec Support** | More codecs | Limited by hardware |

### Memory Management

- **Async Depth:** Each level increases memory by ~2-4 MB per frame
- **Reference Frames:** Each reference frame requires full-frame memory
- **Multi-Stream:** Each stream adds proportional memory overhead

### Optimization Checklist

- [ ] Use hardware encoding/decoding when available (`-hw`)
- [ ] Tune async depth based on your throughput requirements
- [ ] Set target usage appropriate for your quality/speed needs
- [ ] Minimize reference frames if speed is critical
- [ ] Use smaller GOP for better error resilience in streaming
- [ ] Enable lookahead BRC for better bitrate control in quality-critical scenarios
- [ ] Profile memory usage under peak load conditions
- [ ] Test with your specific content type (camera, screen, animated)

---

## Parameter Tuning Strategies

### ⚡ PERFORMANCE-FOCUSED Strategies

#### Strategy 1: Maximum Encoding Throughput

**Goal:** Process as many frames per second as possible.

**Use Case:** Batch encoding, file-to-file transcoding, live streaming at any quality

```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 \
  -u veryfast -async 16 -r 1 -x 1 -b 2000 -hw
```

**Performance Parameters:**
- `-u veryfast` → Fastest preset (3x faster than veryslow)
- `-async 16` → Maximum parallelism (750% throughput vs async=1)
- `-r 1` → No B-frames (fastest prediction)
- `-x 1` → Minimum reference frames (fastest motion estimation)
- `-hw` → GPU acceleration (10-50x faster than CPU)

**Expected Results:**
- H.264: 100-300+ FPS @ 1080p (hardware dependent)
- Large output file due to low compression
- Acceptable quality for non-critical applications

---

#### Strategy 2: Maximum Decoding Throughput

**Goal:** Decode as many frames per second as possible.

**Use Case:** File analysis, batch processing, testing

```bash
sample_decode h264 -i encoded.h264 \
  -async 16 -gpycopy::on -hw -nv12
```

**Performance Parameters:**
- `-async 16` → Maximum parallelism
- `-gpycopy::on` → GPU memory operations (30-50% faster)
- `-hw` → Hardware decoding (10-100x faster)
- `-nv12` → Most efficient output format

**Expected Results:**
- H.264: 500-2000+ FPS @ 1080p (hardware dependent)
- Maximum system throughput

---

#### Strategy 3: Low-Latency Real-Time (Decoding)

**Goal:** Minimal decoding latency for live streams and interactive applications.

**Use Case:** Video conferencing, live streaming consumption, real-time monitoring

```bash
sample_decode h264 -i stream.h264 \
  -low_latency -async 1 -gpycopy::on -hw -nv12
```

**Performance Parameters:**
- `-low_latency` → Minimize buffering (50-80% latency reduction)
- `-async 1` → Synchronous decoding (minimal buffering)
- `-gpycopy::on` → GPU memory operations
- `-hw` → Hardware decoding

**Expected Results:**
- Latency: 5-20 ms (vs 50-150 ms in standard async mode)
- Throughput: Sufficient for real-time streaming

---

#### Strategy 4: Efficient Power Usage (Mobile/Embedded)

**Goal:** Minimize power consumption while maintaining acceptable performance.

**Use Case:** Mobile devices, embedded systems, battery-powered applications

**Encoding:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 \
  -u fast -async 4 -r 2 -x 1 -b 2000 -iGfx
```

**Decoding:**
```bash
sample_decode h264 -i encoded.h264 \
  -async 4 -scaling_mode lowpower -iGfx -nv12
```

**Performance Parameters:**
- `-iGfx` → Integrated GPU (60% lower power than discrete)
- `-u fast` → Good performance/power balance
- `-scaling_mode lowpower` → Efficient scaling algorithms
- `-async 4` → Balanced throughput without excessive buffering

**Expected Results:**
- 30-50% power reduction vs. high-performance settings
- Adequate throughput for streaming at 1080p/30fps

---

### 🎯 QUALITY-FOCUSED Strategies

#### Strategy 1: Maximum Encoding Quality

**Goal:** Achieve best possible output quality regardless of encoding time.

**Use Case:** Archival, on-demand video, streaming services (Netflix-style), professional content

```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -u veryslow -async 4 -r 4 -x 5 -b 8000 -g 256 -la -lad 50 -bref \
  -num_active_P 4 -num_active_BL0 3 -num_active_BL1 3 -hw
```

**Quality Parameters:**
- `-u veryslow` → Best quality preset (15-20% better compression)
- `-b 8000` → High bitrate (visually lossless @ 1080p)
- `-r 4` → Multiple B-frames (better prediction)
- `-x 5` → Maximum reference frames (8-12% better compression)
- `-g 256` → Larger GOP (5-10% better compression efficiency)
- `-la -lad 50` → Lookahead BRC (10-20% better quality consistency)
- `-bref` → B-pyramid reference (5-15% better compression)
- `-num_active_P/BL0/BL1` → Optimized HEVC references

**Expected Results:**
- Visually lossless or near-lossless output
- 5-10x slower encoding than performance mode
- Excellent compression efficiency
- File size 40-50% smaller than fast encoding at same bitrate

---

#### Strategy 2: Streaming Quality Optimization

**Goal:** Balance quality and file size for streaming services.

**Use Case:** YouTube, streaming services, VOD platforms

```bash
sample_encode h265 -i input.yuv -o output.h265 -w 1920 -h 1080 \
  -u slow -async 4 -r 3 -x 3 -b 5000 -g 256 -la -lad 30 -bref -hw
```

**Quality Parameters:**
- `-u slow` → Good quality/speed balance
- `-b 5000` → Streaming-optimized bitrate (high quality @ 1080p)
- `-r 3` → Moderate B-frames (good compression)
- `-x 3` → Balanced reference frames
- `-la -lad 30` → Lookahead BRC for quality consistency
- `-bref` → B-pyramid for better compression

**Expected Results:**
- High-quality output suitable for streaming
- 2-3x slower than fast encoding
- File size suitable for streaming bandwidth

---

#### Strategy 3: Professional Color Grading (Decoding)

**Goal:** Preserve maximum color information for post-processing.

**Use Case:** Color grading, VFX, professional video editing

```bash
sample_decode h264 -i encoded.h264 \
  -y416 -scaling_mode quality -async 4 -hw
```

**Quality Parameters:**
- `-y416` → 16-bit RGBA with full color precision (maximum quality)
- `-scaling_mode quality` → High-quality VPP scaling (20-30% better quality)
- `-async 4` → Balanced throughput for editing workflows
- `-hw` → Hardware decoding for performance

**Expected Results:**
- 16-bit color precision for post-processing
- No color banding or quantization artifacts
- Suitable for professional color grading

---

#### Strategy 4: Balanced Quality/Performance

**Goal:** Good balance of encoding speed, quality, and file size.

**Use Case:** General-purpose encoding, live streaming, website videos

**Encoding:**
```bash
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 \
  -u medium -async 4 -r 2 -x 3 -b 4500 -g 256 -hw
```

**Decoding:**
```bash
sample_decode h264 -i output.h264 \
  -async 4 -gpycopy::on -hw -nv12
```

**Balanced Parameters:**
- `-u medium` → Balanced quality/speed preset
- `-async 4` → Standard throughput (default)
- `-r 2` → Moderate compression
- `-x 3` → Good quality reference frames
- `-b 4500` → Mid-range bitrate
- `-gpycopy::on` → Efficient GPU operations

**Expected Results:**
- Good visual quality
- Reasonable file size
- Acceptable encoding time
- Balanced resource usage

**Goal:** Good balance of speed, quality, and resource usage.

```bash
# Encoding
sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 \
  -u medium -async 4 -r 2 -x 3 -b 4000 -g 256 -hw

# Decoding  
sample_decode h264 -i output.h264 \
  -async 4 -gpucopy::on -hw -nv12
```

**Key Parameters:**
- `-u medium` - Balanced quality/speed
- `-async 4` - Standard throughput
- `-r 2`, `-x 3` - Moderate compression parameters

---

## Performance Measurement

### Encoding Performance Test

```bash
time sample_encode h264 -i input.yuv -o output.h264 -w 1920 -h 1080 -n 1000
```

Measure:
- Total elapsed time
- Output file size
- Frames per second (FPS)

### Decoding Performance Test

```bash
time sample_decode h264 -i encoded.h264 -n 1000
```

Measure:
- Total elapsed time
- Frames per second (FPS)
- Memory consumption (if available)

---

## Notes and Limitations

1. **Codec Support:** Not all parameters work with all codecs. H.264/H.265 have the most options.

2. **Hardware Support:** Parameters like lookahead BRC require specific hardware generations.

3. **Memory Constraints:** On embedded systems, reduce async depth and reference frames.

4. **Content Dependency:** Optimal parameters vary by content type (natural video, screen capture, animation).

5. **API Versions:** Some parameters may only work with specific VPL/MediaSDK versions.

6. **Codec Plugins:** H.265 and VP9 may require separate plugin installation.

---

## References

- Intel Video Processing Library (Intel VPL) Documentation
- Intel Media SDK Samples
- H.264/H.265/VP9 Codec Specifications

