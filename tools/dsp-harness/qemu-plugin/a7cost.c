/*
 * a7cost: a QEMU TCG plugin that counts what the guest executes, weighted
 * by an estimate of what each instruction costs on the RG Nano's
 * Cortex-A7 (in-order, VFPv4 + 64-bit-wide NEON), and charges it to the
 * part of the program that is running.
 *
 * Wall time under qemu is noisy and badly skewed for this job (every guest
 * float operation becomes a softfloat call on the host, integer code runs
 * almost natively), so the DSP harness measures cost with this instead:
 * deterministic, run to run identical, and closer to how the device spends
 * its cycles.
 *
 * The guest marks what is running with a private system call:
 *   syscall(0x7AB0, slot)   from now on, charge slot (0..63); -2 = an
 *                           audio buffer starts, -1 = it ended (both then
 *                           charge slot 0); each buffer's cost is kept
 *   syscall(0x7AB1)         write the per-slot totals and the buffer costs
 *                           to the file given as plugin argument out=<path>
 *                           (the guest reads it back)
 *   syscall(0x7AB2)         zero the totals and forget the buffers
 *   syscall(0x7AB3)         as 0x7AB1 without the per-block profile (cheap
 *                           enough to call after every audio buffer)
 * qemu answers these with ENOSYS; nothing else happens in the guest.
 *
 * Weights (estimates from the Cortex-A7 TRM and published timings; they
 * are a model, not a cycle count - no pipeline stalls or cache misses):
 *   integer / branch / load / store              1
 *   32x32 multiply 1, long multiply 2, divide    10
 *   VFP single arithmetic, moves, compares       1
 *   VFP double add/sub/compare/move 1, mul 2, multiply-accumulate 3
 *   VDIV.F32 / VSQRT.F32 14, VDIV.F64 / VSQRT.F64 29
 *   VFP <-> core register transfer 2, VMRS 3
 *   VLDM/VSTM/VPUSH/VPOP 1 + registers/2
 *   NEON on D registers 1, on Q registers 2 (the A7 NEON is 64 bits wide),
 *   NEON loads/stores 2
 *
 * The device build is ARM (A32) code throughout (the toolchain defaults to
 * -marm), so instructions are decoded as A32.
 *
 * Build: gcc -shared -fPIC -O2 -I<qemu>/include/qemu a7cost.c -o liba7cost.so
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define SLOTS 64
#define SYS_MARK 0x7AB0
#define SYS_SNAPSHOT 0x7AB1
#define SYS_RESET 0x7AB2
#define SYS_SLOTS 0x7AB3

static uint64_t costTotal, insnTotal;
static uint64_t lastCost, lastInsn;
static uint64_t slotCost[SLOTS], slotInsn[SLOTS];
static uint64_t buffers;
static uint64_t bufferStart;
static uint64_t *bufferCost;
static size_t bufferCount, bufferRoom;
static int current;
static char outPath[1024];

/* VFP data processing (cond 1110 / T32 1110 1110, coproc 101x, bit4 0) */
static unsigned vfpDataCost(uint32_t w) {
	int dbl = (w >> 8) & 1;
	unsigned o = ((w >> 23) & 1) << 3 | ((w >> 20) & 3);   /* opc1 without D */
	switch (o) {
	case 0x0: case 0x1:         /* VMLA/VMLS, VNMLA/VNMLS */
	case 0x9: case 0xA:         /* VFNMA/VFNMS, VFMA/VFMS */
		return dbl ? 3 : 1;
	case 0x2:                   /* VMUL, VNMUL */
		return dbl ? 2 : 1;
	case 0x3:                   /* VADD, VSUB */
		return 1;
	case 0x8:                   /* VDIV */
		return dbl ? 29 : 14;
	case 0xB: {
		unsigned opc2 = (w >> 16) & 0xF;
		unsigned opc3 = (w >> 6) & 3;
		if (opc2 == 1 && opc3 == 3) return dbl ? 29 : 14;   /* VSQRT */
		return 1;               /* VMOV imm/reg, VABS, VNEG, VCMP, VCVT */
	}
	default:
		return 1;
	}
}

/* An A32 instruction (or a T32 one converted to the A32 layout) */
static unsigned a32Cost(uint32_t w) {
	unsigned cond = w >> 28;
	if (cond == 0xF) {
		if ((w & 0xFE000000u) == 0xF2000000u) {
			/* NEON data processing: Q bit (6) doubles the work */
			return (w & 0x40) ? 2 : 1;
		}
		if ((w & 0xFF100000u) == 0xF4000000u) return 2;   /* NEON ld/st */
		return 1;
	}
	unsigned op = (w >> 25) & 7;
	if ((w & 0x0F000E00u) == 0x0E000A00u) {
		if (w & 0x10) {
			/* register transfer: VMRS is the one that waits for the FPU */
			if ((w & 0x00F00F10u) == 0x00F00A10u && ((w >> 20) & 1)) return 3;
			return 2;
		}
		return vfpDataCost(w);
	}
	if ((w & 0x0E000E00u) == 0x0C000A00u) {
		/* VLDR/VSTR/VLDM/VSTM/VPUSH/VPOP, 64-bit core<->VFP moves */
		if ((w & 0x0FE00000u) == 0x0C400000u) return 2;   /* VMOV 2 core regs */
		unsigned p = (w >> 24) & 1, u = (w >> 23) & 1, wb = (w >> 21) & 1;
		if (p == 1 && wb == 0) return 1;                  /* VLDR/VSTR */
		(void)u;
		return 1 + (w & 0xFF) / 2;                        /* multiple */
	}
	if (op == 0 && (w & 0x0F0000F0u) == 0x00000090u) {
		/* multiplies: MUL/MLA 1, long forms 2 */
		return (w & 0x00800000u) ? 2 : 1;
	}
	if ((w & 0x0FD000F0u) == 0x07100010u) return 10;      /* SDIV/UDIV */
	return 1;
}

/* Translated blocks by guest address (open addressing; a block translated
   again keeps its entry) */
#define BLOCKS (1 << 20)
struct Block {
	uint64_t vaddr;
	uint64_t cost;
	uint64_t execs;
	int used;
};
static struct Block *blocks;

static struct Block *findBlock(uint64_t vaddr) {
	if (!blocks) blocks = calloc(BLOCKS, sizeof(struct Block));
	if (!blocks) return 0;
	uint64_t h = (vaddr * 0x9E3779B97F4A7C15ull) >> 44;
	for (int probe = 0; probe < 64; probe++) {
		struct Block *b = &blocks[(h + probe) & (BLOCKS - 1)];
		if (!b->used) {
			b->used = 1;
			b->vaddr = vaddr;
			return b;
		}
		if (b->vaddr == vaddr) return b;
	}
	return 0;
}

static void tbTrans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
	size_t n = qemu_plugin_tb_n_insns(tb);
	uint64_t cost = 0;
	for (size_t i = 0; i < n; i++) {
		struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
		const uint8_t *d = qemu_plugin_insn_data(insn);
		size_t size = qemu_plugin_insn_size(insn);
		if (size == 4) {
			uint32_t w = d[0] | (d[1] << 8) | (d[2] << 16) | ((uint32_t)d[3] << 24);
			cost += a32Cost(w);
		} else {
			cost += 1;   /* Thumb (not used by the device build) */
		}
	}
	qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64, &costTotal, cost);
	qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64, &insnTotal, n);
	/* Where the cost goes: executions per block, reported with the
	   block's address and cost (map the addresses to functions with nm) */
	struct Block *b = findBlock(qemu_plugin_tb_vaddr(tb));
	if (b) {
		b->cost = cost;
		qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64, &b->execs, 1);
	}
}

static void charge(void) {
	slotCost[current] += costTotal - lastCost;
	slotInsn[current] += insnTotal - lastInsn;
	lastCost = costTotal;
	lastInsn = insnTotal;
}

static void snapshot(int withBlocks) {
	if (!outPath[0]) return;
	FILE *f = fopen(outPath, "w");
	if (!f) return;
	fprintf(f, "buffers %llu\n", (unsigned long long)buffers);
	for (size_t i = 0; i < bufferCount; i++) {
		fprintf(f, "buf %llu\n", (unsigned long long)bufferCost[i]);
	}
	for (int s = 0; s < SLOTS; s++) {
		if (slotCost[s] || slotInsn[s]) {
			fprintf(f, "slot %d %llu %llu\n", s, (unsigned long long)slotCost[s],
			        (unsigned long long)slotInsn[s]);
		}
	}
	fclose(f);
	if (!withBlocks) return;
	/* <out>.blocks: address, total cost, executions (since the last reset) */
	char path[1100];
	snprintf(path, sizeof(path), "%s.blocks", outPath);
	f = fopen(path, "w");
	if (!f || !blocks) {
		if (f) fclose(f);
		return;
	}
	for (int i = 0; i < BLOCKS; i++) {
		struct Block *b = &blocks[i];
		if (b->used && b->execs) {
			fprintf(f, "%08llx %llu %llu\n", (unsigned long long)b->vaddr,
			        (unsigned long long)(b->cost * b->execs), (unsigned long long)b->execs);
		}
	}
	fclose(f);
}

static void syscallCb(qemu_plugin_id_t id, unsigned int vcpu, int64_t num, uint64_t a1,
                      uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                      uint64_t a7, uint64_t a8) {
	if (num == SYS_MARK) {
		charge();
		int slot = (int)(int32_t)a1;
		if (slot == -2) {
			bufferStart = costTotal;
			slot = 0;
		} else if (slot == -1) {
			if (bufferCount == bufferRoom) {
				bufferRoom = bufferRoom ? bufferRoom * 2 : 1024;
				bufferCost = realloc(bufferCost, bufferRoom * sizeof(uint64_t));
			}
			bufferCost[bufferCount++] = costTotal - bufferStart;
			buffers++;
			slot = 0;
		} else if (slot < 0) {
			slot = 0;
		}
		current = slot < SLOTS ? slot : 0;
	} else if (num == SYS_SNAPSHOT) {
		charge();
		snapshot(1);
	} else if (num == SYS_SLOTS) {
		charge();
		snapshot(0);
	} else if (num == SYS_RESET) {
		charge();
		memset(slotCost, 0, sizeof(slotCost));
		memset(slotInsn, 0, sizeof(slotInsn));
		buffers = 0;
		bufferCount = 0;
		if (blocks) {
			for (int i = 0; i < BLOCKS; i++) blocks[i].execs = 0;
		}
	}
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                                           int argc, char **argv) {
	for (int i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "out=", 4)) {
			strncpy(outPath, argv[i] + 4, sizeof(outPath) - 1);
		}
	}
	qemu_plugin_register_vcpu_tb_trans_cb(id, tbTrans);
	qemu_plugin_register_vcpu_syscall_cb(id, syscallCb);
	return 0;
}
