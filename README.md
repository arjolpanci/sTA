# sTA

sTA (small Theft Auto) is a small GTA-inspired C++ / OpenGL sandbox. The engine and game are built from scratch as a learning project, with simple geometry until proper models are available.

![City streets](docs/screenshot.png)

## The city

- A 360 × 360 metre street grid: 36 blocks, 68 buildings of varied heights, a public park and a ramp yard west of the park.
- Sidewalks, crossings, lane markings, storefront awnings, rooftop equipment, benches, trees and street lamps.
- Procedural window façades with scattered illuminated windows, atmospheric distance fog, and shadows that follow the player. Building materials are generated in the shader; no additional downloaded texture assets are required.
- Animated box characters, 25 pedestrians on sidewalk patrols and eight AI traffic cars that brake into turns.
- Three nearby parked vehicles: sedan, taxi and van. Stolen traffic vehicles become yours and stop following their old route.
- A north-up minimap, speedometer, nearby-car prompt, and repeatable courier deliveries. Find a parked car, follow the teal marker, then stop inside it for 1.5 seconds to earn $150. Five destinations cycle around the city; money and progress last for the current session.

## Movement and collisions

Vehicles use upright oriented boxes with separating-axis collision checks; their hitboxes match their heading without ballooning diagonally. Steering cannot rotate a car into a wall. Character footprints stay stable while their bodies turn. Movement uses small substeps and slides along obstacles.

Both ramps have outward-facing geometry, walkable slopes and solid high ends. Ground following has bounded step heights, supports sidewalk edges and falls naturally off ledges. Vehicle bodies tilt to match slopes while their collision boxes remain upright for arcade handling. The camera shortens its orbit around static obstacles. Vehicle exits check both sides and the rear for safe ground and clearance.

This is still a prototype: traffic follows fixed loops and stops at obstructions; it does not reroute, obey traffic lights or react to crimes. There are no police, combat, audio, save files or imported character models yet. Street lamps are decorative; lighting comes from the sun, sky and material highlights.

## Building

### Linux

Requires CMake, Ninja, GLFW and OpenGL development libraries.

```sh
cmake -S . -B out/build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/linux-debug
cd out/build/linux-debug
./sTA
```

Run from the build directory so the game finds its copied `resources/` folder.

### Windows

Open the folder in Visual Studio with CMake integration and build the x64-Debug configuration. Supply a static `glfw3.lib` in `lib/`; it is not checked in.

## Controls

| Key | Action |
|---|---|
| W / A / S / D | Camera-relative movement; throttle / steer / brake and reverse in a car |
| Shift | Run on foot |
| Space | Jump on foot; handbrake in a car |
| F | Enter a nearby stopped car; exit at low speed if there is room |
| Mouse / scroll | Orbit camera / zoom |
| F1 | Pause simulation and open debug panels |
| Esc | Quit |

The debug panels include movement and vehicle tuning, shadow controls, a shadow-map preview and the actual oriented collision volumes.

## Verification

```sh
ctest --test-dir out/build/linux-debug --output-on-failure
cd out/build/linux-debug
./sTA --smoke-test
```

CTest runs without a display: it checks rotated collision clearance, ramp mesh winding, ramp ascent/descent, high-end blocking, jumping, curb stepping, fast wall collisions, two-minute traffic/pedestrian patrols and camera obstruction. The optional smoke test needs a working display/OpenGL context; it opens a hidden window, validates rendering and writes `smoke-city.ppm` and `smoke-ramp.ppm` in the working directory. Shader compilation/link errors fail the run.

## Dependencies

glad, glm, stb_image, Dear ImGui and GLFW headers are vendored in `include/`. Linux uses the system GLFW library; Windows uses the supplied `glfw3.lib`.
