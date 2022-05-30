#include <stdio.h>

#include "rscs.h"

struct Regfile regfile;
struct SystemMemory memory;
    
void regfile_init()
{
  for (int i = 0; i < END_GP_REGISTERS; i++)
    regfile.gp_registers[i] = 0;

  regfile.gp_registers[REGISTER_PC] = MMIO_SYSTEM_MEMORY_START;
  
  regfile.ctrl_regs.ctrl_brk = 0;
  regfile.ctrl_regs.ctrl_err = 0;
  regfile.ctrl_regs.ctrl_hlt = 0;

  regfile.status_regs.status_if = 0;
  regfile.status_regs.status_nf = 0;
  regfile.status_regs.status_zf = 0;
}

const char *regnames[] = {
    "rz",  "pc",  "fp",  "lr",  "cr",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",
    "r7",  "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17",
    "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27",
};

void regfile_dump_registers()
{
  for (int i = 0; i < END_GP_REGISTERS; i++) {
    printf("%s: %u %d\n", regnames[i], regfile.gp_registers[i], regfile.gp_registers[i]);
  }
}

uint32_t regfile_read(uint8_t reg)
{
  if (reg < END_GP_REGISTERS)
    return regfile.gp_registers[reg];

  switch (reg) {
    case REGISTER_ZF:
      return regfile.status_regs.status_zf;
      
    case REGISTER_NF:
      return regfile.status_regs.status_nf;
      
    case REGISTER_IF:
      return regfile.status_regs.status_if;
      
    case REGISTER_HALT:
      return regfile.ctrl_regs.ctrl_hlt;
      
    case REGISTER_BREAK:
      return regfile.ctrl_regs.ctrl_brk;
      
    case REGISTER_ERROR:
      return regfile.ctrl_regs.ctrl_err;

    default:
      fprintf(stderr, "%s: Error: Invalid register %u\n", __FUNCTION__, reg);
  }

  return -1;
}

void regfile_write(uint8_t reg, uint32_t data)
{
  if (reg > 0 && reg < END_GP_REGISTERS) {
    regfile.gp_registers[reg] = data;
    return;
  }
   
  switch (reg) {
    case REGISTER_ZF:
      regfile.status_regs.status_zf = data;
      break;
      
    case REGISTER_NF:
      regfile.status_regs.status_nf = data;
      break;
      
    case REGISTER_IF:
      regfile.status_regs.status_if = data;
      break;
      
    case REGISTER_HALT:
      regfile.ctrl_regs.ctrl_hlt = data;
      break;
      
    case REGISTER_BREAK:
      regfile.ctrl_regs.ctrl_brk = data;
      break;
      
    case REGISTER_ERROR:
      regfile.ctrl_regs.ctrl_err = data;
      break;

    default:
      fprintf(stderr, "%s: Error: Invalid register %u\n", __FUNCTION__, reg);
  }
}

/* Memory management unit */
static const struct MmioMapEntry mmio_map[] = {
  [VIRT_RESERVED] = {0x000, SIZE_BYTE},
  [VIRT_SPI0] = {0x001, MMIO_SPI_BLOCK_SIZE-1},
  [VIRT_UART0] = {513, SIZE_BYTE},
  [VIRT_UART1] = {514, SIZE_BYTE},
  [VIRT_UART2] = {515, SIZE_BYTE},
  [VIRT_DRAM] = {516, MMIO_SYSTEM_MEMORY_SIZE}
};

bool check_alignment(uint32_t address, uint8_t size)
{
  uint8_t first_memory_cell = address % MMIO_SYSTEM_MEMORY_ALIGN;
  uint8_t last_memory_cell  = first_memory_cell + size - 1;
  bool retval = false;

  
  if (last_memory_cell < 4)
    /* memory is indeed 4 byte aligned */
    retval = true;

  return retval;
}

uint8_t mmu_translate_address(uint32_t address, uint8_t size)
{
  /* Check alignememnt */
  for (int i = 0; i < VIRT_UNKNOWN; i++) {
    uint32_t base = mmio_map[i].base;
    uint32_t end = base + mmio_map[i].size;
    if (address >= base  && address < end)
      return i;
  }

  return VIRT_UNKNOWN;
}

void mmu_write(uint32_t address, uint32_t data, uint8_t size)
{
  uint8_t device = mmu_translate_address(address, size);
  uint32_t converted_address = address - mmio_map[device].base;
  switch (device) {
    case VIRT_RESERVED:
      /* raise null pointer exception */
      fprintf(stderr, "Error in %s. Null pointer exception\n", __FUNCTION__);
      regfile_write(REGISTER_ERROR, true);
      break;
      
    case VIRT_SPI0:
      spi_write(converted_address, data, size);
      break;
      
    case VIRT_UART0:
    case VIRT_UART1:
    case VIRT_UART2:
    case VIRT_UART3:
      uart_write(converted_address, data, size);
      break;
      
    case VIRT_DRAM:
      address = address - mmio_map[VIRT_DRAM].base;
      system_memory_write(converted_address, data, size);
      break;
      
    case VIRT_UNKNOWN:
      /* throw sigbus exception */
      fprintf(stderr, "Bus exception!");
    default:
      break;
  }
}

uint32_t mmu_read(uint32_t address, uint8_t size)
{
  uint8_t device = mmu_translate_address(address, size);
  uint32_t converted_address = address - mmio_map[device].base;
  uint32_t data = 0;
  
  switch (device) {
    case VIRT_RESERVED:
      /* raise null pointer exception */
      fprintf(stderr, "Error in %s. Null pointer exception\n", __FUNCTION__);
      regfile_write(REGISTER_ERROR, true);
      break;
      
    case VIRT_SPI0:
      data = spi_read(converted_address, size);
      break;
      
    case VIRT_UART0:
    case VIRT_UART1:
    case VIRT_UART2:
    case VIRT_UART3:
      data = uart_read(address, size);
      break;
      
    case VIRT_DRAM:
      address = address - mmio_map[VIRT_DRAM].base;
      data = system_memory_read(converted_address, size);
      break;
      
    case VIRT_UNKNOWN:
      /* throw sigbus exception */
      fprintf(stderr, "Error in %s. Bus exception\n", __FUNCTION__);
      regfile_write(REGISTER_ERROR, true);
    default:
      break;
  }
  return data;
}

void mmu_init()
{
  system_memory_init();
}

uint32_t system_memory_read(uint32_t address, uint8_t size)
{
  uint8_t column = address % 4;
  uint16_t row = address / 4;

  uint32_t retval = 0;
  
  switch (size) {
    case SIZE_WORD:
      retval |= memory.memory_block[row][column+3] << 24;
      retval |= memory.memory_block[row][column+2] << 16;
    case SIZE_HWORD:
      retval |= memory.memory_block[row][column+1] << 8;
    case SIZE_BYTE:
      retval |= memory.memory_block[row][column];
  }
  return retval;
}

void system_memory_write(uint32_t address, uint32_t data, uint8_t size)
{
  uint8_t column = address % 4;
  uint16_t row = address / 4;

  switch (size) {
    case SIZE_WORD:
      memory.memory_block[row][column+3] = data & (0xff << 24);
      memory.memory_block[row][column+2] = data & (0xff << 16);
    case SIZE_HWORD:
      memory.memory_block[row][column+1] = data & (0xff << 8);
    case SIZE_BYTE:
      memory.memory_block[row][column] = data & 0xff;
  }
}

void system_memory_init()
{
  for (int i = 0; i < MMIO_SYSTEM_MEMORY_SIZE / MMIO_SYSTEM_MEMORY_ALIGN; i++) {
    for (int j = 0; j < MMIO_SYSTEM_MEMORY_ALIGN; j++) {
      memory.memory_block[i][j] = 0;
    }
  }

  /* add r1, rz, 513 */
  memory.memory_block[0][3] = 0x08;
  memory.memory_block[0][2] = 0x04;
  memory.memory_block[0][1] = 0x06;
  memory.memory_block[0][0] = 0x08;

  /* sb r1, rz, 'h' */
  memory.memory_block[1][3] = 0x01;
  memory.memory_block[1][2] = 0xa0;
  memory.memory_block[1][1] = 0x06;
  memory.memory_block[1][0] = 0x69;

  /* sb r1, rz, '\n' */
  memory.memory_block[2][3] = 0x00;
  memory.memory_block[2][2] = 0x28;
  memory.memory_block[2][1] = 0x06;
  memory.memory_block[2][0] = 0x69;
    
  /* hlt */
  memory.memory_block[3][3] = 0x00;
  memory.memory_block[3][2] = 0x00;
  memory.memory_block[3][1] = 0x00;
  memory.memory_block[3][0] = 0xe7;
}

void uart_write(uint32_t address, uint32_t data, uint8_t size)
{
  if (size == SIZE_BYTE)
    putchar(data);
  else
    fprintf(stderr, "UART invalid size: %d\n", size);
}

uint32_t uart_read(uint32_t address, uint8_t size)
{
  return 0;
}

void spi_write(uint32_t address, uint32_t data, uint8_t size) { return; }
uint32_t spi_read(uint32_t address, uint8_t size) { return 0; }
