#version 330 core

// No "precision mediump float;" here - that's an ES-only convention; this desktop GL 3.3 core
// driver rejected it as a syntax error when tested (confirmed against the legacy app's identical
// scwx-qt/gl/radar.frag, which has the same line - port that shader carefully, not verbatim).

layout (location = 0) out vec4 fragColor;

void main()
{
   // Solid orange - real product color mapping (data moment -> 1D LUT texture) arrives with the
   // ported radar sweep shader. This one only needs to prove it draws in the right place.
   fragColor = vec4(1.0, 0.6, 0.0, 1.0);
}
