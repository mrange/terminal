#include "pch.h"

#include <wincodec.h>

#include "TextureLoader.h"

namespace
{
    // From: https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-textures-how-to

    //-------------------------------------------------------------------------------------
    // WIC Pixel Format Translation Data
    //-------------------------------------------------------------------------------------
    struct WICTranslate
    {
        GUID wic;
        DXGI_FORMAT format;
    };

    static WICTranslate g_WICFormats[] = {
        { GUID_WICPixelFormat128bppRGBAFloat, DXGI_FORMAT_R32G32B32A32_FLOAT },

        { GUID_WICPixelFormat64bppRGBAHalf, DXGI_FORMAT_R16G16B16A16_FLOAT },
        { GUID_WICPixelFormat64bppRGBA, DXGI_FORMAT_R16G16B16A16_UNORM },

        { GUID_WICPixelFormat32bppRGBA, DXGI_FORMAT_R8G8B8A8_UNORM },
        { GUID_WICPixelFormat32bppBGRA, DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppBGR, DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

        { GUID_WICPixelFormat32bppRGBA1010102XR, DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
        { GUID_WICPixelFormat32bppRGBA1010102, DXGI_FORMAT_R10G10B10A2_UNORM },
        { GUID_WICPixelFormat32bppRGBE, DXGI_FORMAT_R9G9B9E5_SHAREDEXP },

#ifdef DXGI_1_2_FORMATS

        { GUID_WICPixelFormat16bppBGRA5551, DXGI_FORMAT_B5G5R5A1_UNORM },
        { GUID_WICPixelFormat16bppBGR565, DXGI_FORMAT_B5G6R5_UNORM },

#endif // DXGI_1_2_FORMATS

        { GUID_WICPixelFormat32bppGrayFloat, DXGI_FORMAT_R32_FLOAT },
        { GUID_WICPixelFormat16bppGrayHalf, DXGI_FORMAT_R16_FLOAT },
        { GUID_WICPixelFormat16bppGray, DXGI_FORMAT_R16_UNORM },
        { GUID_WICPixelFormat8bppGray, DXGI_FORMAT_R8_UNORM },

        { GUID_WICPixelFormat8bppAlpha, DXGI_FORMAT_A8_UNORM },

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
        { GUID_WICPixelFormat96bppRGBFloat, DXGI_FORMAT_R32G32B32_FLOAT },
#endif
    };

    //-------------------------------------------------------------------------------------
    // WIC Pixel Format nearest conversion table
    //-------------------------------------------------------------------------------------

    struct WICConvert
    {
        GUID source;
        GUID target;
    };

    static WICConvert g_WICConvert[] = {
        // Note target GUID in this conversion table must be one of those directly supported formats (above).

        { GUID_WICPixelFormatBlackWhite, GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat1bppIndexed, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat2bppIndexed, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat4bppIndexed, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat8bppIndexed, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat2bppGray, GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
        { GUID_WICPixelFormat4bppGray, GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

        { GUID_WICPixelFormat16bppGrayFixedPoint, GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
        { GUID_WICPixelFormat32bppGrayFixedPoint, GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

#ifdef DXGI_1_2_FORMATS

        { GUID_WICPixelFormat16bppBGR555, GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

#else

        { GUID_WICPixelFormat16bppBGR555, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat16bppBGRA5551, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat16bppBGR565, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

#endif // DXGI_1_2_FORMATS

        { GUID_WICPixelFormat32bppBGR101010, GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

        { GUID_WICPixelFormat24bppBGR, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat24bppRGB, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPBGRA, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat32bppPRGBA, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

        { GUID_WICPixelFormat48bppRGB, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat48bppBGR, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppBGRA, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBA, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPBGRA, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

        { GUID_WICPixelFormat48bppRGBFixedPoint, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppBGRFixedPoint, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBAFixedPoint, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppBGRAFixedPoint, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBFixedPoint, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat64bppRGBHalf, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
        { GUID_WICPixelFormat48bppRGBHalf, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

        { GUID_WICPixelFormat96bppRGBFixedPoint, GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppPRGBAFloat, GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFloat, GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBAFixedPoint, GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
        { GUID_WICPixelFormat128bppRGBFixedPoint, GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

        { GUID_WICPixelFormat32bppCMYK, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppCMYK, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat40bppCMYKAlpha, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat80bppCMYKAlpha, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
        { GUID_WICPixelFormat32bppRGB, GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
        { GUID_WICPixelFormat64bppRGB, GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
        { GUID_WICPixelFormat64bppPRGBAHalf, GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
#endif

        // We don't support n-channel formats
    };

    DXGI_FORMAT WICFormatToDXGIFormat(const GUID& guid) noexcept
    {
        for (size_t i = 0; i < _countof(g_WICFormats); ++i)
        {
            if (g_WICFormats[i].wic == guid)
            {
                return g_WICFormats[i].format;
            }
        }

        return DXGI_FORMAT_UNKNOWN;
    }

    static UINT WICBitsPerPixel(
        IWICImagingFactory* wic,
        REFGUID targetGuid)
    {
        auto hr = S_OK;

        wil::com_ptr<IWICComponentInfo> cinfo;
        hr = wic->CreateComponentInfo(targetGuid, cinfo.addressof());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return 0;
        }

        WICComponentType type;
        hr = cinfo->GetComponentType(&type);
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return 0;
        }

        if (type != WICPixelFormat)
        {
            return 0;
        }

        wil::com_ptr<IWICPixelFormatInfo> pfinfo;
        hr = cinfo->QueryInterface(__uuidof(IWICPixelFormatInfo), pfinfo.put_void());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return 0;
        }

        UINT bpp;
        hr = pfinfo->GetBitsPerPixel(&bpp);
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return 0;
        }

        return bpp;
    }

    Microsoft::Console::Render::ShaderTexture LoadTexture(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
        const std::wstring& fileName)
    {
        Microsoft::Console::Render::ShaderTexture empty;

        if (!d3dDevice)
            return empty;
        if (!d3dContext)
            return empty;

        wil::com_ptr<IWICImagingFactory> wic;
        auto hr = S_OK;

        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),
            wic.put_void());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        wil::com_ptr<IWICBitmapDecoder> decoder;
        hr = wic->CreateDecoderFromFilename(
            fileName.c_str(),
            0,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            decoder.put());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        wil::com_ptr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(
            0,
            frame.put());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        UINT width = 0, height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }
        assert(width > 0 && height > 0);

        UINT maxsize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;

        switch (d3dDevice->GetFeatureLevel())
        {
        case D3D_FEATURE_LEVEL_9_1:
        case D3D_FEATURE_LEVEL_9_2:
            maxsize = 2048 /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        case D3D_FEATURE_LEVEL_9_3:
            maxsize = 4096 /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        case D3D_FEATURE_LEVEL_10_0:
        case D3D_FEATURE_LEVEL_10_1:
            maxsize = 8192 /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
            break;

        default:
            maxsize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
            break;
        }
        assert(maxsize > 0);

        UINT twidth = 0, theight = 0;
        if (width > maxsize || height > maxsize)
        {
            const float ar = static_cast<float>(height) / static_cast<float>(width);
            if (width > height)
            {
                twidth = maxsize;
                theight = static_cast<UINT>(static_cast<float>(maxsize) * ar);
            }
            else
            {
                theight = maxsize;
                twidth = static_cast<UINT>(static_cast<float>(maxsize) / ar);
            }
            assert(twidth <= maxsize && theight <= maxsize);
        }
        else
        {
            twidth = width;
            theight = height;
        }

        WICPixelFormatGUID wicFormat;
        hr = frame->GetPixelFormat(&wicFormat);
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        WICPixelFormatGUID convertGUID = wicFormat;

        UINT bpp = 0;
        DXGI_FORMAT dxgiFormat = WICFormatToDXGIFormat(wicFormat);
        if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
        {
            for (size_t i = 0; i < _countof(g_WICConvert); ++i)
            {
                if (g_WICConvert[i].source == wicFormat)
                {
                    convertGUID = g_WICConvert[i].target;

                    dxgiFormat = WICFormatToDXGIFormat(g_WICConvert[i].target);
                    assert(format != DXGI_FORMAT_UNKNOWN);
                    bpp = WICBitsPerPixel(wic.get(), convertGUID);
                    break;
                }
            }

            if (dxgiFormat == DXGI_FORMAT_UNKNOWN)
            {
                hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                LOG_HR(hr);
                return empty;
            }
        }
        else
        {
            bpp = WICBitsPerPixel(wic.get(), wicFormat);
        }

        if (!bpp)
        {
            hr = E_FAIL;
            LOG_HR(hr);
            return empty;
        }

        UINT support = 0;
        hr = d3dDevice->CheckFormatSupport(dxgiFormat, &support);
        if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_TEXTURE2D))
        {
            // Fallback to RGBA 32-bit format which is supported by all devices
            convertGUID = GUID_WICPixelFormat32bppRGBA;
            dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            bpp = 32;
        }

        // Checking inputs to make sure the final results
        //  fit in a UINT
        //  These are not expected to fail in normal ops
        //  But perhaps someone passes in a fuzzied image

        // bits per pixel of 64 supports up to 16bits per channel
        constexpr UINT MaxBpp = 64;
        constexpr UINT MaxSize = 8192;

        assert(bpp <= MaxBpp && bpp > 0);
        if (!(bpp <= MaxBpp && bpp > 0))
        {
            hr = E_INVALIDARG;
            LOG_HR(hr);
            return empty;
        }

        assert(twidth <= MaxSize && twidth > 0);
        if (!(twidth <= MaxSize && twidth > 0))
        {
            hr = E_INVALIDARG;
            LOG_HR(hr);
            return empty;
        }

        assert(theight <= MaxSize && theight > 0);
        if (!(theight <= MaxSize && theight > 0))
        {
            hr = E_INVALIDARG;
            LOG_HR(hr);
            return empty;
        }

        // Convert to 64bits so we don't get overflows in the static assert
        constexpr size_t MaxBppSz = MaxBpp;
        constexpr size_t MaxSizeSz = MaxSize;
        static_assert(
            (MaxSizeSz * (MaxSizeSz * MaxBppSz + 7) / 8) < UINT_MAX,
            "The maximum combination of size and bits per pixel must fit in UINT");

        const UINT rowPitch = (twidth * bpp + 7) / 8;
        const UINT imageSize = rowPitch * theight;

        auto temp = std::make_unique<uint8_t[]>(imageSize);

        if (convertGUID == wicFormat && twidth == width && theight == height)
        {
            // No format conversion or resize needed
            hr = frame->CopyPixels(nullptr, rowPitch, imageSize, temp.get());
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }
        }
        else if (twidth != width || theight != height)
        {
            wil::com_ptr<IWICBitmapScaler> scaler;
            hr = wic->CreateBitmapScaler(scaler.addressof());
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }

            hr = scaler->Initialize(frame.get(), twidth, theight, WICBitmapInterpolationModeFant);
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }

            WICPixelFormatGUID pfScaler;
            hr = scaler->GetPixelFormat(&pfScaler);
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }

            if (convertGUID == pfScaler)
            {
                // No format conversion needed
                hr = scaler->CopyPixels(nullptr, rowPitch, imageSize, temp.get());
                if (FAILED(hr))
                {
                    LOG_HR(hr);
                    return empty;
                }
            }
            else
            {
                wil::com_ptr<IWICFormatConverter> fc;
                hr = wic->CreateFormatConverter(fc.addressof());
                if (FAILED(hr))
                {
                    LOG_HR(hr);
                    return empty;
                }

                hr = fc->Initialize(scaler.get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
                if (FAILED(hr))
                {
                    LOG_HR(hr);
                    return empty;
                }

                hr = fc->CopyPixels(nullptr, rowPitch, imageSize, temp.get());
                if (FAILED(hr))
                {
                    LOG_HR(hr);
                    return empty;
                }
            }
        }
        else
        {
            wil::com_ptr<IWICFormatConverter> fc;
            hr = wic->CreateFormatConverter(fc.addressof());
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }

            hr = fc->Initialize(frame.get(), convertGUID, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }

            hr = fc->CopyPixels(nullptr, rowPitch, imageSize, temp.get());
            if (FAILED(hr))
            {
                LOG_HR(hr);
                return empty;
            }
        }

        // See if format is supported for auto-gen mipmaps (varies by feature level)
        bool autogen = false;
        wil::com_ptr<ID3D11ShaderResourceView> result;
        UINT fmtSupport = 0;
        hr = d3dDevice->CheckFormatSupport(dxgiFormat, &fmtSupport);
        if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
        {
            autogen = true;
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = twidth;
        desc.Height = theight;
        desc.MipLevels = (autogen) ? 0 : 1;
        desc.ArraySize = 1;
        desc.Format = dxgiFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = (autogen) ? (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET) : (D3D11_BIND_SHADER_RESOURCE);
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = (autogen) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = temp.get();
        initData.SysMemPitch = rowPitch;
        initData.SysMemSlicePitch = imageSize;

        wil::com_ptr<ID3D11Texture2D> texture;
        hr = d3dDevice->CreateTexture2D(
            &desc,
            (autogen) ? nullptr : &initData,
            texture.addressof());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
        SRVDesc.Format = dxgiFormat;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = (autogen) ? -1 : 1;

        wil::com_ptr<ID3D11ShaderResourceView> textureView;
        hr = d3dDevice->CreateShaderResourceView(texture.get(), &SRVDesc, textureView.addressof());
        if (FAILED(hr))
        {
            LOG_HR(hr);
            return empty;
        }

        if (autogen)
        {
            d3dContext->UpdateSubresource(
                texture.get(),
                0,
                nullptr,
                temp.get(),
                rowPitch,
                imageSize);
            d3dContext->GenerateMips(textureView.get());
        }

        return { texture, textureView };
    }
}

namespace Microsoft::Console::Render
{
    Microsoft::Console::Render::ShaderTexture LoadShaderTextureFromFile(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
        const std::wstring& fileName)
    {
        return LoadTexture(d3dDevice, d3dContext, fileName);
    }
}
