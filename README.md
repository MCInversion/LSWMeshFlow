![TitlePicBunnyPtCloud](https://github.com/MCInversion/LSWMeshFlow/blob/main/images/TitlePicBunnyPtCloud.png)

# LSW Mesh Flow:
## An Experimental Console App for Shrink-Wrapping Meshes, Point Clouds and SDFs.

Produces watertight envelopes:

<img src="https://github.com/MCInversion/LSWMeshFlow/blob/main/images/BPAvsLSW.png" data-canonical-src="https://github.com/MCInversion/LSWMeshFlow/blob/main/images/BPAvsLSW.png" width="400" />

and supports any starting surface, such as remeshed MarchingCubes isosurface:

![RockerArmShrinkWrap](https://github.com/MCInversion/LSWMeshFlow/blob/main/images/RockerArmShrinkWrap.png)

## Dev/Setup

Clone the repository:

```sh
git clone --recursive https://github.com/MCInversion/LSWMeshFlow.git
```

If you've already cloned the repo without the `--recursive` flag, check out the submodules in the cloned repo's directory using:

```sh
git submodule update
```

Configure and build:

```sh
cd LSWMeshFlow && mkdir build && cd build && cmake .. && make
```
This repo is a fork of the PMP Library: https://www.pmp-library.org.

### Additional dependencies include:

- Eigen
- Glew
- Glfw
- GoogleTest
- Imgui
- [Nanoflann](https://github.com/jlblancoc/nanoflann)
- Rply
- [Quickhull](https://github.com/akuukka/quickhull)

## Functionality 

This repository focuses primarly on the evaluation of the Lagrangian Shrink Wrapping functionality containing tests:
- SDF Tests
- Sphere Test
- Evolver Tests
- Old Armadillo LSW Test
- Isosurface Evolver Tests
- Sheet Evolver Test
- Brain Evolver Tests
- Remeshing Tests
- Mobius Strip Voxelization
- Metaball Test
- Imported Obj Metrics Eval
- Simple Bunny OBJ Sampling Demo
- PDaniel Pt Cloud PLY Export
- Pt Cloud To DF
- PDaniel Pt Cloud Comparison Test
- Repulsive Surf Result Evaluation
- Histogram Result Evaluation
- Old Result Jacobian Metric Eval
- Hausdorff Distance Measurements Per Time Step
- Direct Higher Genus Pt Cloud Sampling
- Higher Genus Pt Cloud LSW
- TriTri Intersection Tests
- Mesh Self Intersection Tests
- Hurtado Meshes Isosurface Evolver Tests
- Hurtado Trex Icosphere LSW
- Import VTI Debug Tests
- Convex Hull Tests
- Convex Hull Remeshing Tests
- Convex Hull Evolver Tests
- IcoSphere Evolver Tests

To run a test in `Main.cpp`, set its flag to true:
```cpp
constexpr bool performPtCloudToDF = true;
```

### Article Title:
Automated Surface Extraction: Adaptive Remeshing Meets Lagrangian Shrink-Wrapping

### Abstract
Fairing methods, frequently used for smoothing noisy features of surfaces, evolve a surface towards a simpler shape. The process of shaping a simple surface into a more complex object requires using a scalar field defined in the ambient space to drive the surface towards a target shape. Practical implementation of such evolution, referred to as Lagrangian Shrink-Wrapping (LSW), on discrete mesh surfaces presents a variety of challenges. Our key innovation lies in the integration of adaptive remeshing and curvature-based feature detection, ensuring mesh quality and proximity to target data. We introduce the Equilateral Triangle Jacobian Condition Number metric for assessing triangle quality, and introduce trilinear interpolation for enhanced surface detailing to improve upon existing implementations. Our approach is tested with point cloud meshing, isosurface extraction, and the elimination of internal mesh data, providing significant improvements in efficiency and accuracy. Moreover we extend the evolution to surfaces with higher genus to shrink-wrap even more complex data.	

### Keywords:
fairing, surface evolution, Lagrangian Shrink-Wrapping, adaptive remeshing, feature detection, mesh quality, point cloud meshing, isosurface extraction, mesh simplification
