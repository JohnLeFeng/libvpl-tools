//==============================================================================
// Copyright Intel Corporation
//
// SPDX-License-Identifier: MIT
//==============================================================================

#include "./util.h"

#include "./hw-device.h"

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
#include "./capture-frames.h"
#endif

#ifdef TOOLS_ENABLE_OPENCL
#include "./process-frames-ocl.h"
#endif

struct EncodeCtx {
    DevCtx *devCtx = nullptr;

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    CaptureCtx *cc = nullptr;
#endif

#ifdef TOOLS_ENABLE_OPENCL
    OpenCLCtx *oclCtx = nullptr;
#endif

    mfxMemoryInterface *memoryInterface = nullptr;

    mfxU32 surfaceFlags     = MFX_SURFACE_FLAG_IMPORT_SHARED;
    mfxU32 maxCaptureFrames = MAX_NUM_CAPTURE_FRAMES;

    bool bEnableCapture = false;
    bool bEnableOpenCL  = false;
    bool bEnableTiming  = false;
    bool bEnableSuperResolution = false;
};

static mfxStatus EncodeSurfaceAndWrite(mfxSession session,
                                       mfxFrameSurface1 *surface,
                                       FileInfo *fileInfo,
                                       mfxU32 *frameNum) {
    mfxSyncPoint syncp = {};
    mfxStatus sts = MFX_ERR_NONE;

    do {
        sts = MFXVideoENCODE_EncodeFrameAsync(
            session, nullptr, surface, &fileInfo->bitstream, &syncp);
    } while (sts == MFX_WRN_DEVICE_BUSY);

    if (sts != MFX_ERR_NONE)
        return sts;

    if (!syncp)
        return MFX_ERR_NONE;

    do {
        sts = MFXVideoCORE_SyncOperation(session, syncp, WAIT_100_MILLISECONDS);
    } while (sts == MFX_WRN_IN_EXECUTION);

    if (sts == MFX_ERR_NONE) {
        if (fileInfo->outfile.is_open())
            WriteEncodedStream(fileInfo->bitstream, fileInfo->outfile);
        (*frameNum)++;
    }

    return sts;
}

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
static int ProcessStreamCaptureSuperResolution(mfxSession session,
                                               FileInfo *fileInfo,
                                               EncodeCtx *encCtx,
                                               mfxU32 dbgMask) {
    CaptureCtx *cc = encCtx->cc;
    DevCtx *devCtx = encCtx->devCtx;
    mfxU32 frameNum = 0;
    mfxU32 framesCaptured = 0;
    bool captureFinished = false;

    std::cout << "Capturing desktop with AI super resolution. Hit 'Q' or 'esc' to exit...\n";

    while (!captureFinished) {
        mfxSurfaceHeader *extSurface = nullptr;
#ifdef _WIN32
        CComPtr<ID3D11Texture2D> pTex2D;
        mfxStatus sts = cc->CaptureFrame(pTex2D);
        VERIFY(sts == MFX_ERR_NONE, "ERROR: CaptureFrame");

        mfxSurfaceD3D11Tex2D extSurfD3D11 = {};
        extSurfD3D11.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_D3D11_TEX2D;
        extSurfD3D11.SurfaceInterface.Header.SurfaceFlags = encCtx->surfaceFlags;
        extSurfD3D11.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceD3D11Tex2D);
        extSurfD3D11.texture2D = pTex2D;
        extSurface = reinterpret_cast<mfxSurfaceHeader *>(&extSurfD3D11);
#else
        VASurfaceID vaSurfaceID = VA_INVALID_SURFACE;
        mfxStatus sts = cc->CaptureFrame(&vaSurfaceID);
        VERIFY(sts == MFX_ERR_NONE, "ERROR: CaptureFrame");

        mfxSurfaceVAAPI extSurfVAAPI = {};
        extSurfVAAPI.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_VAAPI;
        extSurfVAAPI.SurfaceInterface.Header.SurfaceFlags = encCtx->surfaceFlags;
        extSurfVAAPI.SurfaceInterface.Header.StructSize = sizeof(mfxSurfaceVAAPI);
        extSurfVAAPI.vaDisplay = devCtx->GetVADisplay();
        extSurfVAAPI.vaSurfaceID = vaSurfaceID;
        extSurface = reinterpret_cast<mfxSurfaceHeader *>(&extSurfVAAPI);
#endif

        mfxFrameSurface1 *vppInput = nullptr;
        sts = encCtx->memoryInterface->ImportFrameSurface(encCtx->memoryInterface,
                                                          MFX_SURFACE_COMPONENT_VPP_INPUT,
                                                          extSurface,
                                                          &vppInput);
        if (sts != MFX_ERR_NONE) {
            cc->ReleaseFrame();
            std::cout << "ERROR: importing capture surface as VPP input, status " << sts
                      << std::endl;
            return -1;
        }

        if (DBG_MASK(dbgMask, DBG_MASK_ACTUAL_SURFACE_FLAGS))
            printf("[% 5d] Actual VPP input import mode = %s\n",
                   framesCaptured,
                   DebugGetStringSurfaceFlags(extSurface->SurfaceFlags));

        if (extSurface->SurfaceFlags != encCtx->surfaceFlags) {
            vppInput->FrameInterface->Release(vppInput);
            cc->ReleaseFrame();
            std::cout << "ERROR: runtime changed the requested VPP input import mode" << std::endl;
            return -1;
        }

        mfxFrameSurface1 *vppOutput = nullptr;
        sts = MFXMemory_GetSurfaceForVPPOut(session, &vppOutput);
        if (sts != MFX_ERR_NONE) {
            vppInput->FrameInterface->Release(vppInput);
            cc->ReleaseFrame();
            std::cout << "ERROR: acquiring VPP output surface, status " << sts << std::endl;
            return -1;
        }

        mfxSyncPoint vppSyncp = {};
        do {
            sts = MFXVideoVPP_RunFrameVPPAsync(
                session, vppInput, vppOutput, nullptr, &vppSyncp);
        } while (sts == MFX_WRN_DEVICE_BUSY);

        mfxStatus releaseSts = vppInput->FrameInterface->Release(vppInput);
        if (sts != MFX_ERR_NONE || releaseSts != MFX_ERR_NONE) {
            vppOutput->FrameInterface->Release(vppOutput);
            cc->ReleaseFrame();
            std::cout << "ERROR: running AI super resolution VPP, status " << sts << std::endl;
            return -1;
        }

        do {
            sts = MFXVideoCORE_SyncOperation(session, vppSyncp, WAIT_100_MILLISECONDS);
        } while (sts == MFX_WRN_IN_EXECUTION);

        releaseSts = cc->ReleaseFrame();
        if (sts != MFX_ERR_NONE || releaseSts != MFX_ERR_NONE) {
            vppOutput->FrameInterface->Release(vppOutput);
            std::cout << "ERROR: synchronizing AI super resolution VPP, status " << sts
                      << std::endl;
            return -1;
        }

        sts = EncodeSurfaceAndWrite(session, vppOutput, fileInfo, &frameNum);
        releaseSts = vppOutput->FrameInterface->Release(vppOutput);
        if ((sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) || releaseSts != MFX_ERR_NONE) {
            std::cout << "ERROR: encoding super resolution output, status " << sts << std::endl;
            return -1;
        }

        framesCaptured++;
        captureFinished = framesCaptured >= encCtx->maxCaptureFrames || CheckKB_Quit();
    }

    bool vppDrained = false;
    while (!vppDrained) {
        mfxFrameSurface1 *vppOutput = nullptr;
        mfxStatus sts = MFXMemory_GetSurfaceForVPPOut(session, &vppOutput);
        VERIFY(sts == MFX_ERR_NONE, "ERROR: acquiring VPP drain surface");

        mfxSyncPoint vppSyncp = {};
        do {
            sts = MFXVideoVPP_RunFrameVPPAsync(
                session, nullptr, vppOutput, nullptr, &vppSyncp);
        } while (sts == MFX_WRN_DEVICE_BUSY);

        if (sts == MFX_ERR_MORE_DATA) {
            vppOutput->FrameInterface->Release(vppOutput);
            vppDrained = true;
            continue;
        }
        if (sts != MFX_ERR_NONE) {
            vppOutput->FrameInterface->Release(vppOutput);
            std::cout << "ERROR: draining AI super resolution VPP, status " << sts << std::endl;
            return -1;
        }

        do {
            sts = MFXVideoCORE_SyncOperation(session, vppSyncp, WAIT_100_MILLISECONDS);
        } while (sts == MFX_WRN_IN_EXECUTION);

        if (sts == MFX_ERR_NONE)
            sts = EncodeSurfaceAndWrite(session, vppOutput, fileInfo, &frameNum);

        mfxStatus releaseSts = vppOutput->FrameInterface->Release(vppOutput);
        if ((sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA) || releaseSts != MFX_ERR_NONE) {
            std::cout << "ERROR: encoding drained VPP output, status " << sts << std::endl;
            return -1;
        }
    }

    while (true) {
        mfxStatus sts = EncodeSurfaceAndWrite(session, nullptr, fileInfo, &frameNum);
        if (sts == MFX_ERR_MORE_DATA)
            break;
        if (sts != MFX_ERR_NONE) {
            std::cout << "ERROR: draining encoder, status " << sts << std::endl;
            return -1;
        }
    }

    std::cout << "Encoded " << frameNum << " frames\n\n";
    return 0;
}
#endif

static int ProcessStreamEncode(mfxSession session, FrameInfo *frameInfo, FileInfo *fileInfo, EncodeCtx *encCtx, mfxU32 dbgMask) {
    bool bIsStillGoing    = true;
    bool bIsDrainingEnc   = false;
    mfxU32 frameNum       = 0;
    mfxU32 framesCaptured = 0;

    mfxStatus sts = MFX_ERR_NONE;

    std::vector<mfxU8> cpuBufY;
    std::vector<mfxU8> cpuBufUV;

    if (encCtx->bEnableCapture && encCtx->bEnableOpenCL)
        VERIFY(0, "ERROR: OpenCL not compatible with screen capture");

    DevCtx *devCtx = encCtx->devCtx;

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    CaptureCtx *cc = encCtx->cc;
#endif

#ifdef TOOLS_ENABLE_OPENCL
    OpenCLCtx *oclCtx = encCtx->oclCtx;
    cl_int clError    = CL_SUCCESS;
#endif

    if (encCtx->bEnableCapture)
        std::cout << "Capturing desktop. Hit 'Q' or 'esc' to exit...\n";
    else if (encCtx->bEnableOpenCL)
        std::cout << "Encoding to file with OpenCL processing. Hit 'Q' or 'esc' to exit...\n";
    else
        std::cout << "Encoding to file. Hit 'Q' or 'esc' to exit...\n";

    VPL_TOTAL_TIME_INIT(totalStreamTime);
    VPL_TOTAL_TIME_INIT(totalImportTime);
    VPL_TOTAL_TIME_INIT(totalMapTime);

    VPL_TOTAL_TIME_START(totalStreamTime);

    // main processing loop
    while (bIsStillGoing == true) {
        // Import will return a surface for us
        mfxFrameSurface1 *pmfxEncSurface = NULL;

        if (!bIsDrainingEnc) {
#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
            if (encCtx->bEnableCapture) {
                mfxSurfaceHeader *extSurface = nullptr;
#ifdef _WIN32
                // capture desktop as D3D11 texture with Windows capture API
                CComPtr<ID3D11Texture2D> pTex2D;
                sts = cc->CaptureFrame(pTex2D);
                VERIFY(sts == MFX_ERR_NONE, "ERROR: CaptureFrame");

                // import D3D11 surface into mfxFrameSurface1
                // set header.surfaceType, device, and texture2D, all other fields should be empty
                mfxSurfaceD3D11Tex2D extSurfD3D11                 = {};
                extSurfD3D11.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_D3D11_TEX2D;
                extSurfD3D11.SurfaceInterface.Header.SurfaceFlags = encCtx->surfaceFlags;
                extSurfD3D11.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceD3D11Tex2D);

                extSurfD3D11.texture2D = pTex2D;

                extSurface = reinterpret_cast<mfxSurfaceHeader *>(&extSurfD3D11);
#else
                // capture desktop as VAAPI surface
                VASurfaceID vaSurfaceID;
                sts = cc->CaptureFrame(&vaSurfaceID);
                VERIFY(sts == MFX_ERR_NONE, "ERROR: CaptureFrame");

                mfxSurfaceVAAPI extSurfVAAPI                      = {};
                extSurfVAAPI.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_VAAPI;
                extSurfVAAPI.SurfaceInterface.Header.SurfaceFlags = encCtx->surfaceFlags;
                extSurfVAAPI.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceVAAPI);

                extSurfVAAPI.vaDisplay   = devCtx->GetVADisplay();
                extSurfVAAPI.vaSurfaceID = vaSurfaceID;

                extSurface = reinterpret_cast<mfxSurfaceHeader *>(&extSurfVAAPI);
#endif
                VPL_TOTAL_TIME_START(totalImportTime);

                sts = encCtx->memoryInterface->ImportFrameSurface(encCtx->memoryInterface, MFX_SURFACE_COMPONENT_ENCODE, extSurface, &pmfxEncSurface);
                VERIFY(MFX_ERR_NONE == sts, "ERROR: ImportFrameSurface");

                VPL_TOTAL_TIME_STOP(totalImportTime);

                // optional (debug) - print actual surface flags used by ImportFrameSurface
                if (DBG_MASK(dbgMask, DBG_MASK_ACTUAL_SURFACE_FLAGS))
                    printf("[% 5d] Actual import mode = %s\n", frameNum, DebugGetStringSurfaceFlags(extSurface->SurfaceFlags));

                framesCaptured++;
            }
#endif

#ifdef TOOLS_ENABLE_OPENCL
            if (encCtx->bEnableOpenCL) {
                // read data into an OpenCL image (copy from host to device)
                CPUFrameInfo_NV12 cpuFrameInfo = {};
                mfxU16 height                  = frameInfo->height;
                mfxU16 width                   = frameInfo->width;

                if (cpuBufY.size() != (width * height))
                    cpuBufY.resize(width * height);
                if (cpuBufUV.size() != (width * height / 2))
                    cpuBufUV.resize(width * height / 2);

                cpuFrameInfo.height = height;
                cpuFrameInfo.width  = width;
                cpuFrameInfo.pitch  = width;
                cpuFrameInfo.Y      = cpuBufY.data();
                cpuFrameInfo.UV     = cpuBufUV.data();

                // read frame from infile into system memory
                sts = ReadRawFrameCPU_NV12(cpuFrameInfo, fileInfo->infile);

                if (sts == MFX_ERR_NONE) {
                    cl_mem in_mem_Y   = nullptr;
                    cl_mem in_mem_UV  = nullptr;
                    cl_mem out_mem_Y  = nullptr;
                    cl_mem out_mem_UV = nullptr;

                    // get writable OCL surface to be the the input from OCL kernel
                    clError = oclCtx->GetOCLInputSurface(&in_mem_Y, &in_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: GetOCLInputSurface");

                    // measure the time to copy from sysmem to OCL surface
                    VPL_TOTAL_TIME_START(totalMapTime);

                    // copy to system memory for saving
                    clError = oclCtx->CopySurfaceSystemToOCL(&cpuFrameInfo, in_mem_Y, in_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: CopySurfaceSystemToOCL");

                    VPL_TOTAL_TIME_STOP(totalMapTime);

                    // get writable OCL surface to be the the output from OCL kernel
                    clError = oclCtx->GetOCLOutputSurface(&out_mem_Y, &out_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: GetOCLOutputSurface");

                    // run OpenCL kernels on input surface
                    clError = oclCtx->OpenCLProcessSurface(in_mem_Y, in_mem_UV, out_mem_Y, out_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: OpenCLProcessSurface");

                    // set header and ocl_context, ocl_flags is optional, all other fields should be empty
                    mfxSurfaceOpenCLImg2D extSurfOCL               = {};
                    extSurfOCL.SurfaceInterface.Header.SurfaceType = MFX_SURFACE_TYPE_OPENCL_IMG2D;

                    // Specify either SHARED or COPY as permitted import modes, but we expect that runtime only supports copy mode
                    //   because OCL driver does not support zero-copy mapping yet.
                    // After calling ImportFrameSurface, we can check extSurfOCL.SurfaceInterface.Header.SurfaceFlags to see what
                    //   import mode was actually used by the RT.
                    extSurfOCL.SurfaceInterface.Header.SurfaceFlags = (MFX_SURFACE_FLAG_IMPORT_SHARED | MFX_SURFACE_FLAG_IMPORT_COPY);
                    extSurfOCL.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceOpenCLImg2D);

                    extSurfOCL.ocl_context       = oclCtx->GetOpenCLContext();
                    extSurfOCL.ocl_command_queue = oclCtx->GetOpenCLCommandQueue();

                    extSurfOCL.ocl_image_num = 2;
                    extSurfOCL.ocl_image[0]  = out_mem_Y;
                    extSurfOCL.ocl_image[1]  = out_mem_UV;

                    mfxSurfaceHeader *extSurface = reinterpret_cast<mfxSurfaceHeader *>(&extSurfOCL);

                    VPL_TOTAL_TIME_START(totalImportTime);

                    sts = encCtx->memoryInterface->ImportFrameSurface(encCtx->memoryInterface, MFX_SURFACE_COMPONENT_ENCODE, extSurface, &pmfxEncSurface);
                    VERIFY(MFX_ERR_NONE == sts, "ERROR: ImportFrameSurface (OpenCL)");

                    VPL_TOTAL_TIME_STOP(totalImportTime);

                    // optional (debug) - print actual surface flags used by ImportFrameSurface
                    if (DBG_MASK(dbgMask, DBG_MASK_ACTUAL_SURFACE_FLAGS))
                        printf("[% 5d] Actual import mode = %s\n", frameNum, DebugGetStringSurfaceFlags(extSurface->SurfaceFlags));

                    // release OCL surfaces
                    clError = oclCtx->ReleaseOCLSurface(in_mem_Y, in_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: ReleaseOCLSurface");

                    clError = oclCtx->ReleaseOCLSurface(out_mem_Y, out_mem_UV);
                    VERIFY(CL_SUCCESS == clError, "ERROR: ReleaseOCLSurface");
                }
                else {
                    bIsDrainingEnc = true; // end of input file, start draining encoder
                }
            }
#endif

            if (!encCtx->bEnableCapture && !encCtx->bEnableOpenCL) {
                // not using import, so get a surface from RT
                sts = MFXMemory_GetSurfaceForEncode(session, &pmfxEncSurface);
                if (sts != MFX_ERR_NONE)
                    return sts;

                // read data directly into encode frame (map to CPU)
                sts = pmfxEncSurface->FrameInterface->Map(pmfxEncSurface, MFX_MAP_WRITE);
                VERIFY(sts == MFX_ERR_NONE, "ERROR: Map");

                sts = ReadRawFrame(pmfxEncSurface, fileInfo->infile);
                if (sts != MFX_ERR_NONE)
                    bIsDrainingEnc = true; // end of input file, start draining encoder

                // time the Unmap, i.e. copying from sysmem to GPU
                // NOTE - ID3D11DeviceContext::Unmap just queues up the command and returns (non-blocking), so this
                //   won't capture the actual data transfer time
                VPL_TOTAL_TIME_START(totalMapTime);

                sts = pmfxEncSurface->FrameInterface->Unmap(pmfxEncSurface);
                VERIFY(sts == MFX_ERR_NONE, "ERROR: Unmap");

                VPL_TOTAL_TIME_STOP(totalMapTime);
            }

            if (DBG_MASK(dbgMask, DBG_MASK_NATIVE_SURFACE_DESC))
                DebugDumpNativeSurfaceDesc(pmfxEncSurface); // safe to call if pmfxEncSurface is null

        } // end of (!bIsDrainingEnc) section

        // encode next frame
        mfxSyncPoint syncp = {};
        mfxStatus stsEnc   = MFXVideoENCODE_EncodeFrameAsync(session, NULL, (bIsDrainingEnc ? NULL : pmfxEncSurface), &fileInfo->bitstream, &syncp);

        if (pmfxEncSurface) {
            sts = pmfxEncSurface->FrameInterface->Release(pmfxEncSurface); // use different error code
            VERIFY(sts == MFX_ERR_NONE, "ERROR: Release");
        }

        switch (stsEnc) {
            case MFX_ERR_NONE:
                if (syncp) {
                    do {
                        sts = MFXVideoCORE_SyncOperation(session, syncp, WAIT_100_MILLISECONDS);
                        if (MFX_ERR_NONE == sts && fileInfo->outfile.is_open()) {
                            WriteEncodedStream(fileInfo->bitstream, fileInfo->outfile);
                        }
                        frameNum++;
                    } while (sts == MFX_WRN_IN_EXECUTION);
                }
                break;
            case MFX_ERR_MORE_DATA:
                if (bIsDrainingEnc == true)
                    bIsStillGoing = false;
                break;
            case MFX_WRN_DEVICE_BUSY:
                // wait and try again
                break;
            default:
                // unknown error
                bIsStillGoing = false;
                break;
        }

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
        if (encCtx->bEnableCapture) {
            cc->ReleaseFrame();
        }
#endif

        // check console for keystrokes
        // break by typing 'Q' or 'esc'
        if (CheckKB_Quit())
            bIsStillGoing = false;

        if (encCtx->bEnableCapture && (framesCaptured >= encCtx->maxCaptureFrames))
            bIsDrainingEnc = true;
    }

    VPL_TOTAL_TIME_STOP(totalStreamTime);

    std::cout << "Encoded " << frameNum << " frames\n\n";

    if (encCtx->bEnableTiming) {
        // print timing for overall stream processing
        mfxF32 fStreamTime;
        mfxU64 uStreamCount;
        VPL_TOTAL_TIME_CHECK(totalStreamTime, fStreamTime, uStreamCount);
        if (uStreamCount)
            printf("Stream time:  FPS = % 8.2f   TOTAL = % 9.3f msec   NUM FRAMES = % 5d\n\n", frameNum * 1000.0f / fStreamTime, fStreamTime, frameNum);

        // print timing for export operations
        mfxF32 fImportTime;
        mfxU64 uImportCount;
        VPL_TOTAL_TIME_CHECK(totalImportTime, fImportTime, uImportCount);
        if (uImportCount)
            printf("Import time:  COUNT = % 6lld   TOTAL = % 9.3f msec   PER FRAME = % 7.3f msec\n", uImportCount, fImportTime, fImportTime / uImportCount);

        // print timing for map to system memory
        mfxF32 fMapTime;
        mfxU64 uMapCount;
        VPL_TOTAL_TIME_CHECK(totalMapTime, fMapTime, uMapCount);
        if (uMapCount)
            printf("Map time:     COUNT = % 6lld   TOTAL = % 9.3f msec   PER FRAME = % 7.3f msec\n", uMapCount, fMapTime, fMapTime / uMapCount);
    }

    return 0;
}

int RunEncode(Params *params, FileInfo *fileInfo) {
    mfxStatus sts = MFX_ERR_NONE;

    // validate configuration
#ifndef TOOLS_ENABLE_SCREEN_CAPTURE
    if (params->testMode == TEST_MODE_CAPTURE)
        VERIFY(0, "ERROR: rebuild with TOOLS_ENABLE_SCREEN_CAPTURE set to ON\n");
#endif

#ifndef TOOLS_ENABLE_OPENCL
    if (params->bEnableOpenCL)
        VERIFY(0, "ERROR: rebuild with TOOLS_ENABLE_OPENCL set to ON\n");
#endif

    // initialize encode context
    EncodeCtx encCtx        = {};
    encCtx.bEnableTiming    = params->bEnableTiming;
    encCtx.maxCaptureFrames = params->maxCaptureFrames;
    encCtx.bEnableSuperResolution = params->bEnableSuperResolution;

    if (params->surfaceMode == SURFACE_MODE_SHARED)
        encCtx.surfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
    else if (params->surfaceMode == SURFACE_MODE_COPY)
        encCtx.surfaceFlags = MFX_SURFACE_FLAG_IMPORT_COPY;

    // create HW device context
    // automatically released when devCtx goes out of scope (make sure this happens after closing session)
    DevCtx devCtx            = {};
    mfxHandleType handleType = {};
    mfxHDL handle            = nullptr;

    sts = devCtx.InitDevice(0, &handleType, &handle);
    VERIFY((MFX_ERR_NONE == sts) && (handle != nullptr), "ERROR: InitDevice");
    encCtx.devCtx = &devCtx;

    // specify required capabilities for session creation
    std::list<SurfaceCaps> surfaceCapsList;

    // encode - native type
    SurfaceCaps scEnc = {};
    scEnc.SurfaceType =
#ifdef _WIN32
        MFX_SURFACE_TYPE_D3D11_TEX2D;
#else
        MFX_SURFACE_TYPE_VAAPI;
#endif
    scEnc.SurfaceComponent = params->bEnableSuperResolution
                                 ? MFX_SURFACE_COMPONENT_VPP_INPUT
                                 : MFX_SURFACE_COMPONENT_ENCODE;
    scEnc.SurfaceFlags     = encCtx.surfaceFlags;
    surfaceCapsList.push_back(scEnc);

    // encode - OpenCL (if enabled)
    if (params->bEnableOpenCL) {
        SurfaceCaps scOCL      = {};
        scOCL.SurfaceType      = MFX_SURFACE_TYPE_OPENCL_IMG2D;
        scOCL.SurfaceComponent = MFX_SURFACE_COMPONENT_ENCODE;
        scOCL.SurfaceFlags     = MFX_SURFACE_FLAG_IMPORT_COPY; // require COPY, we will permit both SHARED and COPY when used
        surfaceCapsList.push_back(scOCL);
    }

    // initialize session
    VPLSession vplSession = {};
    sts                   = vplSession.Open(&surfaceCapsList);
    VERIFY(MFX_ERR_NONE == sts, "ERROR: unable to create session");

    // pass device handle to runtime
    sts = MFXVideoCORE_SetHandle(vplSession.GetSession(), handleType, handle);
    VERIFY(MFX_ERR_NONE == sts, "ERROR: SetHandle");

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    // initialize desktop capture
    CaptureCtx cc = {};
    if (params->testMode == TEST_MODE_CAPTURE) {
        sts = cc.CaptureInit(&devCtx);
        VERIFY(MFX_ERR_NONE == sts, "ERROR: CaptureInit");
        encCtx.bEnableCapture = true;
    }
    encCtx.cc = &cc;
#endif

#ifdef TOOLS_ENABLE_OPENCL
    // initialize OpenCL post-processing
    OpenCLCtx oclCtx = {};
    if (params->bEnableOpenCL && !params->openCLfileName.empty()) {
        sts = oclCtx.OpenCLInit(handleType, handle, params->srcWidth, params->srcHeight, params->openCLfileName);
        VERIFY(MFX_ERR_NONE == sts, "ERROR: initializing OpenCL");
        encCtx.bEnableOpenCL = true;
    }
    encCtx.oclCtx = &oclCtx;
#endif

    // init output bitstream
    std::vector<mfxU8 *> outbuf(BITSTREAM_BUFFER_SIZE);
    fileInfo->bitstream.MaxLength = static_cast<mfxU32>(outbuf.size());
    fileInfo->bitstream.Data      = reinterpret_cast<mfxU8 *>(outbuf.data());
    fileInfo->bitstream.CodecId   = params->codecId;

    // init encode parameters
    mfxVideoParam mfxEncParams         = {};
    mfxEncParams.mfx.CodecId           = params->codecId;
    mfxEncParams.mfx.TargetUsage       = MFX_TARGETUSAGE_BALANCED;
    mfxEncParams.mfx.TargetKbps        = 2000;
    mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
#ifdef _WIN32
    mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_ON;
#else
    mfxEncParams.mfx.LowPower = MFX_CODINGOPTION_OFF;
#endif
    mfxEncParams.mfx.FrameInfo.FrameRateExtN = 30;
    mfxEncParams.mfx.FrameInfo.FrameRateExtD = 1;
    mfxEncParams.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
    mfxEncParams.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;

    mfxVideoParam mfxVPPParams = {};
    mfxExtVPPAISuperResolution srConfig = {};
    mfxExtBuffer *vppExtBuffers[] = { reinterpret_cast<mfxExtBuffer *>(&srConfig) };

#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    if (params->testMode == TEST_MODE_CAPTURE) {
        mfxU16 w = 0, h = 0;
        cc.GetCaptureResolution(w, h);

        if ((encCtx.surfaceFlags == MFX_SURFACE_FLAG_IMPORT_SHARED) && ((w != ALIGN8(w)) || (h != ALIGN8(h)))) {
            std::cout << "ERROR: desktop resolution " << w << "x" << h << " is not aligned to 8 pixels. SURFACE_MODE_SHARED is not supported. Run again with '-mode copy'"
                      << std::endl;
            return -1;
        }

        if (params->bEnableSuperResolution) {
            if (!ValidateSuperResolutionDimensions(w, h, params->dstWidth, params->dstHeight)) {
                std::cout << "ERROR: super resolution destination " << params->dstWidth << "x"
                          << params->dstHeight << " must be larger than desktop resolution " << w
                          << "x" << h << " and preserve its aspect ratio" << std::endl;
                return -1;
            }

            mfxVPPParams.vpp.In.CropW         = w;
            mfxVPPParams.vpp.In.CropH         = h;
            mfxVPPParams.vpp.In.Width         = ALIGN8(w);
            mfxVPPParams.vpp.In.Height        = ALIGN8(h);
            mfxVPPParams.vpp.In.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
            mfxVPPParams.vpp.In.FrameRateExtN = 30;
            mfxVPPParams.vpp.In.FrameRateExtD = 1;
#ifdef _WIN32
            mfxVPPParams.vpp.In.FourCC       = MFX_FOURCC_RGB4;
            mfxVPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
#else
            mfxVPPParams.vpp.In.FourCC       = MFX_FOURCC_NV12;
            mfxVPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
#endif

            mfxVPPParams.vpp.Out.CropW         = params->dstWidth;
            mfxVPPParams.vpp.Out.CropH         = params->dstHeight;
            mfxVPPParams.vpp.Out.Width         = ALIGN8(params->dstWidth);
            mfxVPPParams.vpp.Out.Height        = ALIGN8(params->dstHeight);
            mfxVPPParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
            mfxVPPParams.vpp.Out.FrameRateExtN = 30;
            mfxVPPParams.vpp.Out.FrameRateExtD = 1;
            mfxVPPParams.vpp.Out.FourCC         = MFX_FOURCC_NV12;
            mfxVPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;

            mfxVPPParams.IOPattern  = MFX_IOPATTERN_IN_VIDEO_MEMORY |
                                     MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            mfxVPPParams.AsyncDepth = 1;

            srConfig.Header.BufferId = MFX_EXTBUFF_VPP_AI_SUPER_RESOLUTION;
            srConfig.Header.BufferSz = sizeof(srConfig);
            srConfig.SRMode           = MFX_AI_SUPER_RESOLUTION_MODE_DEFAULT;
            srConfig.SRAlgorithm      = params->srAlgorithm;
            mfxVPPParams.ExtParam     = vppExtBuffers;
            mfxVPPParams.NumExtParam  = 1;

            mfxEncParams.mfx.FrameInfo = mfxVPPParams.vpp.Out;
        }
        else {
            mfxEncParams.mfx.FrameInfo.CropW = w;
            mfxEncParams.mfx.FrameInfo.CropH = h;
#ifdef _WIN32
            mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_RGB4;
            mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
#endif
        }
    }
    else {
#endif
        mfxEncParams.mfx.FrameInfo.CropW = params->srcWidth;
        mfxEncParams.mfx.FrameInfo.CropH = params->srcHeight;
#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    }
#endif

    mfxEncParams.mfx.FrameInfo.Width  = ALIGN8(mfxEncParams.mfx.FrameInfo.CropW);
    mfxEncParams.mfx.FrameInfo.Height = ALIGN8(mfxEncParams.mfx.FrameInfo.CropH);

    mfxEncParams.IOPattern  = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    mfxEncParams.AsyncDepth = 1;

    if (params->bEnableSuperResolution) {
        sts = MFXVideoVPP_Query(vplSession.GetSession(), &mfxVPPParams, &mfxVPPParams);
        VERIFY(MFX_ERR_NONE == sts, "ERROR: AI super resolution is not supported");

        sts = MFXVideoVPP_Init(vplSession.GetSession(), &mfxVPPParams);
        VERIFY(MFX_ERR_NONE == sts, "ERROR: initializing AI super resolution VPP");
    }

    // disable frame reordering for testing shared surface import (e.g. Windows capture API)
    // to support frame reordering, the application should maintain a pool of shared surfaces
    //   (QueryIOSurf) and either check the Data.Locked field or use built-in surface refcount
    //   mechanisms (where supported) to track when encoder has released each surface
    if (!params->bEnableSuperResolution &&
        encCtx.surfaceFlags == MFX_SURFACE_FLAG_IMPORT_SHARED)
        mfxEncParams.mfx.GopRefDist = 1;

    // query encoder
    sts = MFXVideoENCODE_Query(vplSession.GetSession(), &mfxEncParams, &mfxEncParams);
    VERIFY(MFX_ERR_NONE <= sts, "ERROR: query Encode");
    if (sts)
        std::cout << "Warning: MFXVideoENCODE_Query returned " << sts << std::endl;

    // init encoder
    sts = MFXVideoENCODE_Init(vplSession.GetSession(), &mfxEncParams);
    VERIFY(MFX_ERR_NONE == sts, "ERROR: initializing Encode");

    FrameInfo frameInfo = {};
    frameInfo.height    = mfxEncParams.mfx.FrameInfo.Height;
    frameInfo.width     = mfxEncParams.mfx.FrameInfo.Width;
    frameInfo.pitch     = mfxEncParams.mfx.FrameInfo.Width;

    // Determine codec name for display
    const char *codecName = "Unknown";
    switch (params->codecId) {
        case MFX_CODEC_AVC:
            codecName = "AVC/H.264";
            break;
        case MFX_CODEC_HEVC:
            codecName = "HEVC/H.265";
            break;
        case MFX_CODEC_AV1:
            codecName = "AV1";
            break;
    }

    std::cout << "Running: Encode (" << codecName << ")" << std::endl;
    std::cout << "  infile  = " << (params->testMode == TEST_MODE_CAPTURE ? "desktop capture" : params->infileName) << std::endl;
    std::cout << "  input colorspace = " << FourCCToString(mfxEncParams.mfx.FrameInfo.FourCC) << std::endl;
    std::cout << "  outfile = " << (params->outfileName.empty() ? "none" : params->outfileName) << std::endl;
    std::cout << "  output resolution = " << frameInfo.width << "x" << frameInfo.height << std::endl;
    if (params->bEnableSuperResolution) {
        std::cout << "  AI super resolution algorithm = " << params->srAlgorithm << std::endl;
        std::cout << "  VPP input import mode = "
                  << DebugGetStringSurfaceFlags(encCtx.surfaceFlags) << std::endl;
    }
    std::cout << std::endl << std::endl;

    // get interface for ImportFrameSurface
    if (encCtx.bEnableCapture || encCtx.bEnableOpenCL) {
        mfxMemoryInterface *iface = nullptr;
        sts                       = MFXGetMemoryInterface(vplSession.GetSession(), &iface);
        VERIFY((MFX_ERR_NONE == sts) && (iface != nullptr), "ERROR: MFXGetMemoryInterface");
        encCtx.memoryInterface = iface;
    }

    // run main processing loop
    int err = 0;
#ifdef TOOLS_ENABLE_SCREEN_CAPTURE
    if (params->bEnableSuperResolution)
        err = ProcessStreamCaptureSuperResolution(
            vplSession.GetSession(), fileInfo, &encCtx, params->dbgMask);
    else
#endif
        err = ProcessStreamEncode(
            vplSession.GetSession(), &frameInfo, fileInfo, &encCtx, params->dbgMask);

    if (err) {
        std::cout << "ERROR: ProcessStream() returned " << err << std::endl;
        if (params->bEnableSuperResolution) {
            MFXVideoENCODE_Close(vplSession.GetSession());
            MFXVideoVPP_Close(vplSession.GetSession());
        }
        return err;
    }

    if (params->bEnableSuperResolution) {
        mfxStatus encCloseSts = MFXVideoENCODE_Close(vplSession.GetSession());
        mfxStatus vppCloseSts = MFXVideoVPP_Close(vplSession.GetSession());
        VERIFY(MFX_ERR_NONE == encCloseSts, "ERROR: closing encoder");
        VERIFY(MFX_ERR_NONE == vppCloseSts, "ERROR: closing AI super resolution VPP");
    }

    // dtor will call Close() automatically when it goes out of scope (e.g. error causes early exit).
    // But it's also okay to call it explicitly here - it won't try to teardown twice.
    vplSession.Close();

    return 0;
}
