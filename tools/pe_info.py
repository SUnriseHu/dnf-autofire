import sys, struct

def rva_to_off(rva, sections):
    for va, vs, raw, rsz in sections:
        size = max(vs, rsz)
        if va <= rva < va + size:
            return raw + (rva - va)
    return None

def cstr(data, off):
    if off is None or off >= len(data):
        return None
    e = data.find(b'\x00', off)
    if e < 0:
        return None
    return data[off:e].decode('latin1', 'replace')

def parse(path):
    d = open(path, 'rb').read()
    print("===", path, "size=%d" % len(d))
    if d[:2] != b'MZ':
        print("  not MZ"); return
    e_lfanew = struct.unpack_from('<I', d, 0x3c)[0]
    if d[e_lfanew:e_lfanew+4] != b'PE\0\0':
        print("  not PE"); return
    coff = e_lfanew + 4
    machine, nsec, tds, psym, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', d, coff)
    print("  Machine=0x%04x (%s)  Sections=%d  Chars=0x%04x" %
          (machine, {0x14c: 'x86', 0x8664: 'x64'}.get(machine, '?'), nsec, chars))
    opt = coff + 20
    magic = struct.unpack_from('<H', d, opt)[0]
    print("  OptionalMagic=0x%04x (%s)" % (magic, {0x10b: 'PE32', 0x20b: 'PE32+'}.get(magic, '?')))
    # 时间戳
    print("  TimeDateStamp=0x%08x" % tds)
    # 入口点
    ep = struct.unpack_from('<I', d, opt + 16)[0]
    print("  EntryPoint RVA=0x%08x" % ep)
    ddoff = opt + (96 if magic == 0x10b else 112)
    dirs = []
    for i in range(16):
        rva, sz = struct.unpack_from('<II', d, ddoff + i * 8)
        dirs.append((rva, sz))
    sec_off = opt + optsz
    sections = []
    print("  -- Sections --")
    for i in range(nsec):
        o = sec_off + i * 40
        name = d[o:o + 8].rstrip(b'\x00').decode('latin1', 'replace')
        vs, va, rsz, raw = struct.unpack_from('<IIII', d, o + 8)
        secchars = struct.unpack_from('<I', d, o + 36)[0]
        sections.append((va, vs, raw, rsz))
        print("    %-9s VA=0x%08x VS=0x%08x RAW=0x%08x RSZ=0x%08x C=0x%08x" %
              (name, va, vs, raw, rsz, secchars))
    # 导出
    exp_rva = dirs[0][0]
    if exp_rva:
        eo = rva_to_off(exp_rva, sections)
        if eo is not None:
            nname = struct.unpack_from('<I', d, eo + 24)[0]
            aon = struct.unpack_from('<I', d, eo + 32)[0]
            no = rva_to_off(aon, sections)
            print("  -- Exports (names=%d) --" % nname)
            if no is not None:
                for i in range(nname):
                    nrva = struct.unpack_from('<I', d, no + i * 4)[0]
                    print("    ", cstr(d, rva_to_off(nrva, sections)))

    # 导入
    imp_rva = dirs[1][0]
    print("  -- Imports --")
    off = rva_to_off(imp_rva, sections)
    if off:
        i = 0
        while True:
            o = off + i * 20
            oft, tds2, fc, nameRVA, ft = struct.unpack_from('<IIIII', d, o)
            if nameRVA == 0 and oft == 0 and ft == 0:
                break
            print("    ", cstr(d, rva_to_off(nameRVA, sections)))
            i += 1
            if i > 300:
                break
    # 延迟导入
    dil_rva = dirs[13][0]
    if dil_rva:
        print("  -- Delay Imports --")
        off = rva_to_off(dil_rva, sections)
        i = 0
        while off:
            o = off + i * 32
            attrs, nameRVA = struct.unpack_from('<II', d, o)
            if nameRVA == 0:
                break
            print("    ", cstr(d, rva_to_off(nameRVA, sections)))
            i += 1
            if i > 100:
                break

for p in sys.argv[1:]:
    parse(p)
    print()
