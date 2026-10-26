// Optional display-referred grading, not physically based scene relighting.
vec3 nfsNightLook(vec3 color, float enabled) {
  if (enabled < 0.5) return color;
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float shadows = 1.0 - smoothstep(0.05, 0.55, luminance);
  vec3 lifted = color + 0.055 * sqrt(max(color, vec3(0.0))) * (1.0 - color) * shadows;
  lifted = (lifted - 0.4) * 1.035 + 0.4;
  float l = dot(lifted, vec3(0.2126, 0.7152, 0.0722));
  return clamp(mix(vec3(l), lifted, 1.035), 0.0, 1.0);
}
vec3 nfsSharpen(vec3 center, vec3 n, vec3 s, vec3 e, vec3 w, float amount) {
  vec3 low = min(center, min(min(n,s),min(e,w)));
  vec3 high = max(center, max(max(n,s),max(e,w)));
  return clamp(center + amount * (center - 0.25*(n+s+e+w)), low, high);
}
