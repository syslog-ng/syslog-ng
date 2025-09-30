/*
 * Copyright (c) 2024 Balázs Scheidler <balazs.scheidler@axoflow.com>
 * Copyright (c) 2024 Axoflow
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "stackdump.h"

#if SYSLOG_NG_ENABLE_STACKDUMP

#include "console.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <dlfcn.h>


static void
_stackdump_print_stack(gpointer stack_pointer)
{
  guint8 *p = (guint8 *) stack_pointer;

  for (gint i = 0; i < 16; i++)
    {
      gchar line[64] = {0};  /* plenty of room for 16 bytes in hex */
      for (gint j = 0; j < 8; j++)
        {
          g_snprintf(&line[j*3], sizeof(line) - j*3, "%02x ", (guint) *(p+j));
        }
      line[8*3] = ' ';
      for (gint j = 8; j < 16; j++)
        {
          g_snprintf(&line[j*3 + 1], sizeof(line) - (j*3 + 1), "%02x ", (guint) *(p+j));
        }

      console_printf("Stack %p: %s", p, line);
      p += 16;
    }
}

#ifdef __linux__
/****************************************************************************
 * Linux
 ****************************************************************************/

#include <link.h>
#include <ucontext.h>

#define _STRUCT_MCONTEXT mcontext_t

static void
_stackdump_print_backtrace(void)
{
  void *bt[256];
  gint count = unw_backtrace(bt, 256);

  console_printf("Backtrace dump, count=%d", count);
  for (gint i = 0; i < count; i++)
    {
      Dl_info dlinfo;
      struct link_map *linkmap;

      if (!dladdr1(bt[i], &dlinfo, (void **) &linkmap, RTLD_DL_LINKMAP))
        {
          console_printf("[%d]: %p", i, bt[i]);
        }
      else
        {
          if (dlinfo.dli_sname == NULL || dlinfo.dli_sname[0] == '\0')
            {
              ptrdiff_t addr_rel_to_base = (char *) bt[i] - (char *) linkmap->l_addr;
              console_printf("[%d]: %s(+0x%lx) [%p]", i, dlinfo.dli_fname, (guint64) addr_rel_to_base, bt[i]);
            }
          else
            {
              ptrdiff_t addr_rel_to_sym = (char *) bt[i] - (char *) dlinfo.dli_saddr;
              console_printf("[%d]: %s(%s+0x%lx) [%p]", i, dlinfo.dli_fname, dlinfo.dli_sname, addr_rel_to_sym, bt[i]);
            }
        }
    }
}

#elif defined(__APPLE__) || defined(__FreeBSD__)
/****************************************************************************
 * macOS / FreeBSD
 ****************************************************************************/

#if defined(__APPLE__)
#define _XOPEN_SOURCE 1
#else
#include <inttypes.h>
#define _STRUCT_MCONTEXT mcontext_t
#endif
#include <ucontext.h>
#include <execinfo.h>

static void
_stackdump_print_backtrace(void)
{
  void *bt[256];
  gint count = backtrace(bt, 256);

  console_printf("Backtrace dump, count=%d", count);

  for (gint i = 0; i < count; i++)
    {
      Dl_info dlinfo;
      if (!dladdr(bt[i], &dlinfo))
        console_printf("[%d]: %p", i, bt[i]);
      else
        {
          if (dlinfo.dli_sname == NULL || dlinfo.dli_sname[0] == '\0')
            console_printf("[%d]: %s [%p]", i, dlinfo.dli_fname ? dlinfo.dli_fname : "?", bt[i]);
          else
            console_printf("[%d]: %s(%s) [%p]", i, dlinfo.dli_fname ? dlinfo.dli_fname : "?", dlinfo.dli_sname, bt[i]);
        }
    }
}

#else // #elif defined(__APPLE__) || defined(__FreeBSD__)
/****************************************************************************
 * unsupported OS
 ****************************************************************************/
#error "This is an unsupported OS for stackdump currenty :("
#endif // OS variants

/*
 * Reguster accessor hepers by OS / Architecture
 */

#if defined(__linux__)
/****************************************************************************
 * Linux x86_64
 ****************************************************************************/
# define REG_RAX (p->gregs[REG_RAX])
# define REG_RBX (p->gregs[REG_RBX])
# define REG_RCX (p->gregs[REG_RCX])
# define REG_RDX (p->gregs[REG_RDX])
# define REG_RSI (p->gregs[REG_RSI])
# define REG_RDI (p->gregs[REG_RDI])
# define REG_RBP (p->gregs[REG_RBP])
# define REG_RSP (p->gregs[REG_RSP])
# define REG_R8  (p->gregs[REG_R8])
# define REG_R9  (p->gregs[REG_R9])
# define REG_R10 (p->gregs[REG_R10])
# define REG_R11 (p->gregs[REG_R11])
# define REG_R12 (p->gregs[REG_R12])
# define REG_R13 (p->gregs[REG_R13])
# define REG_R14 (p->gregs[REG_R14])
# define REG_R15 (p->gregs[REG_R15])
# define REG_RIP (p->gregs[REG_RIP])
/****************************************************************************
 * Linux arm64
 ****************************************************************************/
# define REG_X(n)   (p->regs[n])
# define REG_FP     (p->regs[29])
# define REG_LR     (p->regs[30])
# define REG_SP     (p->sp)
# define REG_PC     (p->pc)
# define REG_PSTATE (p->pstate)
# define FMT_PSTATE "%016llx"
/****************************************************************************
 * Linux i386
 ****************************************************************************/
# define REG_EAX (p->gregs[REG_EAX])
# define REG_EBX (p->gregs[REG_EBX])
# define REG_ECX (p->gregs[REG_ECX])
# define REG_EDX (p->gregs[REG_EDX])
# define REG_ESI (p->gregs[REG_ESI])
# define REG_EDI (p->gregs[REG_EDI])
# define REG_EBP (p->gregs[REG_EBP])
# define REG_ESP (p->gregs[REG_ESP])
# define REG_EIP (p->gregs[REG_EIP])

#elif defined(__APPLE__)
/****************************************************************************
 * macOS x86_64
 ****************************************************************************/
# define REG_RAX (p->__ss.__rax)
# define REG_RBX (p->__ss.__rbx)
# define REG_RCX (p->__ss.__rcx)
# define REG_RDX (p->__ss.__rdx)
# define REG_RSI (p->__ss.__rsi)
# define REG_RDI (p->__ss.__rdi)
# define REG_RBP (p->__ss.__rbp)
# define REG_RSP (p->__ss.__rsp)
# define REG_R8  (p->__ss.__r8)
# define REG_R9  (p->__ss.__r9)
# define REG_R10 (p->__ss.__r10)
# define REG_R11 (p->__ss.__r11)
# define REG_R12 (p->__ss.__r12)
# define REG_R13 (p->__ss.__r13)
# define REG_R14 (p->__ss.__r14)
# define REG_R15 (p->__ss.__r15)
# define REG_RIP (p->__ss.__rip)
/****************************************************************************
 * macOS arm64
 ****************************************************************************/
# define REG_X(n)   (p->__ss.__x[n])
# define REG_FP     (p->__ss.__fp)
# define REG_LR     (p->__ss.__lr)
# define REG_SP     (p->__ss.__sp)
# define REG_PC     (p->__ss.__pc)
# define REG_PSTATE (p->__ss.__cpsr)
# define FMT_PSTATE "%08x"
/****************************************************************************
 * macOS i386
 ****************************************************************************/
# define REG_EAX (p->__ss.__eax)
# define REG_EBX (p->__ss.__ebx)
# define REG_ECX (p->__ss.__ecx)
# define REG_EDX (p->__ss.__edx)
# define REG_ESI (p->__ss.__esi)
# define REG_EDI (p->__ss.__edi)
# define REG_EBP (p->__ss.__ebp)
# define REG_ESP (p->__ss.__esp)
# define REG_EIP (p->__ss.__eip)

#elif defined(__FreeBSD__)
/****************************************************************************
 * FreeBSD x86_64
 ****************************************************************************/
# define REG_RAX (p->mc_rax)
# define REG_RBX (p->mc_rbx)
# define REG_RCX (p->mc_rcx)
# define REG_RDX (p->mc_rdx)
# define REG_RSI (p->mc_rsi)
# define REG_RDI (p->mc_rdi)
# define REG_RBP (p->mc_rbp)
# define REG_RSP (p->mc_rsp)
# define REG_R8  (p->mc_r8)
# define REG_R9  (p->mc_r9)
# define REG_R10 (p->mc_r10)
# define REG_R11 (p->mc_r11)
# define REG_R12 (p->mc_r12)
# define REG_R13 (p->mc_r13)
# define REG_R14 (p->mc_r14)
# define REG_R15 (p->mc_r15)
# define REG_RIP (p->mc_rip)
/****************************************************************************
 * FreeBSD arm64
 ****************************************************************************/
# define REG_X(n)   (p->mc_gpregs.gp_x[n])
# define REG_FP     (p->mc_gpregs.gp_x[29])
# define REG_LR     (p->mc_gpregs.gp_lr)
# define REG_SP     (p->mc_gpregs.gp_sp)
# define REG_PC     (p->mc_gpregs.gp_elr)
# define REG_PSTATE (p->mc_gpregs.gp_spsr)
# define FMT_PSTATE "%08" PRIx32
/****************************************************************************
 * FreeBSD i386
 ****************************************************************************/
# define REG_EAX (p->mc_eax)
# define REG_EBX (p->mc_ebx)
# define REG_ECX (p->mc_ecx)
# define REG_EDX (p->mc_edx)
# define REG_ESI (p->mc_esi)
# define REG_EDI (p->mc_edi)
# define REG_EBP (p->mc_ebp)
# define REG_ESP (p->mc_esp)
# define REG_EIP (p->mc_eip)

#endif // #elif defined(__FreeBSD__)

#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64)

void
_stackdump_print_registers(_STRUCT_MCONTEXT *p)
{
  console_printf(
    "Registers: rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx "
    "rsi=%016llx rdi=%016llx rbp=%016llx rsp=%016llx "
    "r8=%016llx r9=%016llx r10=%016llx r11=%016llx "
    "r12=%016llx r13=%016llx r14=%016llx r15=%016llx rip=%016llx",
    (unsigned long long) REG_RAX, (unsigned long long) REG_RBX,
    (unsigned long long) REG_RCX, (unsigned long long) REG_RDX,
    (unsigned long long) REG_RSI, (unsigned long long) REG_RDI,
    (unsigned long long) REG_RBP, (unsigned long long) REG_RSP,
    (unsigned long long) REG_R8,  (unsigned long long) REG_R9,
    (unsigned long long) REG_R10, (unsigned long long) REG_R11,
    (unsigned long long) REG_R12, (unsigned long long) REG_R13,
    (unsigned long long) REG_R14, (unsigned long long) REG_R15,
    (unsigned long long) REG_RIP);

  _stackdump_print_stack((gpointer) REG_RSP);
}

#elif defined(__aarch64__) || defined(__arm64__) || defined(__arm64)

void
_stackdump_print_registers(_STRUCT_MCONTEXT *p)
{
  console_printf(
    "Registers: "
    "x0=%016llx x1=%016llx x2=%016llx x3=%016llx x4=%016llx x5=%016llx "
    "x6=%016llx x7=%016llx x8=%016llx x9=%016llx x10=%016llx x11=%016llx "
    "x12=%016llx x13=%016llx x14=%016llx x15=%016llx x16=%016llx x17=%016llx "
    "x18=%016llx x19=%016llx x20=%016llx x21=%016llx x22=%016llx x23=%016llx "
    "x24=%016llx x25=%016llx x26=%016llx x27=%016llx x28=%016llx fp=%016llx "
    "lr=%016llx sp=%016llx pc=%016llx pstate=" FMT_PSTATE,
    REG_X(0), REG_X(1), REG_X(2), REG_X(3), REG_X(4), REG_X(5),
    REG_X(6), REG_X(7), REG_X(8), REG_X(9), REG_X(10), REG_X(11),
    REG_X(12), REG_X(13), REG_X(14), REG_X(15), REG_X(16), REG_X(17),
    REG_X(18), REG_X(19), REG_X(20), REG_X(21), REG_X(22), REG_X(23),
    REG_X(24), REG_X(25), REG_X(26), REG_X(27), REG_X(28), REG_FP,
    REG_LR, REG_SP, REG_PC, REG_PSTATE);

  _stackdump_print_stack((gpointer) REG_SP);
}

#elif defined(__i386__) || defined(__i386) || defined(i386) || defined(__i686__)

void
_stackdump_print_registers(_STRUCT_MCONTEXT *p)
{
  console_printf(
    "Registers: eax=%08x ebx=%08x ecx=%08x edx=%08x "
    "esi=%08x edi=%08x ebp=%08x esp=%08x eip=%08x",
    (unsigned int) REG_EAX, (unsigned int) REG_EBX,
    (unsigned int) REG_ECX, (unsigned int) REG_EDX,
    (unsigned int) REG_ESI, (unsigned int) REG_EDI,
    (unsigned int) REG_EBP, (unsigned int) REG_ESP,
    (unsigned int) REG_EIP);

  _stackdump_print_stack((gpointer) REG_ESP);
}

#else
/****************************************************************************
 * unsupported platform
 ****************************************************************************/
#error "This is an unsupported architecture for stackdump currenty :("
#endif

static inline void
_stackdump_log(_STRUCT_MCONTEXT *p)
{
  /* the order is important here, even if it might be illogical.  The
   * backtrace function is the most fragile (as backtrace_symbols() may
   * allocate memory).  Let's log everything else first, and then we can
   * produce the backtrace, which is potentially causing another crash.  */

  _stackdump_print_registers(p);
  _stackdump_print_backtrace();
}

static void
_fatal_signal_handler(int signo, siginfo_t *info, void *uc)
{
  ucontext_t *ucontext = (ucontext_t *) uc;
  _STRUCT_MCONTEXT  *p = (_STRUCT_MCONTEXT *) &ucontext->uc_mcontext;
  struct sigaction act;

  memset(&act, 0, sizeof(act));
  act.sa_handler = SIG_DFL;
  sigaction(signo, &act, NULL);

  console_printf("Fatal signal received, stackdump follows, signal=%d", signo);
  _stackdump_log(p);

  /* let's get a stacktrace as well */
  kill(getpid(), signo);
}

void
stackdump_setup_signal(gint signal_number)
{
  struct sigaction act;

  memset(&act, 0, sizeof(act));
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = _fatal_signal_handler;

  sigaction(signal_number, &act, NULL);
}

#else // #if SYSLOG_NG_ENABLE_STACKDUMP

void
stackdump_setup_signal(gint signal_number)
{
}

#endif // #if SYSLOG_NG_ENABLE_STACKDUMP
