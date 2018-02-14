/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef JIT_IL_H_
#define JIT_IL_H_

// for union Sh4OpArgs
#include "hw/sh4/sh4_inst.h"

enum jit_opcode {
    // this opcode calls an interpreter function
    JIT_OP_FALLBACK,

    // This jumps to the jump destination address previously stored
    JIT_OP_JUMP,

    // this will jump iff the conditional jump flag is set
    JIT_JUMP_COND,

    // this will set a register to the given constant value
    JIT_SET_SLOT,

    // this will copy a slot into SR and handle any state changes
    JIT_OP_RESTORE_SR,

    // read 16 bits from a constant address and store them in a given slot
    JIT_OP_READ_16_CONSTADDR,

    // sign-extend a 16-bit int in a slot into a 32-bit int
    JIT_OP_SIGN_EXTEND_16,

    // read a 32-bit int at a constant address into a slot
    JIT_OP_READ_32_CONSTADDR,

    // read a 32-bit int at an address contained in a slot into another slot
    JIT_OP_READ_32_SLOT,

    /*
     * load 16-bits from a host memory address into a jit register
     * upper 16-bits should be zero-extended.
     */
    JIT_OP_LOAD_SLOT16,

    // load 32-bits from a host memory address into a jit register
    JIT_OP_LOAD_SLOT,

    // store 32-bits from a jit register address into a host memory address
    JIT_OP_STORE_SLOT,

    // add one slot into another
    JIT_OP_ADD,

    // subtract one slot from another
    JIT_OP_SUB,

    // add a 32-bit constant value into a slot
    JIT_OP_ADD_CONST32,

    // xor one slot into another
    JIT_OP_XOR,

    // XOR one slot with a 32-bit constant
    JIT_OP_XOR_CONST32,

    // move one slot into another
    JIT_OP_MOV,

    // AND one slot with another
    JIT_OP_AND,

    // AND one slot with a 32-bit constant
    JIT_OP_AND_CONST32,

    // OR one slot with another
    JIT_OP_OR,

    // OR one slot with a 32-bit constant
    JIT_OP_OR_CONST32,

    // set the given slot to 1 if any bits are set, else 0
    JIT_OP_SLOT_TO_BOOL,

    // place one's-compliment of given slot into another slot
    JIT_OP_NOT,

    /*
     * This tells the backend that a given slot is no longer needed and its
     * value does not need to be preserved.
     */
    JIT_OP_DISCARD_SLOT
};

struct jit_fallback_immed {
    void(*fallback_fn)(Sh4*,Sh4OpArgs);
    Sh4OpArgs inst;
};

struct jump_immed {
    // this should point to the slot where the jump address is stored
    unsigned slot_no;
};

struct jump_cond_immed {
    /*
     * this should point to SR, but really it can point to any register.
     *
     * But it should point to SR.
     */
    unsigned slot_no;

    unsigned jmp_addr_slot, alt_jmp_addr_slot;

    /*
     * expected value of the t_flag (either 0 or 1).  the conditional jump will
     * go to the jump address if bit 0 in the given slot matches this expected
     * value.  Otherwise, it will go to the alt jump address.
     */
    unsigned t_flag;
};

struct set_slot_immed {
    unsigned slot_idx;
    uint32_t new_val;
};

struct restore_sr_immed {
    unsigned slot_no;
};

struct read_16_constaddr_immed {
    addr32_t addr;
    unsigned slot_no;
};

struct sign_extend_16_immed {
    unsigned slot_no;
};

struct read_32_constaddr_immed {
    addr32_t addr;
    unsigned slot_no;
};

struct read_32_slot_immed {
    unsigned addr_slot;
    unsigned dst_slot;
};

struct load_slot16_immed {
    uint16_t const *src;
    unsigned slot_no;
};

struct load_slot_immed {
    uint32_t const *src;
    unsigned slot_no;
};

struct store_slot_immed {
    uint32_t *dst;
    unsigned slot_no;
};

struct add_immed {
    unsigned slot_src, slot_dst;
};

struct sub_immed {
    unsigned slot_src, slot_dst;
};

struct add_const32_immed {
    unsigned slot_dst;
    uint32_t const32;
};

struct discard_slot_immed {
    unsigned slot_no;
};

struct xor_immed {
    unsigned slot_src, slot_dst;
};

struct xor_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct mov_immed {
    unsigned slot_src, slot_dst;
};

struct and_immed {
    unsigned slot_src, slot_dst;
};

struct and_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct or_immed {
    unsigned slot_src, slot_dst;
};

struct or_const32_immed {
    unsigned slot_no;
    unsigned const32;
};

struct slot_to_bool_immed {
    unsigned slot_no;
};

struct not_immed {
    unsigned slot_no;
};

union jit_immed {
    struct jit_fallback_immed fallback;
    struct jump_immed jump;
    struct jump_cond_immed jump_cond;
    struct set_slot_immed set_slot;
    struct restore_sr_immed restore_sr;
    struct read_16_constaddr_immed read_16_constaddr;
    struct sign_extend_16_immed sign_extend_16;
    struct read_32_constaddr_immed read_32_constaddr;
    struct read_32_slot_immed read_32_slot;
    struct load_slot16_immed load_slot16;
    struct load_slot_immed load_slot;
    struct store_slot_immed store_slot;
    struct add_immed add;
    struct sub_immed sub;
    struct add_const32_immed add_const32;
    struct discard_slot_immed discard_slot;
    struct xor_immed xor;
    struct xor_const32_immed xor_const32;
    struct mov_immed mov;
    struct and_immed and;
    struct and_const32_immed and_const32;
    struct or_immed or;
    struct or_const32_immed or_const32;
    struct slot_to_bool_immed slot_to_bool;
    struct not_immed not;
};

struct jit_inst {
    enum jit_opcode op;
    union jit_immed immed;
};

struct il_code_block;

void jit_fallback(struct il_code_block *block,
                  void(*fallback_fn)(Sh4*,Sh4OpArgs), inst_t inst);
void jit_jump(struct il_code_block *block, unsigned slot_no);
void jit_jump_cond(struct il_code_block *block,
                   unsigned slot_no, unsigned jmp_addr_slot,
                   unsigned alt_jmp_addr_slot, unsigned t_val);
void jit_set_slot(struct il_code_block *block, unsigned slot_idx,
                  uint32_t new_val);
void jit_restore_sr(struct il_code_block *block, unsigned slot_no);
void jit_read_16_constaddr(struct il_code_block *block, addr32_t addr,
                      unsigned slot_no);
void jit_sign_extend_16(struct il_code_block *block, unsigned slot_no);
void jit_read_32_constaddr(struct il_code_block *block, addr32_t addr,
                           unsigned slot_no);
void jit_read_32_slot(struct il_code_block *block, unsigned addr_slot,
                      unsigned dst_slot);
void jit_load_slot(struct il_code_block *block, unsigned slot_no,
                   uint32_t const *src);
void jit_load_slot16(struct il_code_block *block, unsigned slot_no,
                     uint16_t const *src);
void jit_store_slot(struct il_code_block *block, unsigned slot_no,
                    uint32_t *dst);
void jit_add(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_sub(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_add_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32);
void jit_discard_slot(struct il_code_block *block, unsigned slot_no);
void jit_xor(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_xor_const32(struct il_code_block *block, unsigned slot_no,
                     uint32_t const32);
void jit_mov(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_and(struct il_code_block *block, unsigned slot_src,
             unsigned slot_dst);
void jit_and_const32(struct il_code_block *block, unsigned slot_src,
                     unsigned const32);
void jit_or(struct il_code_block *block, unsigned slot_src,
            unsigned slot_dst);
void jit_or_const32(struct il_code_block *block, unsigned slot_no,
                    unsigned const32);
void jit_slot_to_bool(struct il_code_block *block, unsigned slot_no);
void jit_not(struct il_code_block *block, unsigned slot_no);

#endif
