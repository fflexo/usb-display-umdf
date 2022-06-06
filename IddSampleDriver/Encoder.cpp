#include "Encoder.h"
#include "Precomp.h"
#include "Util.h"
#include "qoi.h"
#include <exception>
#include <fstream>

using namespace Microsoft::WRL;
//using namespace WEX::TestExecution;

void SaveToPipe(HANDLE pipe, ID3D11Texture2D* Texture)
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
        throw std::exception("Unsupported DXGI_FORMAT: %d. Only RGBA and BGRA are supported.");
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
                throw std::exception("Failed to create staging texture");
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
                throw std::exception("Failed to map staging texture");
            }

            mappedTexture = std::move(stagingTexture);
        }
        else {
            throw std::exception("Failed to map texture.");
        }
    }
    else {
        mappedTexture = Texture;
    }
    auto unmapResource = Finally([&] {
        d3dContext->Unmap(mappedTexture.Get(), 0);
        });


    // AWKWARD: desc.Height * mapInfo.RowPitch - how into QOI?
    qoi_desc qdesc = {};
    qdesc.width = desc.Width;
    qdesc.height = desc.Height;
    qdesc.channels = 4;
    qdesc.colorspace = QOI_SRGB;
    int out_len = 0;

    void* data = qoi_encode(mapInfo.pData, &qdesc, &out_len, qoi_fmt);

    if (data) {
        DWORD numWritten;
        WriteFile(pipe, data, out_len, &numWritten, NULL);

        /*/std::ofstream file;
        file.open(FileName, std::ios_base::out | std::ios_base::binary);
        file.write((char*)data, out_len);*/
    }
    free(data);

#if 0
    ComPtr<IWICImagingFactory> wicFactory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(wicFactory),
        reinterpret_cast<void**>(wicFactory.GetAddressOf()));
    if (FAILED(hr)) {
        throw std::exception("Failed to create instance of WICImagingFactory");
    }

    ComPtr<IWICBitmapEncoder> wicEncoder;
    hr = wicFactory->CreateEncoder(
        GUID_ContainerFormatBmp,
        nullptr,
        &wicEncoder);
    if (FAILED(hr)) {
        throw std::exception("Failed to create BMP encoder");
    }

    ComPtr<IWICStream> wicStream;
    hr = wicFactory->CreateStream(&wicStream);
    if (FAILED(hr)) {
        throw std::exception("Failed to create IWICStream");
    }

    hr = wicStream->InitializeFromFilename(FileName, GENERIC_WRITE);
    if (FAILED(hr)) {
        throw std::exception("Failed to initialize stream from file name");
    }

    hr = wicEncoder->Initialize(wicStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        throw std::exception("Failed to initialize bitmap encoder");
    }

    // Encode and commit the frame
    {
        ComPtr<IWICBitmapFrameEncode> frameEncode;
        wicEncoder->CreateNewFrame(&frameEncode, nullptr);
        if (FAILED(hr)) {
            throw std::exception("Failed to create IWICBitmapFrameEncode");
        }

        hr = frameEncode->Initialize(nullptr);
        if (FAILED(hr)) {
            throw std::exception("Failed to initialize IWICBitmapFrameEncode");
        }


        hr = frameEncode->SetPixelFormat(&wicFormatGuid);
        if (FAILED(hr)) {
            throw std::exception("SetPixelFormat(%s) failed.");
        }

        hr = frameEncode->SetSize(desc.Width, desc.Height);
        if (FAILED(hr)) {
            throw std::exception("SetSize(...) failed.");
        }

        hr = frameEncode->WritePixels(
            desc.Height,
            mapInfo.RowPitch,
            desc.Height * mapInfo.RowPitch,
            reinterpret_cast<BYTE*>(mapInfo.pData));
        if (FAILED(hr)) {
            throw std::exception("frameEncode->WritePixels(...) failed.");
        }

        hr = frameEncode->Commit();
        if (FAILED(hr)) {
            throw std::exception("Failed to commit frameEncode");
        }
    }

    hr = wicEncoder->Commit();
    if (FAILED(hr)) {
        throw std::exception("Failed to commit encoder");
    }
#endif
}