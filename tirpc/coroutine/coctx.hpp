#pragma once

namespace tirpc {

enum {
  kRBP = 6,      // rbp, bottom of stack
  kRDI = 7,      // rdi, first para when call function
  kRSI = 8,      // rsi, second para when call function
  kRETAddr = 9,  // the next excute cmd address, it will be assigned to rip
  kRSP = 13,     // rsp, top of stack
};

struct CoCtx {
  void *regs_[14];
};

extern "C" {

extern void CoctxSwap(CoCtx *, CoCtx *) asm("coctx_swap");
}

}  // namespace tirpc