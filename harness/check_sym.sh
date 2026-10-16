#!/bin/bash
export PATH="/c/msys64/home/celso/saturn-tools/bin:$PATH"
cd "/c/Users/celso/Documents/projetos/libsaturn-1"
sh2eb-elf-nm build/vdp2_dual_rotation.elf | grep "_g_dual_rotation"