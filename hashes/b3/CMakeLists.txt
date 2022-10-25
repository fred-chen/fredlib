enable_language(ASM)
add_library ( blake3 STATIC
                blake3.c 
                blake3_dispatch.c 
                blake3_portable.c
                blake3_sse2_x86-64_unix.S 
                blake3_sse41_x86-64_unix.S
                blake3_avx2_x86-64_unix.S
                blake3_avx512_x86-64_unix.S 
            )