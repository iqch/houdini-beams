Made with impression of http://www.iro.umontreal.ca/~derek/publication1.html article this plug-in builds volumetric effect enveloping set of line segments with ability to control it's appearance with CVEX-shader.

# Description #
  * Uses set of 2-points primitives as base geometry, spooled to bgeo file
  * May reuse next attributes with beam building - color Cd, float width, normal N
  * ~~Ejects 2 shading variables with CVEX shader - color Cd, color Os~~ Now produced one vector4 Bs variable - you may assume it as color with alpha.
  * CVEX shader allows use any external parameters as uniform, excluding set of varying parameters - point P, color Cd, vector L, float Bl, float r, float id
  * Point P is real world position of currently shaded point
  * Color Cd is calculated from nearest beam point and its Cd attribute
  * Float Bl is current beam length
  * Float r is distance to nearest beam base line, normalized with width-parameter of nearest base point
  * Float id is a normalized count number of beam in beam set.
  * Vector l is a coordinate vector, composited from normalized base length at 0, and components of projection of current position relative beam's start point to beam's start point' normal and binomial. May be used as beam' s local coordsystem.
  * Beam has width, interpolated from point width attribute (default value, if such attribute does not exist is 1.0). Points in hemispherical caps are shaded too.
  * Multiple beams at one point compose their values with screen procedure.

# Issues #
  * ~~There is redundant job for computing different quantities, when we use atomic evaluation of variables. [R2](https://code.google.com/p/houdini-beams/source/detail?r=2): we need to use caching feature or see at more sophisticated evaluation schemes, like multyeval method etc.~~ All output quantities composed into one vector4 Bs. It produces very good rendering speedup, especially with complex beam shaders (5-10 times).
  * It may require cascade value compositing for numerical error minimization.

# Improvements #
  * We may support any additional variables from base geometry as varying parameters.
  * We may support not one-segment based beams bases. [R4](https://code.google.com/p/houdini-beams/source/detail?r=4): it may introduce a lot of fun with coordinate computing, width resolving, variable interpolation issues etc. Too much for starting.
  * ~~We may use nonAABB for optimization of presence solving, or coordinates computation, use special matrices. [R5](https://code.google.com/p/houdini-beams/source/detail?r=5): it will be much more useful, if this task would be supported with mantra too, to skip useless sampling of volume.~~ solved with splitting boundbox to group of boxes, which better envelop a beam and contain less empty space. It produced speedup up to 25%.
  * We may rebuild existing solution for using with prman, with implicits, since there are no issues with reading of text based geo-format by dso. But we absolutely han't any knowledge about beam shading routines. Using CVEX or native geo-reading may lead to non-working code, as we already know.
