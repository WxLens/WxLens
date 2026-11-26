#version 330 core

#define LATITUDE_MAX  85.051128779806604f
#define PI_OVER_4     0.785398163397448309615660825f
#define PI_OVER_360   0.00872664625997164788461845361111f
#define RAD2DEG       57.295779513082320876798156332941f

layout (location = 0) in vec2 aLatLong;
layout (location = 1) in vec2 aOtherLatLong;
layout (location = 2) in vec2 aSideHalfWidth; // x: side (+1/-1), y: half line width in pixels
layout (location = 3) in vec4 aColor;

uniform mat4 uMVPMatrix;
uniform vec2 uOriginLatLong;
uniform vec2 uViewportHalf;

out vec4 vColor;

// Identical to radar.vert, so overlays project exactly where the radar sweep does.
vec2 latLngToDeltaScreenCoordinate(in vec2 latLng)
{
   latLng.x = clamp(latLng.x, -LATITUDE_MAX, LATITUDE_MAX);

   vec2 deltaLatLng = latLng - uOriginLatLong;

   vec2 deltaScreen = vec2(
      deltaLatLng.y,
      RAD2DEG * log(tan(PI_OVER_4 + (uOriginLatLong.x + deltaLatLng.x) * PI_OVER_360)) -
      RAD2DEG * log(tan(PI_OVER_4 + uOriginLatLong.x * PI_OVER_360))
   );

   return deltaScreen;
}

void main()
{
   vColor = aColor;

   vec4 position = uMVPMatrix * vec4(latLngToDeltaScreenCoordinate(aLatLong), 0.0f, 1.0f);

   float halfWidth = aSideHalfWidth.y;
   if (halfWidth > 0.0f)
   {
      // Stroke corner: work in pixels so the width is constant on screen at every zoom.
      vec4 other = uMVPMatrix * vec4(latLngToDeltaScreenCoordinate(aOtherLatLong), 0.0f, 1.0f);
      vec2 self  = position.xy / position.w * uViewportHalf;
      vec2 toward = other.xy / other.w * uViewportHalf - self;
      float len = length(toward);
      vec2 dir = len > 1e-4f ? toward / len : vec2(1.0f, 0.0f);
      vec2 normal = vec2(-dir.y, dir.x);

      // Sideways by half the width, and back along the segment by the same amount (square cap)
      // so consecutive segments overlap at their shared vertex instead of leaving a notch.
      vec2 offsetPx = normal * aSideHalfWidth.x * halfWidth - dir * halfWidth;
      position.xy += offsetPx / uViewportHalf * position.w;
   }

   gl_Position = position;
}
