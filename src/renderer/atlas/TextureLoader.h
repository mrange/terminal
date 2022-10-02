// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console::Render
{
    struct ShaderTexture
    {
        wil::com_ptr<ID3D11Texture2D> Texture;
        wil::com_ptr<ID3D11ShaderResourceView> TextureView;
    };

    Microsoft::Console::Render::ShaderTexture LoadShaderTextureFromFile(
        ID3D11Device* d3dDevice,
        ID3D11DeviceContext* d3dContext,
        const std::wstring& fileName);
}
