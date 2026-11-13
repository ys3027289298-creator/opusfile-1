import sys, struct

def packets(fn):
    data=open(fn,'rb').read()
    i=0
    cur=bytearray()
    serial=None
    while i < len(data):
        if data[i:i+4]!=b'OggS':
            i+=1; continue
        htype=data[i+5]
        serial=struct.unpack('<I',data[i+14:i+18])[0]
        nseg=data[i+26]
        segs=list(data[i+27:i+27+nseg])
        body_start=i+27+nseg
        body=data[body_start:body_start+sum(segs)]
        i=body_start+sum(segs)
        off=0
        for s in segs:
            cur+=body[off:off+s]; off+=s
            if s<255:
                yield serial,bytes(cur); cur=bytearray()

for serial,p in packets(sys.argv[1]):
    if p.startswith(b'OpusTags'):
        j=8
        vendor_len=struct.unpack('<I',p[j:j+4])[0]; j+=4
        vendor=p[j:j+vendor_len].decode(); j+=vendor_len
        n=struct.unpack('<I',p[j:j+4])[0]; j+=4
        print("serial",serial,"vendor:",vendor,"ntags:",n)
        for _ in range(n):
            l=struct.unpack('<I',p[j:j+4])[0]; j+=4
            print("  ",p[j:j+l].decode('utf-8','replace')); j+=l
        print("  suffix bytes:",len(p)-j)
    elif p.startswith(b'OpusHead'):
        ch=p[9]; preskip=struct.unpack('<H',p[10:12])[0]
        sr=struct.unpack('<I',p[12:16])[0]; gain=struct.unpack('<h',p[16:18])[0]
        print("serial",serial,"OpusHead ch",ch,"preskip",preskip,"orig_sr",sr,"output_gain_q8",gain)
