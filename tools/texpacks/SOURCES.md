`bundle_reader.py` is copied from `tools/texpacks.py` in th0mk/nfsu2-web,
revision 655e6c8c700c71bbf2c06d9e4f4fb43ae4a5f1bc. Its MIT license is in LICENSE.
The local world_pack.py converter extends that parser's texture extraction to
retain complete mip chains. Generated packs contain game/mod assets and are
local build inputs only; they must not be committed.

Tested inputs:
- LunicAura106: Upscaled HUD, https://nfsmods.xyz/mod/4319.
  The ready NTXP pack was fetched from https://nfsu2.xyz/data/TEXPACKS/uhud.pak.
- AJLethal: PLUS World Texture Mod, https://nfsmods.xyz/mod/349.
  Author credit is retained here as requested by the mod. The mod's TRACKS bundle
  is read separately and converted into a GPU-only replacement pack; the original
  game bundle is not overwritten. No claim of authorship of either asset pack.
- I_NEED_SPEED / Azazel: New HD Textures Pack v1.1,
  https://nfsmods.xyz/mod/896. Source archive linked by the author:
  https://www.mediafire.com/file/had3j808lw1a1qp/NFSU2_MOD_v1.1.7z.
  Converted separately obtained DDS assets using texmod_pack.py; not redistributed
  in Git. TexMod's uncomplemented CRC32 format is documented in the open-source
  uMod implementation: build-wars/texmod, uMod_DX9/uMod_TextureFunction.cpp.
