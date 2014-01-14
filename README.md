# Lightprobe Composer

Lightprobe Composer interactively converts high dynamic range lightprobe (mirror sphere) images to usable environment maps. The GUI is implemented in Racket 5.1 with domain-specific extensions in C and image processing in GLSL. All Racket, C, and GLSL code is available here under the terms of the GNU GPL. One or more HDR lightprobe photographs are loaded in TIFF format and their alignment is interactively tuned. Output may be produced in cube map, sphere map, and dome master forms.
