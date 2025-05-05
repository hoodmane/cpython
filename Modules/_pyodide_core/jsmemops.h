#define DEREF_U8(addr, offset) HEAPU8[addr + offset]
#define DEREF_U16(addr, offset) HEAPU16[(addr >> 1) + offset]
#define DEREF_U32(addr, offset) HEAPU32[(addr >> 2) + offset]

#define ASSIGN_U8(addr, offset, value) DEREF_U8(addr, offset) = value
#define ASSIGN_U16(addr, offset, value) DEREF_U16(addr, offset) = value
#define ASSIGN_U32(addr, offset, value) DEREF_U32(addr, offset) = value
