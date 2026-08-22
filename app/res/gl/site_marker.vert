#version 330 core

// Phase 1 slice 3: coordinate transform ported unchanged from the legacy app's
// scwx-qt/gl/radar.vert (external/legacy-supercell-wx) - this is the exact projection the real
// radar sweep shader will also use once ported. This trivial marker shader proves the custom
// layer rendering seam end-to-end (registration, GL context, uniforms, geo-anchoring) before
// that larger port.

#define LATITUDE_MAX  85.051128779806604f
#define PI_OVER_4     0.785398163397448309615660825f
#define PI_OVER_360   0.00872664625997164788461845361111f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;

uniform mat4 uMVPMatrix;
uniform vec2 uOriginLatLong;

vec2 latLngToDeltaScreenCoordinate(in vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);

   // Convert to smaller, relative coordinates
   vec2 deltaLatLng = latLng - uOriginLatLong;

   // Apply Web Mercator projection to the delta
   vec2 deltaScreen = vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360))
   );

   return deltaScreen;
}

void main()
{
   vec2 p = latLngToDeltaScreenCoordinate(aLatLong);
   gl_Position = uMVPMatrix * vec4(p, 0.0f, 1.0f);
   gl_PointSize = 16.0f;
}
