<img align="left" src="Adria/Resources/Icons/adria_logo.jpg" width="120px"/>

# Adria-DX12

Graphics engine written in C++ using DirectX12. For successful build you will need textures that you can find [here](https://github.com/mateeeeeee/Adria-DX11/releases/tag/1.0).
 
## Features
* Render graph
	- Automatic resource barriers
	- Resource reuse using resource pool
	- Automatic resource bind flags and initial state deduction
	- Pass culling
    - Graph visualization
* DDGI
    - Probe visualization
* GPU-Driven Rendering
    - GPU frustum culling
    - 2 phase GPU occlusion culling
* Reference path tracer 
* Temporal upscalers
    - FSR2, FSR3
    - XeSS 
    - DLSS3
* Ultimate Bindless 
    - Only one root signature 
* Volumetric lighting for shadow casting lights
* Tiled deferred rendering 
* Clustered deferred rendering
* Shadows
    - PCF shadows for directional, spot and point lights
    - Cascade shadow maps for directional lights
    - Ray traced shadows (DXR)
* Volumetric clouds
    - Temporal reprojection
* Hosek-Wilkie sky
* Ocean FFT
* Automatic exposure
* Bloom
* Depth of field + Bokeh 
    - Custom
    - FFX
* Ambient occlusion: 
    - SSAO
    - HBAO
    - CACAO (FFX)
    - RTAO (DXR)
* Reflections: 
    - SSR
    - RTR (DXR)
* Antialiasing: FXAA, TAA
* Contrast Adaptive Sharpening (FFX)
* Film effects: Lens distortion, Chromatic aberration, Vignette, Film grain
* God rays
* Lens flare
* Motion blur
* ImGui editor
* Profiler
    - custom and tracy profiler
* Debug tools
    - Debug renderer
    - Shader hot reloading
    - Render graph dump and visualization
    - Shader debug printf

## TODO
* ReSTIR GI

## Screenshots

### DDGI

| Disabled |  Enabled |
|---|---|
|  ![](Adria/Saved/Screenshots/noddgi.png) | ![](Adria/Saved/Screenshots/ddgi.png) |

| Probe Visualization |
|---|
|  ![](Adria/Saved/Screenshots/ddgi_probes1.png) |

### Volumetric Clouds
![alt text](Adria/Saved/Screenshots/clouds.png "Volumetric clouds") 

### San Miguel
![alt text](Adria/Saved/Screenshots/sanmiguel.png "San Miguel") 

### Ocean
![alt text](Adria/Saved/Screenshots/ocean.png "Ocean") 

### Path Tracer
![alt text](Adria/Saved/Screenshots/pathtracing1.png "Path tracing") 
![alt text](Adria/Saved/Screenshots/pathtracing2.png "Path tracing") 

### Editor
![alt text](Adria/Saved/Screenshots/editor.png "Editor") 

### Ray Tracing Features

| Cascaded Shadow Maps |  Hard Ray Traced Shadows |
|---|---|
|  ![](Adria/Saved/Screenshots/cascades.png) | ![](Adria/Saved/Screenshots/raytraced.png) |

| Screen Space Reflections |  Ray Traced Reflections |
|---|---|
|  ![](Adria/Saved/Screenshots/ssr.png) | ![](Adria/Saved/Screenshots/rtr.png) |

| SSAO | RTAO |
|---|---|
|  ![](Adria/Saved/Screenshots/ssao.png) | ![](Adria/Saved/Screenshots/rtao.png) |

### Render Graph Visualization
![alt text](Adria/Saved/RenderGraph/rendergraph.svg "Render graph visualization") 
