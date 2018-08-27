#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "memory_manager.h"
#include "writer-arm64.h"

inline void ReadBytes(void *data, void *address, int length) {
  memcpy(data, address, length);
}

ARM64AssemblyWriter *arm64_assembly_writer_cclass(new)(void *pc) {
  ARM64AssemblyWriter *writer = SAFE_MALLOC_TYPE(ARM64AssemblyWriter);
  writer->pc                  = pc;
  writer->instCTXs            = list_new();
  writer->inst_bytes          = buffer_array_create(64);
  writer->ldr_address_stubs   = list_new();
  return writer;
}

void arm64_assembly_writer_cclass(destory)(ARM64AssemblyWriter *self) {
}

void arm64_assembly_writer_cclass(reset)(ARM64AssemblyWriter *self, void *pc) {
  self->pc = pc;

  list_destroy(self->instCTXs);
  self->instCTXs = list_new();

  list_destroy(self->ldr_address_stubs);
  self->ldr_address_stubs = list_new();

  buffer_array_clear(self->inst_bytes);
  return;
}

void arm64_assembly_writer_cclass(patch_to)(ARM64AssemblyWriter *self, void *target_address) {
  self->buffer = target_address;
  memory_manager_t *memory_manager;
  memory_manager = memory_manager_cclass(shared_instance)();
  memory_manager_cclass(patch_code)(memory_manager, target_address, self->inst_bytes->data, self->inst_bytes->size);
  return;
}

size_t arm64_assembly_writer_cclass(bxxx_range)() {
  return ((1 << 23) << 2);
}

#define ARM64_INST_SIZE 4

void arm64_assembly_writer_cclass(put_bytes)(ARM64AssemblyWriter *self, void *data, int length) {
  assert(length % 4 == 0);
  for (int i = 0; i < (length / ARM64_INST_SIZE); i++) {
    ARM64InstructionCTX *instCTX = SAFE_MALLOC_TYPE(ARM64InstructionCTX);
    instCTX->pc                  = (zz_addr_t)self->pc + self->instCTXs->len * ARM64_INST_SIZE;
    instCTX->size                = ARM64_INST_SIZE;
    ReadBytes(&instCTX->bytes, (void *)((zz_addr_t)data + ARM64_INST_SIZE * i), ARM64_INST_SIZE);

    buffer_array_put(self->inst_bytes, (void *)((zz_addr_t)data + ARM64_INST_SIZE * i), ARM64_INST_SIZE);

    list_rpush(self->instCTXs, list_node_new(instCTX));
  }
}

void arm64_assembly_writer_cclass(put_ldr_literal_reg_offset)(ARM64AssemblyWriter *self, ARM64Reg reg,
                                                              uint32_t offset) {
  ARM64RegInfo ri;
  arm64_register_describe(reg, &ri);

  uint32_t imm19, Rt;
  imm19         = offset >> 2;
  Rt            = ri.index;
  uint32_t inst = 0x58000000 | imm19 << 5 | Rt;

  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}
void arm64_assembly_writer_cclass(put_str_reg_reg_offset)(ARM64AssemblyWriter *self, ARM64Reg src_reg,
                                                          ARM64Reg dest_reg, uint64_t offset) {
  ARM64RegInfo reg_ctx, rd;
  arm64_register_describe(src_reg, &reg_ctx);
  arm64_register_describe(dest_reg, &rd);

  uint32_t size, v = 0, opc = 0, Rn_ndx, Rt_ndx;
  Rn_ndx = rd.index;
  Rt_ndx = reg_ctx.index;

  if (reg_ctx.is_integer) {
    size = (reg_ctx.width == 64) ? 0b11 : 0b10;
  }

  uint32_t imm12 = offset >> size;
  uint32_t inst  = 0x39000000 | size << 30 | opc << 22 | imm12 << 10 | Rn_ndx << 5 | Rt_ndx;
  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}

void arm64_assembly_writer_cclass(put_ldr_reg_reg_offset)(ARM64AssemblyWriter *self, ARM64Reg dest_reg,
                                                          ARM64Reg src_reg, uint64_t offset) {
  ARM64RegInfo reg_ctx, rd;
  arm64_register_describe(src_reg, &reg_ctx);
  arm64_register_describe(dest_reg, &rd);

  uint32_t size, v = 0, opc = 0b01, Rn_ndx, Rt_ndx;
  Rn_ndx = reg_ctx.index;
  Rt_ndx = rd.index;

  if (reg_ctx.is_integer) {
    size = (reg_ctx.width == 64) ? 0b11 : 0b10;
  }

  uint32_t imm12 = offset >> size;
  uint32_t inst  = 0x39000000 | size << 30 | opc << 22 | imm12 << 10 | Rn_ndx << 5 | Rt_ndx;
  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}

void arm64_assembly_writer_cclass(put_br_reg)(ARM64AssemblyWriter *self, ARM64Reg reg) {
  ARM64RegInfo ri;
  arm64_register_describe(reg, &ri);

  uint32_t op   = 0, Rn_ndx;
  Rn_ndx        = ri.index;
  uint32_t inst = 0xd61f0000 | op << 21 | Rn_ndx << 5;
  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}

void arm64_assembly_writer_cclass(put_blr_reg)(ARM64AssemblyWriter *self, ARM64Reg reg) {
  ARM64RegInfo ri;
  arm64_register_describe(reg, &ri);

  uint32_t op = 0b01, Rn_ndx;

  Rn_ndx        = ri.index;
  uint32_t inst = 0xd63f0000 | op << 21 | Rn_ndx << 5;
  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}

void arm64_assembly_writer_cclass(put_b_imm)(ARM64AssemblyWriter *self, uint64_t imm) {
  uint32_t op   = 0b0, imm26;
  imm26         = (imm >> 2) & 0x03ffffff;
  uint32_t inst = 0x14000000 | op << 31 | imm26;
  arm64_assembly_writer_cclass(put_bytes)(self, (void *)&inst, 4);
}

static void arm64_assembly_writer_register_ldr_address_stub(ARM64AssemblyWriter *writer, int ldr_inst_index,
                                                            zz_addr_t address) {
  ldr_address_stub_t *ldr_stub = SAFE_MALLOC_TYPE(ldr_address_stub_t);
  ldr_stub->address            = address;
  ldr_stub->ldr_inst_index     = ldr_inst_index;
  list_lpush(writer->ldr_address_stubs, list_node_new(ldr_stub));
  return;
}

// combine instructions set.
// 0x4: ldr reg, #label
void arm64_assembly_writer_cclass(put_load_reg_address)(ARM64AssemblyWriter *self, ARM64Reg reg, zz_addr_t address) {
  arm64_assembly_writer_register_ldr_address_stub(self, self->instCTXs->len, address);
  arm64_assembly_writer_cclass(put_ldr_literal_reg_offset)(self, reg, -1);
}