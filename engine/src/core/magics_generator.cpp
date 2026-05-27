#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>

typedef uint64_t U64;

int count_bits(U64 bb) {
    int count = 0;
    while (bb) { count++; bb &= bb - 1; }
    return count;
}
int lsb(U64 bb) { return __builtin_ctzll(bb); }
void pop_bit(U64& bb, int sq) { bb &= ~(1ULL << sq); }

U64 mask_bishop_attacks(int square) {
    U64 attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    for (int r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) attacks |= (1ULL << (r * 8 + f));
    for (int r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) attacks |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) attacks |= (1ULL << (r * 8 + f));
    for (int r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) attacks |= (1ULL << (r * 8 + f));
    return attacks;
}

U64 mask_rook_attacks(int square) {
    U64 attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    for (int r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
    for (int r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
    for (int f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
    for (int f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));
    return attacks;
}

U64 bishop_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8, tf = square % 8;
    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) { attacks |= (1ULL << (r * 8 + f)); if (block & (1ULL << (r * 8 + f))) break; }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) { attacks |= (1ULL << (r * 8 + f)); if (block & (1ULL << (r * 8 + f))) break; }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) { attacks |= (1ULL << (r * 8 + f)); if (block & (1ULL << (r * 8 + f))) break; }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) { attacks |= (1ULL << (r * 8 + f)); if (block & (1ULL << (r * 8 + f))) break; }
    return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8, tf = square % 8;
    for (r = tr + 1; r <= 7; r++) { attacks |= (1ULL << (r * 8 + tf)); if (block & (1ULL << (r * 8 + tf))) break; }
    for (r = tr - 1; r >= 0; r--) { attacks |= (1ULL << (r * 8 + tf)); if (block & (1ULL << (r * 8 + tf))) break; }
    for (f = tf + 1; f <= 7; f++) { attacks |= (1ULL << (tr * 8 + f)); if (block & (1ULL << (tr * 8 + f))) break; }
    for (f = tf - 1; f >= 0; f--) { attacks |= (1ULL << (tr * 8 + f)); if (block & (1ULL << (tr * 8 + f))) break; }
    return attacks;
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++) {
        int square = lsb(attack_mask);
        pop_bit(attack_mask, square);
        if (index & (1 << count)) occupancy |= (1ULL << square);
    }
    return occupancy;
}

unsigned int state = 1804289383;
unsigned int get_random_U32_number() {
    unsigned int number = state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    state = number;
    return number;
}
U64 get_random_U64_number() {
    U64 n1, n2, n3, n4;
    n1 = (U64)(get_random_U32_number()) & 0xFFFF;
    n2 = (U64)(get_random_U32_number()) & 0xFFFF;
    n3 = (U64)(get_random_U32_number()) & 0xFFFF;
    n4 = (U64)(get_random_U32_number()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}
U64 generate_magic_number() {
    return get_random_U64_number() & get_random_U64_number() & get_random_U64_number();
}

U64 find_magic_number(int square, int relevant_bits, int bishop) {
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];
    
    U64 attack_mask = bishop ? mask_bishop_attacks(square) : mask_rook_attacks(square);
    int occupancy_indices = 1 << relevant_bits;
    
    for (int index = 0; index < occupancy_indices; index++) {
        occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);
        attacks[index] = bishop ? bishop_attacks_on_the_fly(square, occupancies[index]) : rook_attacks_on_the_fly(square, occupancies[index]);
    }
    
    for (int random_count = 0; random_count < 100000000; random_count++) {
        U64 magic_number = generate_magic_number();
        if (count_bits((attack_mask * magic_number) & 0xFF00000000000000ULL) < 6) continue;
        
        memset(used_attacks, 0, sizeof(used_attacks));
        int index, fail;
        for (index = 0, fail = 0; !fail && index < occupancy_indices; index++) {
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - relevant_bits));
            if (used_attacks[magic_index] == 0ULL) used_attacks[magic_index] = attacks[index];
            else if (used_attacks[magic_index] != attacks[index]) fail = 1;
        }
        if (!fail) return magic_number;
    }
    return 0ULL;
}

int bishop_relevant_bits[64];
int rook_relevant_bits[64];

int main() {
    for (int sq = 0; sq < 64; sq++) {
        bishop_relevant_bits[sq] = count_bits(mask_bishop_attacks(sq));
        rook_relevant_bits[sq] = count_bits(mask_rook_attacks(sq));
    }
    
    std::cout << "const U64 bishop_magic_numbers[64] = {" << std::endl;
    for (int sq = 0; sq < 64; sq++) {
        std::cout << "0x" << std::hex << find_magic_number(sq, bishop_relevant_bits[sq], 1) << "ULL";
        if (sq != 63) std::cout << ", ";
        if ((sq + 1) % 4 == 0) std::cout << std::endl;
    }
    std::cout << "};\n" << std::endl;
    
    std::cout << "const U64 rook_magic_numbers[64] = {" << std::endl;
    for (int sq = 0; sq < 64; sq++) {
        std::cout << "0x" << std::hex << find_magic_number(sq, rook_relevant_bits[sq], 0) << "ULL";
        if (sq != 63) std::cout << ", ";
        if ((sq + 1) % 4 == 0) std::cout << std::endl;
    }
    std::cout << "};\n" << std::endl;
    return 0;
}
