#!/usr/bin/env python3
"""Convert matching textures from a separate modified TRACKS bundle into NTXP.

Never edits the base game. Keeps original upload hashes and the mod's mip chain.
Bundle reader/writer: th0mk/nfsu2-web, MIT; see LICENSE and SOURCES.md.
"""
import argparse
from pathlib import Path
import struct
import zlib
import bundle_reader as reader

original_texture=reader.texture

def texture_levels(ti,pf,buf,base):
    nh,w,h,fmt,lv0=original_texture(ti,pf,buf,base)
    if lv0 is None:return nh,w,h,fmt,None,0,b''
    image_off,palette_off,image_size,palette_size,_=struct.unpack_from('<5I',ti,0x30)
    p=base+image_off
    if p<0 or image_size>len(buf)-p:return nh,w,h,fmt,None,0,b''
    image=buf[p:p+image_size]
    code=struct.unpack_from('<I',pf,0x14)[0] if len(pf)>=0x18 else 0
    palette=buf[base+palette_off:base+palette_off+1024] if code==reader.P8 else b''
    if code==reader.P8 and len(palette)!=1024:return nh,w,h,fmt,None,0,b''
    out=bytearray();offset=0;levels=0
    for level in range(max(w,h).bit_length()):
        lw,lh=max(1,w>>level),max(1,h>>level)
        size=lw*lh if code==reader.P8 else reader.level_size(lw,lh,fmt)
        if offset+size>len(image):break
        data=image[offset:offset+size];offset+=size
        if code==reader.P8:
            rgba=bytearray(lw*lh*4)
            for c in range(4):rgba[c::4]=data.translate(bytes(palette[c::4]))
            data=rgba
        out+=data;levels+=1
    return nh,w,h,fmt,lv0,levels,bytes(out)

reader.texture=texture_levels

def load(path):
    data=Path(path).read_bytes()
    if data[:4]==b'JDLZ':data=reader.jdlz(data)
    return {t[0]:t[1:] for t in reader.textures(data) if t[4] is not None}

def build(original,modified,output):
    old=load(original);new=load(modified);entries={};data=bytearray();skipped=0
    for name,(w,h,fmt,lv0,levels,levels_data) in new.items():
        if name not in old or not levels:continue
        ow,oh,ofmt,base,*_=old[name]
        if w<ow or h<oh or w*oh!=h*ow:skipped+=1;continue
        if (w,h,fmt,lv0)==(ow,oh,ofmt,base):continue
        key=zlib.crc32(base),zlib.adler32(base)
        if key in entries:continue
        entries[key]=dict(key=key,ow=ow,oh=oh,ofmt=ofmt,w=w,h=h,fmt=fmt,
                          levels=levels,off=len(data),size=len(levels_data),name=f'PLUS-World/{name:08X}')
        data+=levels_data;data+=bytes(-len(data)%16)
    if not entries:raise ValueError('No compatible changed textures found')
    reader.write(str(output),entries,data)
    larger=sum(e['w']>e['ow'] for e in entries.values())
    print(f'{len(entries)} replacements ({larger} higher-resolution), {len(data)/1048576:.1f} MiB; skipped {skipped} incompatible shapes')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('original');p.add_argument('modified');p.add_argument('output')
    a=p.parse_args();build(a.original,a.modified,a.output)
