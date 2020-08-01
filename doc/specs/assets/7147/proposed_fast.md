# https://www.websequencediagrams.com/
title Proposed "fast" present workflow

note right of Renderer: Assumes "slow" present has already been done
Renderer->DxRenderer: Present graphics
DxRenderer->DX11: Set texture as input to pixel shader
note left of DX11: Without terminal effects a NOP pixelshader is used
DxRenderer->DX11: Draw quad with pixelshader and texture to backbuffer

DxRenderer->DX11: Do full present

DxRenderer-->Renderer: Present graphics done
