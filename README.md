![TitlePicBunnyPtCloud](https://github.com/MCInversion/LSWMeshFlow/blob/main/images/TitlePicBunnyPtCloud.png)

# LSW Mesh Flow:
## An Experimental Console App for Shrink-Wrapping Meshes, Point Clouds and SDFs.

Produces watertight envelopes:
<img src="https://github.com/MCInversion/LSWMeshFlow/blob/main/images/BPAvsLSW.png" data-canonical-src="https://github.com/MCInversion/LSWMeshFlow/blob/main/images/BPAvsLSW.png" width="200" height="400" />
and supports any starting surface, such as remeshed MarchingCubes isosurface:
![RockerArmShrinkWrap](https://github.com/MCInversion/LSWMeshFlow/blob/main/images/RockerArmShrinkWrap.png)

### Article Title:
Automated Surface Extraction: Adaptive Remeshing Meets Lagrangian Shrink-Wrapping

### Abstract
Fairing methods, frequently used for smoothing noisy features of surfaces, evolve a surface towards a simpler shape. The process of shaping a simple surface into a more complex object requires using a scalar field defined in the ambient space to drive the surface towards a target shape. Practical implementation of such evolution, referred to as Lagrangian Shrink-Wrapping (LSW), on discrete mesh surfaces presents a variety of challenges. Our key innovation lies in the integration of adaptive remeshing and curvature-based feature detection, ensuring mesh quality and proximity to target data. We introduce the Equilateral Triangle Jacobian Condition Number metric for assessing triangle quality, and introduce trilinear interpolation for enhanced surface detailing to improve upon existing implementations. Our approach is tested with point cloud meshing, isosurface extraction, and the elimination of internal mesh data, providing significant improvements in efficiency and accuracy. Moreover we extend the evolution to surfaces with higher genus to shrink-wrap even more complex data.	

### Keywords:
fairing, surface evolution, Lagrangian Shrink-Wrapping, adaptive remeshing, feature detection, mesh quality, point cloud meshing, isosurface extraction, mesh simplification
