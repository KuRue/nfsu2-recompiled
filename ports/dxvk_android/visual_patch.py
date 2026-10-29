"""Apply optional presentation effects to DXVK's final swap-chain blit.

Occupied `native_visual` slots:
  x = sharpening amount (0..0.50)          from NFSU2_NATIVE_SHARPNESS
  y = look preset index (0..3)             from NFSU2_NATIVE_LOOK
  z = bloom strength (0, 0.5, 1.0)         from NFSU2_NATIVE_BLOOM
  w = reserved for future effects
"""
from pathlib import Path

SAMPLE_RING = (
    (5, 5), (-5, 5), (5, -5), (-5, -5),
    (14, 0), (-14, 0), (0, 14), (0, -14),
)


def _bloom_block(kind):
    """Glow taps for a variant; the multisampled variants keep it out."""
    if kind == "texel":
        taps = " +\n".join(
            "        nfsBright(texelFetch(s_image, clamp(nfs_c + ivec2(%d, %d), ivec2(0), nfs_edge), 0).rgb)"
            % (x, y) for x, y in SAMPLE_RING)
        return """  if (native_visual.z > 0.0) {
    ivec2 nfs_edge = textureSize(s_image, 0) - ivec2(1);
    ivec2 nfs_c = clamp(coord, ivec2(0), nfs_edge);
    vec3 nfs_glow =
%s;
    o_color.rgb += nfs_glow * (0.028 * native_visual.z);
  }
""" % taps
    if kind == "blit":
        taps = " +\n".join(
            "        nfsBright(textureLod(s_image, clamp(coord + vec2(%d, %d), vec2(0.5), nfs_edge), 0).rgb)"
            % (x, y) for x, y in SAMPLE_RING)
        return """  if (native_visual.z > 0.0) {
    vec2 nfs_edge = vec2(textureSize(s_image, 0)) - vec2(0.5);
    vec3 nfs_glow =
%s;
    o_color.rgb += nfs_glow * (0.028 * native_visual.z);
  }
""" % taps
    return ""


def _chain(kind):
    if kind == "blit":
        uv = "  vec2 nfs_uv = coord / vec2(textureSize(s_image, 0));\n"
    elif kind == "ms":
        # Multisampled samplers have no LOD argument for textureSize.
        uv = "  vec2 nfs_uv = (vec2(coord) + 0.5) / vec2(textureSize(s_image));\n"
    else:
        uv = "  vec2 nfs_uv = (vec2(coord) + 0.5) / vec2(textureSize(s_image, 0));\n"
    return (uv
            + "  o_color.rgb = nfsLook(o_color.rgb, native_visual.y);\n"
            + _bloom_block(kind)
            + "  o_color.rgb = nfsVignette(o_color.rgb, nfs_uv, nfsVignetteAmount(native_visual.y));\n"
            + "  if (nfsAnyEffect(native_visual.x, native_visual.y, native_visual.z))\n"
            + "    o_color.rgb = nfsDither(o_color.rgb, ivec2(gl_FragCoord.xy));\n")


def apply(root):
    here = Path(__file__).parent
    cpp = root / 'src/dxvk/dxvk_swapchain_blitter.cpp'
    header = root / 'src/dxvk/dxvk_swapchain_blitter.h'
    s = header.read_text()
    needle = '''        VkOffset2D dstOffset;
      };
    };'''
    assert needle in s
    s = s.replace(needle, '''        VkOffset2D dstOffset;
      };
      float nativeVisual[4];
    };''')
    s = s.replace('    Rc<DxvkDevice>      m_device;',
                  '    Rc<DxvkDevice>      m_device;\n    float m_nativeVisual[4] = {};')
    header.write_text(s)

    s = cpp.read_text().replace('#include "dxvk_swapchain_blitter.h"',
                                 '#include "dxvk_swapchain_blitter.h"\n#include <cstdlib>\n#include <algorithm>')
    s = s.replace('''  : m_device(device) {
    this->createSampler();''',
                  '''  : m_device(device) {
    const char* sharp = std::getenv("NFSU2_NATIVE_SHARPNESS");
    const char* look = std::getenv("NFSU2_NATIVE_LOOK");
    const char* bloom = std::getenv("NFSU2_NATIVE_BLOOM");
    m_nativeVisual[0] = sharp ? float(std::clamp(std::atoi(sharp), 0, 50)) / 100.0f : 0.0f;
    m_nativeVisual[1] = look ? float(std::clamp(std::atoi(look), 0, 3)) : 0.0f;
    int bloomLevel = bloom ? std::clamp(std::atoi(bloom), 0, 2) : 0;
    m_nativeVisual[2] = bloomLevel == 1 ? 0.5f : bloomLevel == 2 ? 1.0f : 0.0f;
    this->createSampler();''')
    s = s.replace('    args.srcOffset = srcRect.offset;',
                  '    args.srcOffset = srcRect.offset;\n    std::copy(m_nativeVisual, m_nativeVisual + 4, args.nativeVisual);')
    s = s.replace('m_device->extensions().amdShaderFragmentMask\n        ?',
                  'm_device->extensions().amdShaderFragmentMask && m_nativeVisual[0] == 0.0f\n        ?')
    cpp.write_text(s)

    helper = (here / 'visual_effects.glsl').read_text()
    variants = {
        'dxvk_present_frag': 'texel',
        'dxvk_present_frag_blit': 'blit',
        'dxvk_present_frag_ms': 'ms',
        'dxvk_present_frag_ms_amd': 'ms',
    }
    for name, kind in variants.items():
        path = root / f'src/dxvk/shaders/{name}.frag'
        s = path.read_text()
        needle = '  uvec2 src_extent;' if name.endswith('_blit') else '  ivec2 dst_offset;'
        assert needle in s
        s = s.replace(needle, needle + '\n  vec4 native_visual;')
        s = s.replace('void main() {', helper + '\nvoid main() {')

        if name == 'dxvk_present_frag':
            anchor = '  o_color = texelFetch(s_image, coord, 0);'
            sharp = '''
  if (native_visual.x > 0.0) {
    ivec2 edge = textureSize(s_image, 0) - ivec2(1);
    vec3 n = texelFetch(s_image, clamp(coord+ivec2(0,-1),ivec2(0),edge),0).rgb;
    vec3 s = texelFetch(s_image, clamp(coord+ivec2(0,1),ivec2(0),edge),0).rgb;
    vec3 e = texelFetch(s_image, clamp(coord+ivec2(1,0),ivec2(0),edge),0).rgb;
    vec3 w = texelFetch(s_image, clamp(coord+ivec2(-1,0),ivec2(0),edge),0).rgb;
    o_color.rgb = nfsSharpen(o_color.rgb,n,s,e,w,native_visual.x);
  }
'''
            assert anchor in s
            s = s.replace(anchor, anchor + sharp)
        elif name == 'dxvk_present_frag_blit':
            anchor = '  o_color = textureLod(s_image, coord, 0.0f);'
            sharp = '''
  if (native_visual.x > 0.0) {
    // DXVK's present sampler uses unnormalized pixel coordinates.
    vec2 edge = vec2(textureSize(s_image,0)) - vec2(0.5);
    vec3 n = textureLod(s_image,clamp(coord+vec2(0,-1),vec2(0.5),edge),0).rgb;
    vec3 s = textureLod(s_image,clamp(coord+vec2(0,1),vec2(0.5),edge),0).rgb;
    vec3 e = textureLod(s_image,clamp(coord+vec2(1,0),vec2(0.5),edge),0).rgb;
    vec3 w = textureLod(s_image,clamp(coord+vec2(-1,0),vec2(0.5),edge),0).rgb;
    o_color.rgb = nfsSharpen(o_color.rgb,n,s,e,w,native_visual.x);
  }
'''
            assert anchor in s
            s = s.replace(anchor, anchor + sharp)
        elif name == 'dxvk_present_frag_ms':
            resolve = '''vec3 nfsResolved(ivec2 coord) {
  ivec2 edge = textureSize(s_image)-ivec2(1);
  coord = clamp(coord,ivec2(0),edge);
  vec3 c=vec3(0); for(uint i=0;i<c_samples;i++) c+=texelFetch(s_image,coord,int(i)).rgb;
  return c/float(c_samples);
}
'''
            s = s.replace('void main() {', resolve + '\nvoid main() {')
            anchor = '  o_color /= float(c_samples);'
            assert anchor in s
            s = s.replace(anchor, anchor + '''
  if(native_visual.x>0.0) o_color.rgb=nfsSharpen(o_color.rgb,
    nfsResolved(coord+ivec2(0,-1)),nfsResolved(coord+ivec2(0,1)),
    nfsResolved(coord+ivec2(1,0)),nfsResolved(coord+ivec2(-1,0)),native_visual.x);
''')

        last = s.rfind('}')
        s = s[:last] + _chain(kind) + s[last:]
        path.write_text(s)
