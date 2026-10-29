// Optional display-referred presentation effects. DXVK's final swap-chain blit
// runs this after the game's gamma ramp, so it grades the finished picture and
// never changes scene rendering.
float nfsLuma(vec3 color) { return dot(color, vec3(0.2126, 0.7152, 0.0722)); }

// Look presets: 0 original, 1 night balanced, 2 neon night, 3 filmic night.
vec3 nfsLook(vec3 color, float preset) {
  if (preset < 0.5) return color;
  float luminance = nfsLuma(color);
  if (preset < 1.5) {
    float shadows = 1.0 - smoothstep(0.05, 0.55, luminance);
    vec3 lifted = color + 0.055 * sqrt(max(color, vec3(0.0))) * (1.0 - color) * shadows;
    lifted = (lifted - 0.4) * 1.035 + 0.4;
    float l = nfsLuma(lifted);
    return clamp(mix(vec3(l), lifted, 1.035), 0.0, 1.0);
  }
  if (preset < 2.5) {
    // Neon night: cool shadows, warm highlights, a little more contrast and colour.
    vec3 shadowTint = vec3(-0.010, 0.014, 0.052);
    vec3 lightTint = vec3(0.024, 0.006, 0.030);
    vec3 t = color + mix(shadowTint, lightTint, smoothstep(0.15, 0.85, luminance));
    t = clamp((t - 0.5) * 1.06 + 0.5, 0.0, 1.0);
    float l = nfsLuma(t);
    return clamp(mix(vec3(l), t, 1.10), 0.0, 1.0);
  }
  // Filmic night: lifted blacks, a soft highlight shoulder, gentle desaturation.
  vec3 t = color * 0.94 + 0.032;
  t = t / (1.0 + 0.10 * t) * 1.09;
  t += vec3(0.014, 0.005, -0.008) * smoothstep(0.5, 1.0, luminance);
  float l = nfsLuma(t);
  return clamp(mix(vec3(l), t, 0.94), 0.0, 1.0);
}

// Keep only bright picture content for the glow taps.
vec3 nfsBright(vec3 color) { return color * smoothstep(0.58, 0.95, nfsLuma(color)); }

float nfsVignetteAmount(float preset) {
  return preset < 0.5 ? 0.0 : preset < 1.5 ? 0.05 : preset < 2.5 ? 0.11 : 0.16;
}

vec3 nfsVignette(vec3 color, vec2 uv, float amount) {
  if (amount <= 0.0) return color;
  vec2 d = uv - 0.5;
  return color * (1.0 - amount * smoothstep(0.18, 0.85, dot(d, d) * 2.0));
}

// Ordered dithering removes banding in the night sky on an 8-bit swap chain.
vec3 nfsDither(vec3 color, ivec2 pixel) {
  float m[16] = float[16](
      0.0,  8.0,  2.0, 10.0,
     12.0,  4.0, 14.0,  6.0,
      3.0, 11.0,  1.0,  9.0,
     15.0,  7.0, 13.0,  5.0);
  int index = (pixel.y & 3) * 4 + (pixel.x & 3);
  return color + (m[index] / 16.0 - 0.5) * (1.0 / 255.0);
}

bool nfsAnyEffect(float sharpen, float preset, float bloom) {
  return sharpen > 0.0 || preset > 0.0 || bloom > 0.0;
}

vec3 nfsSharpen(vec3 center, vec3 n, vec3 s, vec3 e, vec3 w, float amount) {
  vec3 low = min(center, min(min(n,s),min(e,w)));
  vec3 high = max(center, max(max(n,s),max(e,w)));
  return clamp(center + amount * (center - 0.25*(n+s+e+w)), low, high);
}
