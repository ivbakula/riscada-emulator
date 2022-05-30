#ifndef __RSCS_H
#define __RSCS_H

#include <stdint.h>
#include <stdbool.h>

/* General purpose registers */
enum {
  REGISTER_RZ,
  REGISTER_PC,
  REGISTER_FP,
  REGISTER_LR,
  REGISTER_CR,
  REGISTER_R1,
  REGISTER_R2,
  REGISTER_R3,
  REGISTER_R4,
  REGISTER_R5,
  REGISTER_R6,
  REGISTER_R7,
  REGISTER_R8,
  REGISTER_R9,
  REGISTER_R10,
  REGISTER_R11,
  REGISTER_R12,
  REGISTER_R13,
  REGISTER_R14,
  REGISTER_R15,
  REGISTER_R16,
  REGISTER_R17,
  REGISTER_R18,
  REGISTER_R19,
  REGISTER_R20,
  REGISTER_R21,
  REGISTER_R22,
  REGISTER_R23,
  REGISTER_R24,
  REGISTER_R25,
  REGISTER_R26,
  REGISTER_R27,

  END_GP_REGISTERS,

  /* status registers */
  REGISTER_ZF,
  REGISTER_NF,
  REGISTER_IF,

  END_STATUS_REGISTERS,
  
  /* control registers */
  REGISTER_HALT,
  REGISTER_BREAK,
  REGISTER_ERROR,

  END_CTRL_REGISTERS
};

#define GENERAL_PURPOSE_REGISTER_COUNT 32

struct Regfile {
  uint32_t gp_registers[GENERAL_PURPOSE_REGISTER_COUNT];
  
  struct {
    uint8_t status_zf : 1; // zero flag
    uint8_t status_nf : 1; // negative flag
    uint8_t status_if : 1; // interrupt flag
    uint8_t __unused  : 5;
  } status_regs;

  struct {
    uint8_t ctrl_hlt : 1; // halt flag
    uint8_t ctrl_brk : 1; // break flag
    uint8_t ctrl_err : 1; // error flag
    uint8_t __unused : 5;
  } ctrl_regs;
};

uint32_t regfile_read(uint8_t reg);
void regfile_write(uint8_t reg, uint32_t data);
void regfile_init();
void regfile_dump_registers();

#define LOGIC_HIGH 1
#define LOGIC_LOW  0

#define GET_PC() \
  read_register(REGISTER_PC)

#define INC_PC() \
  write_register(REGISTER_PC, read_register(REGISTER_PC) + 4)

/* Memory stuff */
#define SIZE_WORD 4
#define SIZE_HWORD 2
#define SIZE_BYTE 1

#define MMIO_RESERVED_NULL 0x0

#define MMIO_SPI_START 0x1
#define MMIO_SPI_BLOCK_SIZE 512
#define MMIO_SPI_END MMIO_SPI_START + MMIO_SPI_BLOCK_SIZE

#define MMIO_UART_0 MMIO_SPI_END
#define MMIO_UART_1 MMIO_UART_0 + 1
#define MMIO_UART_2 MMIO_UART_1 + 1

#define MMIO_SYSTEM_MEMORY_ALIGN 4 // four byte alignement
#define MMIO_SYSTEM_MEMORY_SIZE  4096 // 4kB
#define MMIO_SYSTEM_MEMORY_START MMIO_UART_2 + 1
#define MMIO_SYSTEM_MEMORY_END MMIO_SYSTEM_MEMORY_START + MMIO_SYSTEM_MEMORY_SIZE

enum {
  VIRT_RESERVED,
  VIRT_SPI0,
  VIRT_UART0,
  VIRT_UART1,
  VIRT_UART2,
  VIRT_UART3,
  VIRT_DRAM,
  VIRT_UNKNOWN,
};

struct MmioMapEntry {
  uint32_t base;
  uint32_t size;
};

/* TODO: implement memory map like in https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c.
 * To find appropriate device, you should iterate through this map and check if address
 * falls in interval between base address and base address + size.
 * This map should have one more field: pointer to devices read and write functions.
 */
/* System memory */

void system_memory_write(uint32_t address, uint32_t data, uint8_t size);
uint32_t system_memory_read(uint32_t address, uint8_t size);
void system_memory_init();

struct SystemMemory {
  uint8_t memory_block[MMIO_SYSTEM_MEMORY_SIZE/MMIO_SYSTEM_MEMORY_ALIGN][MMIO_SYSTEM_MEMORY_ALIGN];
};

/* Uart device */
void uart_write(uint32_t address, uint32_t data, uint8_t size);
uint32_t uart_read(uint32_t address, uint8_t size);

/* SPI device */
void spi_write(uint32_t address, uint32_t data, uint8_t size);
uint32_t spi_read(uint32_t address, uint8_t size);

void invalid_address_write_handler(uint32_t address, uint32_t data,
                                   uint8_t size);
uint32_t invalid_address_read_handler(uint32_t address, uint32_t size);

/* Memory management unit */
enum {
  MMIO_DEVICE_SYS_MEMORY,
  
  MMIO_DEVICE_UART,

  MMIO_DEVICE_SPI
};

bool check_alignment(uint32_t address, uint8_t size);
uint8_t mmu_translate_address(uint32_t address, uint8_t size);

void mmu_write(uint32_t address, uint32_t data, uint8_t size);
uint32_t mmu_read(uint32_t address, uint8_t size);
void mmu_init();

typedef void (*MMIO_DEVICE_WRITE[])(uint32_t address, uint32_t data, uint8_t size);
typedef uint32_t (*MMIO_DEVICE_READ)(uint32_t address, uint8_t size);

#endif
