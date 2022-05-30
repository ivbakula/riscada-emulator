#ifndef __CORE_H
#define __CORE_H

#include <stdint.h>
#include <stdbool.h>

/* FSM states */
enum {
  STATE_INIT,
  STATE_FETCH,
  STATE_DECODE,
  STATE_EXECUTE,
  STATE_CHECK,
  STATE_BREAK,
  STATE_ERROR,
  STATE_HALT
};

struct FSM_stuff {
  uint8_t current_state;
  uint8_t next_state;
  uint32_t current_instruction;
};

void core_init();
void fsm_init();
bool fsm_cycle_state();

void decode();
union Decoder {
  uint32_t instruction;
  struct {
    uint32_t __block : 3;     // 0
    uint32_t __scheme : 2;    // 3
    uint32_t __opcode : 3;    // 5
    uint32_t __dstreg : 5;    // 8
    uint32_t __srcreg : 5;    // 13
    uint32_t __not_used : 14; // 18
  } common;
  
  struct {
    uint32_t __common : 18;
    uint32_t __src2reg : 5;
    uint32_t __not_used : 9;
  } type_reg;
  
  struct {
    uint32_t __common : 18;
    uint32_t __imm : 14;
  } type_imm;
  
  struct {
    uint32_t __common : 13; // don't use srcreg.
    uint32_t __imm : 18;
  } type_imm_extended;
};

enum { CODING_SCHEME_R, CODING_SCHEME_UI, CODING_SCHEME_SI, CODING_SCHEME_IB };

uint32_t sign_extend(uint32_t n);


/* Execute block */
enum {
  BLOCK_ARITHMETIC,
  BLOCK_MEMORY,
  BLOCK_BRANCH,
  BLOCK_REGISTER,
  BLOCK_PLH_4,
  BLOCK_PLH_5,
  BLOCK_PLH_6,
  BLOCK_CONTROL
};

static inline void inc_pc();
void execute_instruction();

static void execute_arith(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2);
static void execute_memory(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2);
static void execute_branch(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2);
static void execute_ctrl(uint8_t opcode);

enum {
  OPCODE_ADD,
  OPCODE_SUB,
  OPCODE_SHL,
  OPCODE_SHR,
  OPCODE_AND,
  OPCODE_OR,
  OPCODE_NOT,
  OPCODE_XOR
};

enum { OPCODE_LB, OPCODE_LHW, OPCODE_LW, OPCODE_SB, OPCODE_SHW, OPCODE_SW };

enum {
  OPCODE_BR,
  OPCODE_BEQ,
  OPCODE_BLT,
  OPCODE_BLE,
  OPCODE_BGT,
  OPCODE_BGE,
  OPCODE_CMP,
};

/* TODO add these opcodes to enum */
#define OPCODE_BRK 0
#define OPCODE_HALT 7

uint8_t check_ctrl_regs();

#endif
