import struct
import sys

def parse_elf(filename):
    with open(filename, 'rb') as f:
        elf = f.read()

    if elf[:4] != b'\x7fELF':
        print("Not an ELF file")
        return

    e_shoff, = struct.unpack('<I', elf[32:36])
    e_shentsize, = struct.unpack('<H', elf[46:48])
    e_shnum, = struct.unpack('<H', elf[48:50])
    e_shstrndx, = struct.unpack('<H', elf[50:52])

    shstr_hdr = elf[e_shoff + e_shstrndx*e_shentsize : e_shoff + (e_shstrndx+1)*e_shentsize]
    _, _, _, _, shstr_offset, _, _, _, _, _ = struct.unpack('<10I', shstr_hdr)

    symtabs = []
    strtabs = {}
    for i in range(e_shnum):
        sh = elf[e_shoff + i*e_shentsize : e_shoff + (i+1)*e_shentsize]
        sh_name_idx, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize = struct.unpack('<10I', sh)
        name = elf[shstr_offset + sh_name_idx:].split(b'\x00')[0].decode('latin1')
        if sh_type == 3: # SHT_STRTAB
            strtabs[i] = elf[sh_offset : sh_offset + sh_size]
        if sh_type == 2: # SHT_SYMTAB
            symtabs.append((name, sh_offset, sh_size, sh_entsize, sh_link))

    symbols = []
    for name, offset, size, entsize, link in symtabs:
        strtab = strtabs.get(link, b'')
        count = size // entsize
        for idx in range(count):
            entry = elf[offset + idx*entsize : offset + (idx+1)*entsize]
            st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack('<3IBBH', entry)
            sym_name = strtab[st_name:].split(b'\x00')[0].decode('latin1', 'ignore')
            if sym_name and st_value > 0:
                symbols.append((st_value, st_size, sym_name))

    symbols.sort(key=lambda s: s[0])
    print(f"Total symbols found: {len(symbols)}")
    return symbols

def lookup(symbols, target_addr):
    # Find matching symbol
    best = None
    for val, size, name in symbols:
        if val <= target_addr:
            best = (val, size, name)
        else:
            break
    if best:
        val, size, name = best
        offset = target_addr - val
        print(f"Address {hex(target_addr)} is in {name} + {hex(offset)} (size {hex(size)}, start {hex(val)})")
    else:
        print(f"No symbol found for {hex(target_addr)}")

if __name__ == '__main__':
    syms = parse_elf('build/eboot.elf')
    if syms:
        # Check PC: 0x8120b81c
        lookup(syms, 0x8120b81c)
        lookup(syms, 0x81037a33)
        lookup(syms, 0x811fb058)
        lookup(syms, 0x8120b800)
        # Search for bf_buff
        print("\nSymbols with bio or buff:")
        for v, s, n in syms:
            if 'buff' in n.lower() or 'bio' in n.lower() or 'curl' in n.lower():
                if abs(v - 0x8120b81c) < 0x20000:
                    print(f"  {hex(v)} ({s}): {n}")
