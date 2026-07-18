# Third-Party Showcase Models

These visual test assets come from the official [Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) repository and [Poly Haven](https://polyhaven.com/). The renderer, importer changes, material system, and ImGui integration belong to this project; the original assets remain under the licenses listed below.

| Model | Local file | Meshes / materials | Source | License |
| --- | --- | ---: | --- | --- |
| Avocado | `Avocado/Avocado.glb` | 1 / 1 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Avocado) | CC0 1.0 |
| Lantern | `Lantern/Lantern.glb` | 1 / 1 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Lantern) | CC0 1.0 |
| BoomBox | `BoomBox/BoomBox.glb` | 1 / 1 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/BoomBox) | CC0 1.0 |
| Flight Helmet | `FlightHelmet/FlightHelmet.gltf` | 6 / 6 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/FlightHelmet) | CC0 1.0 |
| Toy Car | `ToyCar/ToyCar.glb` | 3 / 3 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/ToyCar) | CC0 1.0 |
| A Beautiful Game | `ABeautifulGame/ABeautifulGame.glb` | 15 / 15 | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/ABeautifulGame) | CC BY 4.0 |
| Fox | `Fox/Fox.glb` | 1 / 1; 24 bones / 3 clips | [Khronos source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Fox) | Model CC0 1.0; rigging/animation CC BY 4.0 |
| Rock 07 | `Rock07/rock_07_1k.gltf` | 1 / 1 | [Poly Haven / Jenelle van Heerden](https://polyhaven.com/a/rock_07) | CC0 |
| Moon Rock 01 | `MoonRock01/moon_rock_01_2k.gltf` | 1 / 1 | [Poly Haven](https://polyhaven.com/a/moon_rock_01) | CC0 |
| Moon Rock 02 | `MoonRock02/moon_rock_02_2k.gltf` | 1 / 1 | [Poly Haven](https://polyhaven.com/a/moon_rock_02) | CC0 |
| Moon Rock 06 | `MoonRock06/moon_rock_06_2k.gltf` | 1 / 1 | [Poly Haven](https://polyhaven.com/a/moon_rock_06) | CC0 |

Flight Helmet and Toy Car are dedicated to the public domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). A Beautiful Game credits the MaterialX Project for the original model and Ed Mackey for the glTF conversion under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

Fox credits PixelMannen for the CC0 model, tomkranis for the CC BY 4.0 rigging and animation, and AsoboStudio/scurest for the CC BY 4.0 glTF conversion. The local [`Fox/SOURCE.md`](Fox/SOURCE.md) and [`Fox/LICENSE.md`](Fox/LICENSE.md) preserve the upstream attribution and license metadata.

Toy Car and A Beautiful Game include optional glTF material extensions. This renderer currently displays their core metallic-roughness PBR data; clearcoat, sheen, transmission, and volume remain explicit extension work rather than silently claimed support.

The GPU Asteroid Belt combines Moon Rock 01, 02, and 06 as three instanced PBR batches. [Moon Meteor 01](https://polyhaven.com/a/moon_meteor_01) supplies subtle normal detail for the geological planet, while [Qwantani Night Pure Sky](https://polyhaven.com/a/qwantani_night_puresky) supplies the EXR input used for IBL precomputation. These Poly Haven assets are published under CC0. The planet albedo and visible sky have separate attributions in [`assets/textures/README.md`](../textures/README.md).
