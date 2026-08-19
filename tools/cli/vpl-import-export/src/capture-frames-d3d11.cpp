//==============================================================================
// Copyright Intel Corporation
//
// SPDX-License-Identifier: MIT
//==============================================================================

#include "./capture-frames.h"

mfxStatus CaptureCtxD3D11::CaptureInit(DevCtx *devCtx) {
    m_devCtx       = devCtx;
    m_pD3D11Device = m_devCtx->GetDeviceHandle();

    CComPtr<IDXGIDevice> pDXGIDevice;
    CComPtr<IDXGIAdapter> pDXGIAdapter;
    CComPtr<IDXGIOutput> pDXGIOutput;
    CComPtr<IDXGIOutput1> pDXGIOutput1;

    HRESULT hres = m_pD3D11Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    DXGI_ADAPTER_DESC d = {};
    hres                = pDXGIAdapter->GetDesc(&d);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = pDXGIAdapter->EnumOutputs(0, &pDXGIOutput);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = pDXGIOutput->QueryInterface(__uuidof(pDXGIOutput), (void **)&pDXGIOutput1);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    CComPtr<IDXGIOutputDuplication> pDXGIOutputDupl;
    hres = pDXGIOutput1->DuplicateOutput(m_pD3D11Device, &pDXGIOutputDupl);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    m_pDXGIOutputDupl = pDXGIOutputDupl;

    m_pDXGIOutputDupl->GetDesc(&m_DXGIOutduplDesc);

    hres = m_pD3D11Device->QueryInterface(__uuidof(ID3D11VideoDevice),
                                          reinterpret_cast<void **>(&m_pVideoDevice));
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    CComPtr<ID3D11DeviceContext> deviceContext;
    m_pD3D11Device->GetImmediateContext(&deviceContext);
    hres = deviceContext->QueryInterface(__uuidof(ID3D11VideoContext),
                                         reinterpret_cast<void **>(&m_pVideoContext));
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
    contentDesc.InputFrameFormat                   = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate.Numerator           = 30;
    contentDesc.InputFrameRate.Denominator         = 1;
    contentDesc.InputWidth                         = m_DXGIOutduplDesc.ModeDesc.Width;
    contentDesc.InputHeight                        = m_DXGIOutduplDesc.ModeDesc.Height;
    contentDesc.OutputFrameRate.Numerator          = 30;
    contentDesc.OutputFrameRate.Denominator        = 1;
    contentDesc.OutputWidth                        = m_DXGIOutduplDesc.ModeDesc.Width;
    contentDesc.OutputHeight                       = m_DXGIOutduplDesc.ModeDesc.Height;
    contentDesc.Usage                              = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hres = m_pVideoDevice->CreateVideoProcessorEnumerator(&contentDesc,
                                                           &m_pVideoProcessorEnum);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    hres = m_pVideoDevice->CreateVideoProcessor(m_pVideoProcessorEnum, 0, &m_pVideoProcessor);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width                = m_DXGIOutduplDesc.ModeDesc.Width;
    textureDesc.Height               = m_DXGIOutduplDesc.ModeDesc.Height;
    textureDesc.MipLevels            = 1;
    textureDesc.ArraySize            = 1;
    textureDesc.Format               = DXGI_FORMAT_NV12;
    textureDesc.SampleDesc.Count     = 1;
    textureDesc.Usage                = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags            = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags            = D3D11_RESOURCE_MISC_SHARED;

    hres = m_pD3D11Device->CreateTexture2D(&textureDesc, nullptr, &m_pNV12Texture);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}

mfxStatus CaptureCtxD3D11::CaptureFrame(CComPtr<ID3D11Texture2D> &pTex2D) {
    CComPtr<IDXGIResource> pDXGIResource;
    DXGI_OUTDUPL_FRAME_INFO DXGIOutduplFrameInfo;

    // capture next desktop frame
    HRESULT hres = m_pDXGIOutputDupl->AcquireNextFrame(INFINITE, &DXGIOutduplFrameInfo, &pDXGIResource);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    // map resource to D3D11 texture
    hres = pDXGIResource->QueryInterface(__uuidof(ID3D11Texture2D), (void **)&pTex2D);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    // DBG
    // D3D11_TEXTURE2D_DESC desc = {};
    // pTex2D->GetDesc(&desc);

    return MFX_ERR_NONE;
}

mfxStatus CaptureCtxD3D11::ConvertFrameToNV12(ID3D11Texture2D *pSrc,
                                               CComPtr<ID3D11Texture2D> &pDst) {
    if (!pSrc || !m_pVideoDevice || !m_pVideoContext || !m_pVideoProcessorEnum ||
        !m_pVideoProcessor || !m_pNV12Texture) {
        return MFX_ERR_NULL_PTR;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
    inputDesc.ViewDimension                         = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.MipSlice                    = 0;
    inputDesc.Texture2D.ArraySlice                  = 0;

    CComPtr<ID3D11VideoProcessorInputView> inputView;
    HRESULT hres = m_pVideoDevice->CreateVideoProcessorInputView(
        pSrc, m_pVideoProcessorEnum, &inputDesc, &inputView);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
    outputDesc.ViewDimension                          = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice                     = 0;

    CComPtr<ID3D11VideoProcessorOutputView> outputView;
    hres = m_pVideoDevice->CreateVideoProcessorOutputView(
        m_pNV12Texture, m_pVideoProcessorEnum, &outputDesc, &outputView);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    RECT rect = { 0,
                  0,
                  static_cast<LONG>(m_DXGIOutduplDesc.ModeDesc.Width),
                  static_cast<LONG>(m_DXGIOutduplDesc.ModeDesc.Height) };
    m_pVideoContext->VideoProcessorSetStreamFrameFormat(
        m_pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, 0, TRUE, &rect);
    m_pVideoContext->VideoProcessorSetStreamDestRect(m_pVideoProcessor, 0, TRUE, &rect);
    m_pVideoContext->VideoProcessorSetOutputTargetRect(m_pVideoProcessor, TRUE, &rect);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable                       = TRUE;
    stream.pInputSurface                = inputView;

    hres = m_pVideoContext->VideoProcessorBlt(
        m_pVideoProcessor, outputView, 0, 1, &stream);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    pDst = m_pNV12Texture;
    return MFX_ERR_NONE;
}

mfxStatus CaptureCtxD3D11::ReleaseFrame(void) {
    m_pDXGIOutputDupl->ReleaseFrame();

    return MFX_ERR_NONE;
}
