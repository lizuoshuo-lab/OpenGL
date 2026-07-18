# Third-Party Planet And Sky Textures

The renderer keeps the visible sky separate from the HDR image used for IBL. This allows the photographic background to be composed independently while PBR lighting continues to use a high-dynamic-range source.

| Local file | Purpose | Source | License |
| --- | --- | --- | --- |
| `mars_viking_mdim21_color_4k.jpg` | Geological planet albedo | [Mars Viking Colorized Global Mosaic, USGS Astrogeology / NASA Ames](https://astrogeology.usgs.gov/search/map/mars_viking_colorized_global_mosaic_232m) | Public domain |
| `eso_milky_way_360_6k.jpg` | Visible 360-degree Milky Way background | [The Milky Way panorama, ESO/S. Brunier](https://www.eso.org/public/images/eso0932a/) | [CC BY 4.0](https://www.eso.org/public/outreach/copyright/) |
| `qwantani_night_puresky_2k.exr` | HDR input for environment, irradiance, and GGX prefilter cubemaps | [Poly Haven](https://polyhaven.com/a/qwantani_night_puresky) | CC0 |
| `MoonMeteor01/moon_meteor_01_nor_gl_2k.jpg` | Subtle planet micro-normal detail | [Poly Haven](https://polyhaven.com/a/moon_meteor_01) | CC0 |

The Mars source mosaic was resized to 4096 x 2048 and lightly color-adjusted for the real-time PBR scene. The ESO panorama is stored at its published 6000 x 3000 large-JPEG resolution. Credit for the visible panorama must remain **ESO/S. Brunier** in screenshots, videos, and derivative showcase pages.
