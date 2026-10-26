"""Apply optional sharpening/grading to DXVK's existing final presentation pass."""
from pathlib import Path

def apply(root):
    here=Path(__file__).parent
    cpp=root/'src/dxvk/dxvk_swapchain_blitter.cpp'
    header=root/'src/dxvk/dxvk_swapchain_blitter.h'
    s=header.read_text().replace('      VkOffset2D srcOffset;', '      VkOffset2D srcOffset;')
    needle='''        VkOffset2D dstOffset;
      };
    };'''
    assert needle in s
    s=s.replace(needle,'''        VkOffset2D dstOffset;
      };
      float nativeVisual[4];
    };''')
    s=s.replace('    Rc<DxvkDevice>      m_device;', '    Rc<DxvkDevice>      m_device;\n    float m_nativeVisual[4] = {};')
    header.write_text(s)
    s=cpp.read_text().replace('#include "dxvk_swapchain_blitter.h"', '#include "dxvk_swapchain_blitter.h"\n#include <cstdlib>\n#include <algorithm>')
    s=s.replace('''  : m_device(device) {
    this->createSampler();''','''  : m_device(device) {
    const char* sharp = std::getenv("NFSU2_NATIVE_SHARPNESS");
    const char* look = std::getenv("NFSU2_NATIVE_LOOK");
    m_nativeVisual[0] = sharp ? float(std::clamp(std::atoi(sharp), 0, 50)) / 100.0f : 0.0f;
    m_nativeVisual[1] = look && std::atoi(look) == 1 ? 1.0f : 0.0f;
    this->createSampler();''')
    s=s.replace('    args.srcOffset = srcRect.offset;', '    args.srcOffset = srcRect.offset;\n    std::copy(m_nativeVisual, m_nativeVisual + 4, args.nativeVisual);')
    s=s.replace('m_device->extensions().amdShaderFragmentMask\n        ?', 'm_device->extensions().amdShaderFragmentMask && m_nativeVisual[0] == 0.0f\n        ?')
    cpp.write_text(s)
    helper=(here/'visual_effects.glsl').read_text()
    for name in ('dxvk_present_frag','dxvk_present_frag_blit','dxvk_present_frag_ms','dxvk_present_frag_ms_amd'):
        path=root/f'src/dxvk/shaders/{name}.frag'
        s=path.read_text()
        needle='  uvec2 src_extent;' if name.endswith('_blit') else '  ivec2 dst_offset;'
        assert needle in s
        s=s.replace(needle,needle+'\n  vec4 native_visual;')
        s=s.replace('void main() {',helper+'\nvoid main() {')
        if name=='dxvk_present_frag':
            needle='  o_color = texelFetch(s_image, coord, 0);'
            sharp='''
  if (native_visual.x > 0.0) {
    ivec2 edge = textureSize(s_image, 0) - ivec2(1);
    vec3 n = texelFetch(s_image, clamp(coord+ivec2(0,-1),ivec2(0),edge),0).rgb;
    vec3 s = texelFetch(s_image, clamp(coord+ivec2(0,1),ivec2(0),edge),0).rgb;
    vec3 e = texelFetch(s_image, clamp(coord+ivec2(1,0),ivec2(0),edge),0).rgb;
    vec3 w = texelFetch(s_image, clamp(coord+ivec2(-1,0),ivec2(0),edge),0).rgb;
    o_color.rgb = nfsSharpen(o_color.rgb,n,s,e,w,native_visual.x);
  }
'''
            assert needle in s;s=s.replace(needle,needle+sharp)
        elif name=='dxvk_present_frag_blit':
            needle='  o_color = textureLod(s_image, coord, 0.0f);'
            sharp='''
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
            assert needle in s;s=s.replace(needle,needle+sharp)
        elif name=='dxvk_present_frag_ms':
            resolve='''vec3 nfsResolved(ivec2 coord) {
  ivec2 edge = textureSize(s_image)-ivec2(1);
  coord = clamp(coord,ivec2(0),edge);
  vec3 c=vec3(0); for(uint i=0;i<c_samples;i++) c+=texelFetch(s_image,coord,int(i)).rgb;
  return c/float(c_samples);
}
'''
            s=s.replace('void main() {',resolve+'\nvoid main() {')
            needle='  o_color /= float(c_samples);'
            assert needle in s
            s=s.replace(needle,needle+'''\n  if(native_visual.x>0.0) o_color.rgb=nfsSharpen(o_color.rgb,
    nfsResolved(coord+ivec2(0,-1)),nfsResolved(coord+ivec2(0,1)),
    nfsResolved(coord+ivec2(1,0)),nfsResolved(coord+ivec2(-1,0)),native_visual.x);\n''')
        last=s.rfind('}')
        s=s[:last]+'  o_color.rgb = nfsNightLook(o_color.rgb,native_visual.y);\n'+s[last:]
        path.write_text(s)
