cmake_minimum_required(VERSION 3.10)
project(DIP_Exercise_C C)

set(CMAKE_C_STANDARD 99)

# 1. Huffman Coding
add_executable(Huffman Huffman.c)
target_link_libraries(Huffman m)

# 2. DCT + Quantization + IQ + IDCT
add_executable(dct_q_iq_idct dct-q-iq-idct.c)
target_link_libraries(dct_q_iq_idct m)

# 3. LPF in frequency domain
add_executable(lpf lpf.c)
target_link_libraries(lpf m)

# 4. HPF in frequency domain
add_executable(hpf hpf.c)
target_link_libraries(hpf m)
