import gzip
import struct
import sys
import glob

def analyze_dump(dump_path):
    print("=" * 60)
    print("Analyzing:", dump_path)
    with gzip.open(dump_path, 'rb') as f:
        data = f.read()

    e_phoff, = struct.unpack('<I', data[28:32])
    e_phnum, = struct.unpack('<H', data[44:46])
    print(f"Program headers: {e_phnum} at offset {hex(e_phoff)}")

    for i in range(e_phnum):
        ph = data[e_phoff + i*32 : e_phoff + (i+1)*32]
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack('<8I', ph)
        if p_type == 4: # PT_NOTE
            print(f"PT_NOTE at offset {hex(p_offset)}, size {hex(p_filesz)}")
            note_data = data[p_offset : p_offset + p_filesz]
            n_off = 0
            while n_off + 12 <= len(note_data):
                namesz, descsz, ntype = struct.unpack('<3I', note_data[n_off:n_off+12])
                n_name = note_data[n_off+12 : n_off+12+namesz].rstrip(b'\x00').decode('latin1', 'ignore')
                n_off_aligned = (n_off + 12 + namesz + 3) & ~3
                desc = note_data[n_off_aligned : n_off_aligned + descsz]
                print(f"  Note type={hex(ntype)} name='{n_name}' descsz={descsz}")
                
                if "THREAD_REG_INFO" in n_name:
                    print(f"THREAD_REG_INFO total bytes: {len(desc)}")
                    # Each thread reg block is typically 0x178 or 0x180 bytes
                    off = 0
                    while off + 0x60 <= len(desc):
                        thid, = struct.unpack('<I', desc[off+12:off+16]) if off+16 <= len(desc) else (0,)
                        # Look for r0-r15, cpsr
                        # Vita SceThreadCpuRegisters has r0-r12, sp, lr, pc, cpsr
                        print(f"  Thread reg block at {hex(off)}:")
                        regs = [struct.unpack('<I', desc[off+i:off+i+4])[0] for i in range(0, min(len(desc)-off, 0x60), 4)]
                        print("   Regs:", [hex(r) for r in regs])
                        off += 0x178

                
                n_off = (n_off_aligned + descsz + 3) & ~3

if __name__ == '__main__':
    dumps = glob.glob('build/*.psp2dmp')
    for d in sorted(dumps, reverse=True)[:3]:
        try:
            analyze_dump(d)
        except Exception as e:
            print("Error analyzing:", d, e)
