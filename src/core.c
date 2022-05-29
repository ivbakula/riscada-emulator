#include <stdint.h>
#include <stdio.h>

#include "rscs.h"
#include "core.h"

extern struct Regfile regfile;

static uint8_t fsm_current_state;
static uint8_t fsm_next_state;

static union Decoder decoder;
static uint32_t execute_op1;
static uint32_t execute_op2;

void core_init()
{
  fsm_init();
  regfile_init();
  mmu_init();
}

void fsm_init()
{
  fsm_current_state = STATE_INIT;
  fsm_next_state = STATE_INIT;
  decoder.instruction = 0;
}

bool fsm_cycle_state()
{
  bool retval = true;
  uint8_t check = 0;
  
  switch (fsm_current_state) {
    case STATE_INIT:
      fsm_next_state = STATE_FETCH;
      break;
      
    case STATE_FETCH:
      decoder.instruction = mmu_read(regfile.gp_registers[REGISTER_PC], SIZE_WORD);
      fsm_next_state = STATE_DECODE;
      break;
      
    case STATE_DECODE:
      decode();
      fsm_next_state = STATE_EXECUTE;
      break;
      
    case STATE_EXECUTE:
      execute_instruction();
      fsm_next_state = STATE_CHECK;
      break;
          
    case STATE_CHECK:
      fsm_next_state = check_ctrl_regs();
      break;
      
    case STATE_BREAK:
      fsm_next_state = STATE_BREAK;
      break;
      
    case STATE_ERROR:
      fprintf(stderr, "Error occured\n");

      printf("INSTRUCTION: \n");
      printf("0x%08x\n", decoder.instruction);
          
      printf("REGISTERS: \n");
      regfile_dump_registers();
      retval = false;
      break;
      
    case STATE_HALT:
      retval = false;
      break;
      
    default:
      fprintf(stderr, "Invalid state: %d\n", fsm_current_state);
      break;
  }

  fsm_current_state = fsm_next_state;
  return retval;
}

void decode()
{
  uint32_t op1;
  uint32_t op2;

  switch (decoder.common.__scheme) {
    case CODING_SCHEME_R:
      execute_op1 = regfile.gp_registers[decoder.common.__srcreg];
      execute_op2 = regfile.gp_registers[decoder.type_reg.__src2reg];
      break;
      
    case CODING_SCHEME_SI:
      execute_op1 = regfile.gp_registers[decoder.common.__srcreg];
      execute_op2 = sign_extend(decoder.type_imm.__imm);
      break;
      
    case CODING_SCHEME_UI:
      execute_op1 = regfile.gp_registers[decoder.common.__srcreg];
      execute_op2 = decoder.type_imm.__imm;
      break;
      
    case CODING_SCHEME_IB:
      execute_op1 = 0;
      execute_op2 = decoder.type_imm_extended.__imm;
      break;
      
    default:
      fprintf(stderr, "Unknown coding scheme: %u\n", decoder.common.__scheme);
      break;
  }
}

uint32_t sign_extend(uint32_t n)
{
  const uint32_t imm_max = (1 << 14) - 1;
  const uint32_t mask = UINT32_MAX & (~imm_max);
  const uint32_t sign_bit = 1 << 13;

  if (n & sign_bit)
    n |= mask;

  return n;
}

void inc_pc()
{
  regfile.gp_registers[REGISTER_PC] = regfile.gp_registers[REGISTER_PC] + 4;
}

void execute_instruction()
{
  switch (decoder.common.__block) {
    case BLOCK_ARITHMETIC:
      execute_arith(decoder.common.__opcode, decoder.common.__dstreg, execute_op1, execute_op2);
      break;
      
    case BLOCK_MEMORY:
      execute_memory(decoder.common.__opcode, decoder.common.__dstreg, execute_op1, execute_op2);
      break;
      
    case BLOCK_BRANCH:
      execute_branch(decoder.common.__opcode, decoder.common.__dstreg, execute_op1, execute_op2);
      break;
      
    case BLOCK_CONTROL:
      execute_ctrl(decoder.common.__opcode);
      break;
      
    default:
      fprintf(stderr, "Block not implemented!\n");
      regfile.ctrl_regs.ctrl_err = true;
      break;
  }
}

void execute_arith(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2)
{
  switch (opcode) {
    case OPCODE_ADD:
      regfile.gp_registers[dstreg] = op1 + op2;
      break;

    case OPCODE_SUB:
      regfile.gp_registers[dstreg] = op1 - op2;
      break;
      
    case OPCODE_SHL:
      regfile.gp_registers[dstreg] = op1 << op2;
      break;
      
    case OPCODE_SHR:
      regfile.gp_registers[dstreg] = op1 >> op2;
      break;
      
    case OPCODE_AND:
      regfile.gp_registers[dstreg] = op1 & op2;
      break;
      
    case OPCODE_OR:
      regfile.gp_registers[dstreg] = op1 | op2;
      break;
      
    case OPCODE_NOT:
      regfile.gp_registers[dstreg] = ~regfile.gp_registers[dstreg];
      break;
      
    case OPCODE_XOR:
      regfile.gp_registers[dstreg] = op1 ^ op2;
      break;
      
    default:
      fprintf(stderr, "Error! Invalid opcode: %d\n", opcode);
  }
  inc_pc();
}

void execute_memory(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2)
{
  uint32_t ptr = regfile.gp_registers[dstreg];
  switch (opcode) {
    case OPCODE_LB:
      regfile.gp_registers[dstreg] = mmu_read(op1 + op2, SIZE_BYTE);
      break;
      
    case OPCODE_LHW:
      regfile.gp_registers[dstreg] = mmu_read(op1 + op2, SIZE_HWORD);
      break;

    case OPCODE_LW:
      regfile.gp_registers[dstreg] = mmu_read(op1 + op2, SIZE_WORD);
      break;
      
    case OPCODE_SB:
      mmu_write(ptr + op1, op2, SIZE_BYTE);
      break;

    case OPCODE_SHW:
      mmu_write(ptr + op1, op2, SIZE_HWORD);
      break;
          
    case OPCODE_SW:
      mmu_write(ptr + op1, op2, SIZE_WORD);
      break;
      
    default:
      fprintf(stderr, "Error in %s! Invalid opcode: %d\n", __FUNCTION__, opcode);
      break;
  }

  inc_pc();
}

void execute_ctrl(uint8_t opcode)
{
  switch (opcode) {
    case OPCODE_HALT:
      regfile_write(REGISTER_HALT, true);
      break;
      
    case OPCODE_BRK:
      regfile_write(REGISTER_BREAK, true);
      break;
      
    default:
      fprintf(stderr, "Error in %s! Opcode %d not implemented\n", __FUNCTION__, opcode);
  }

  inc_pc();
}

void execute_branch(uint8_t opcode, uint8_t dstreg, uint32_t op1, uint32_t op2)
{
  bool take_jump = false;
  bool __zf = regfile_read(REGISTER_ZF);
  bool __nf = regfile_read(REGISTER_NF);
  int32_t comparator = 0;
  
  switch (opcode) {
    case OPCODE_BR:
      take_jump = true;
      break;
      
    case OPCODE_BEQ:
      take_jump = __zf;
      break;
      
    case OPCODE_BLT:
      take_jump = __nf;
      break;
      
    case OPCODE_BLE:
      take_jump = __zf || __nf;
      break;
      
    case OPCODE_BGT:
      take_jump = (!__zf) && (!__nf);
      break;
      
    case OPCODE_BGE:
      take_jump = !__nf;
      break;
      
    case OPCODE_CMP:
      comparator = op1 - op2;
      if (comparator < 0) {
        regfile_write(REGISTER_ZF, false); // set zero flag low
        regfile_write(REGISTER_NF, true);  // set negative flag high
      } else if (comparator == 0) {
        regfile_write(REGISTER_ZF, true);  // set zero flag high
        regfile_write(REGISTER_NF, false); // set negative flag low
      } else {
        regfile_write(REGISTER_ZF, false); // set zero flag low
        regfile_write(REGISTER_NF, false); // set negative flag low
      }
      break;
      
    default:
      fprintf(stderr, "Error in %s! Opcode %d not implemented", __FUNCTION__, opcode);
      break;
  }

  if (take_jump) {
    regfile.gp_registers[dstreg] = op1 + op2;
  } else {
    inc_pc();
  }
}

uint8_t check_ctrl_regs()
{
  return (regfile_read(REGISTER_HALT) ? STATE_HALT :
          (regfile_read(REGISTER_BREAK) ? STATE_BREAK :
           (regfile_read(REGISTER_ERROR) ? STATE_ERROR : STATE_FETCH)));
}
