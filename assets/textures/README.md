# Third-Party Planet And Sky Textures

The renderer keeps the visible sky separate from the HDR image used for IBL. This allows the photographic background to be composed independently while PBR lighting continues to use a high-dynamic-range source.

| Local file | Purpose | Source | License |
| --- | --- | --- | --- |
| `mars_viking_mdim21_color_4k.jpg` | Geological planet albedo | [Mars Viking Colorized Global Mosaic, USGS Astrogeology / NASA Ames](https://astrogeology.usgs.gov/search/map/mars_viking_colorized_global_mosaic_232m) | Public domain |
| `qwantani_night_puresky_2k.exr` | HDR input for environment, irradiance, and GGX prefilter cubemaps | [Poly Haven](https://polyhaven.com/a/qwantani_night_puresky) | CC0 |
| `MoonMeteor01/moon_meteor_01_nor_gl_2k.jpg` | Subtle planet micro-normal detail | [Poly Haven](https://polyhaven.com/a/moon_meteor_01) | CC0 |

The Mars source mosaic was resized to 4096 x 2048 and lightly color-adjusted for the real-time PBR scene. The visible sky is generated in `assets/shaders/cube.frag` from a near-black deep-space base and bounded random star points; it does not require an external background photograph.
