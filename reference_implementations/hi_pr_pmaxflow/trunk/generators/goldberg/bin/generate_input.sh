#!/bin/bash

./genrmf -out ../../../input/genrmf_wide_28_5_1_10000_0 -seed 0 -a 28 -b 5 -c1 1 -c2 10000
./genrmf -out ../../../input/genrmf_wide_37_6_1_10000_0 -seed 0 -a 37 -b 6 -c1 1 -c2 10000
./genrmf -out ../../../input/genrmf_wide_49_7_1_10000_0 -seed 0 -a 49 -b 7 -c1 1 -c2 10000
./genrmf -out ../../../input/genrmf_wide_64_8_1_10000_0 -seed 0 -a 64 -b 8 -c1 1 -c2 10000
./genrmf -out ../../../input/genrmf_wide_85_9_1_10000_0 -seed 0 -a 85 -b 9 -c1 1 -c2 10000
#./genrmf -out ../../../input/genrmf_wide_111_10_1_10000_0 -seed 0 -a 111 -b 10 -c1 1 -c2 10000
#./genrmf -out ../../../input/genrmf_wide_147_12_1_10000_0 -seed 0 -a 147 -b 12 -c1 1 -c2 10000
#./genrmf -out ../../../input/genrmf_wide_194_14_1_10000_0 -seed 0 -a 194 -b 14 -c1 1 -c2 10000
./washington 6 1024 4 16 0 > ../../../input/wash_line_mod_6_1024_4_16_0
./washington 6 2048 4 23 0 > ../../../input/wash_line_mod_6_2048_4_23_0
./washington 6 4096 4 32 0 > ../../../input/wash_line_mod_6_4096_4_32_0
#./washington 6 8192 4 45 0 > ../../../input/wash_line_mod_6_8192_4_45_0
#./washington 6 16384 4 64 0 > ../../../input/wash_line_mod_6_16384_4_64_0
