#include "Encoder.h"
#include "Precomp.h"
#include "Util.h"
#include "qoi.h"
#include <exception>
#include <fstream>

using namespace Microsoft::WRL;
//using namespace WEX::TestExecution;

void *SaveToPipe(HANDLE pipe, ID3D11Texture2D* Texture, LPOVERLAPPED state, std::wostream& debug_log)
{
    //(void)FileName;
    HRESULT hr;

    // First verify that we can map the texture
    D3D11_TEXTURE2D_DESC desc;
    Texture->GetDesc(&desc);

    // translate texture format to QOI format. We support only BGRA and ARGB.
    qoi_source_format qoi_fmt;
    switch (desc.Format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        qoi_fmt = QOI_SRC_RGBA;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        qoi_fmt = QOI_SRC_BGRA;
        break;
    default:
        debug_log << "Unsupported DXGI_FORMAT: %d. Only RGBA and BGRA are supported\n";
        return NULL;
    }

    // Get the device context
    ComPtr<ID3D11Device> d3dDevice;
    Texture->GetDevice(&d3dDevice);
    ComPtr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(&d3dContext);

    // map the texture
    ComPtr<ID3D11Texture2D> mappedTexture;
    D3D11_MAPPED_SUBRESOURCE mapInfo;
    mapInfo.RowPitch;
    hr = d3dContext->Map(
        Texture,
        0,  // Subresource
        D3D11_MAP_READ,
        0,  // MapFlags
        &mapInfo);

    //debug_log << "Map attempted" << std::endl;

    if (FAILED(hr)) {
        // If we failed to map the texture, copy it to a staging resource
        if (hr == E_INVALIDARG) {
            D3D11_TEXTURE2D_DESC desc2;
            desc2.Width = desc.Width;
            desc2.Height = desc.Height;
            desc2.MipLevels = desc.MipLevels;
            desc2.ArraySize = desc.ArraySize;
            desc2.Format = desc.Format;
            desc2.SampleDesc = desc.SampleDesc;
            desc2.Usage = D3D11_USAGE_STAGING;
            desc2.BindFlags = 0;
            desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc2.MiscFlags = 0;

            ComPtr<ID3D11Texture2D> stagingTexture;
            hr = d3dDevice->CreateTexture2D(&desc2, nullptr, &stagingTexture);
            if (FAILED(hr)) {
                debug_log << "Failed to create staging texture\n";
                return NULL;
            }

            // copy the texture to a staging resource
            d3dContext->CopyResource(stagingTexture.Get(), Texture);

            // now, map the staging resource
            hr = d3dContext->Map(
                stagingTexture.Get(),
                0,
                D3D11_MAP_READ,
                0,
                &mapInfo);
            if (FAILED(hr)) {
                debug_log << "Failed to map staging texture\n";
                return NULL;
            }

            mappedTexture = std::move(stagingTexture);
        }
        else {
            debug_log << "Failed to map texture.\n";
            return NULL;
        }
    }
    else {
        mappedTexture = Texture;
    }
    auto unmapResource = Finally([&] {
        d3dContext->Unmap(mappedTexture.Get(), 0);
        });

    //debug_log << "Begin encode" << std::endl;

    // AWKWARD: desc.Height * mapInfo.RowPitch - how into QOI?
    qoi_desc qdesc = {};
    qdesc.width = desc.Width;
    qdesc.height = desc.Height;
    qdesc.channels = 4;
    qdesc.colorspace = QOI_SRGB;
    int out_len = 0;

    void* data = qoi_encode(mapInfo.pData, &qdesc, &out_len, qoi_fmt);

    //debug_log << "Encode done" << std::endl;
    if (out_len > FRAME_SIZE_MAX) {
        debug_log << "Compressed frame exceeded 32MB limit\n";
        free(data);
        return NULL;
    }
    if (data) {
        //assert(!state->hEvent);
        memset(state, 0, sizeof *state);
        //state->hEvent = data;
        //debug_log << "Before write" << std::endl;
        //BOOL writeOk = WriteFileEx(pipe, data, out_len, state, /*IoCompletionHandler*/NULL);
        BOOL writeOk = WriteFile(pipe, data, out_len, NULL, state);
        if (!writeOk) {
            if (GetLastError() != ERROR_IO_PENDING) {
                debug_log << "Write not ok" << std::endl;
                debug_log.flush();
            }
        }
        else {
            // this was fast and synchronous - free and return NULL instead?
        }
       
        /*/std::ofstream file;
        file.open(FileName, std::ios_base::out | std::ios_base::binary);
        file.write((char*)data, out_len);*/
    }
    //free(data);
    return data;
}