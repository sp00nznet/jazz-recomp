/*
 * jazz.c - the DOS machine under Jazz Jackrabbit.
 *
 * FILE0001.EXE is a 16-bit protected-mode NE program for Borland's DOS
 * extender (RTM.EXE over DPMI16BI.OVL). pcrecomp's Win16 host loads it and
 * answers its KERNEL imports, since RTM's loader looks like Windows' to the
 * program. What a Windows program never does, a DOS game does all the time,
 * and that is this file:
 *
 *   - conventional memory: one 1 MB block, with the selectors KERNEL exports
 *     for real-mode memory (__A000H, __0040H, __B800H, ...) pointing into it
 *     and A000-AFFF routed to the VGA (vga.c) through the memory hook;
 *   - DPMI (INT 31h): descriptors, DOS memory, interrupt vectors;
 *   - DOS (INT 21h) beyond what the Win16 host has: files that can be
 *     written, vectors, the console;
 *   - RTM's own heap calls (RTM.5/6/21) and KERNEL's selector calls;
 *   - the PIT and the keyboard, delivered to the handlers the game installs
 *     with DPMI 0205h, from the lifted code's loop back-edges (RECOMP_IRQ);
 *   - a window, scripted keys and screenshots for test runs.
 *
 * Environment: JAZZ_HEADLESS=1 (no window), JAZZ_SECS=N (exit after N s),
 * JAZZ_SHOT="S:file.bmp,..." (the screen at S seconds), JAZZ_KEYS="S:key,..."
 * (press a key at S seconds: enter space esc up down left right ctrl alt
 * y n f1..f10 1..9 a..z), JAZZ_LOG=1 (every DPMI and DOS call).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "jazz.h"
#include "recomp_game.h"

#ifndef RECOMP_GAME_DIR
#define RECOMP_GAME_DIR "game"
#endif

void dispatch_far(CPU *cpu, uint16_t seg, uint16_t off);

static CPU *g;
static uint32_t g_real;                  /* flat base of conventional memory */
static uint32_t g_vga = 0xF0000000u;     /* flat base of A000:0000 */
static int g_log;

uint8_t *jazz_real(uint32_t lin) { return g->mem + g_real + lin; }

static LARGE_INTEGER g_qpf, g_qp0;
uint64_t jazz_us(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)((t.QuadPart - g_qp0.QuadPart) * 1000000 / g_qpf.QuadPart);
}

#define LOG(...) do { if (g_log) printf(__VA_ARGS__); } while (0)

/* ---- memory ------------------------------------------------------------------ */

int recomp_mem_write8(CPU *cpu, uint32_t a, uint8_t v)
{
    (void)cpu;
    if (a - g_vga >= 0x10000u) return 0;
    vga_write(a - g_vga, v);
    return 1;
}

int recomp_mem_read8(CPU *cpu, uint32_t a, uint8_t *out)
{
    (void)cpu;
    if (a - g_vga >= 0x10000u) return 0;
    *out = vga_read(a - g_vga);
    return 1;
}

static inline uint16_t arg16(CPU *cpu, int off) { return mem_read16(cpu, cpu->ss, (uint16_t)(cpu->sp + off)); }
static inline uint8_t *flat(CPU *cpu, uint16_t sel, uint16_t off) { return cpu->mem + cpu->sel_base[sel] + off; }

/* Copies between host memory and guest memory that may be video memory. */
static void to_guest(CPU *cpu, uint16_t sel, uint16_t off, const uint8_t *src, uint32_t n)
{
    uint32_t a = cpu->sel_base[sel] + off;
    if (a + n <= g_vga || a >= g_vga + 0x10000u) { memcpy(cpu->mem + a, src, n); return; }
    for (uint32_t i = 0; i < n; i++) mem_write8(cpu, sel, (uint16_t)(off + i), src[i]);
}

static void from_guest(CPU *cpu, uint16_t sel, uint16_t off, uint8_t *dst, uint32_t n)
{
    uint32_t a = cpu->sel_base[sel] + off;
    if (a + n <= g_vga || a >= g_vga + 0x10000u) { memcpy(dst, cpu->mem + a, n); return; }
    for (uint32_t i = 0; i < n; i++) dst[i] = mem_read8(cpu, sel, (uint16_t)(off + i));
}

static uint16_t new_sel(CPU *cpu, uint32_t base)
{
    uint16_t s = cpu->next_sel++;
    cpu->sel_base[s] = base;
    return s;
}

/* ---- KERNEL's selector calls (PASCAL: the callee pops) ------------------------ */

void KERNEL_ALLOCSELECTOR(CPU *cpu)
{
    uint16_t s = arg16(cpu, 4);
    cpu->ax = new_sel(cpu, s ? cpu->sel_base[s] : RECOMP_GUARD_BASE);
    cpu->sp += 4 + 2;
}

void KERNEL_FREESELECTOR(CPU *cpu)
{
    uint16_t s = arg16(cpu, 4);
    if (s >= 0x4000) cpu->sel_base[s] = RECOMP_GUARD_BASE;
    cpu->ax = 0;
    cpu->sp += 4 + 2;
}

/* PrestoChangoSelector(src, dst): dst now describes src's memory. */
void KERNEL_PRESTOCHANGOSELECTOR(CPU *cpu)
{
    uint16_t dst = arg16(cpu, 4), src = arg16(cpu, 6);
    cpu->sel_base[dst] = cpu->sel_base[src];
    cpu->ax = dst;
    cpu->sp += 4 + 4;
}

void KERNEL_ALLOCDSTOCSALIAS(CPU *cpu)
{
    uint16_t s = arg16(cpu, 4);
    cpu->ax = new_sel(cpu, cpu->sel_base[s]);
    cpu->sp += 4 + 2;
}

/* ---- RTM's heap (Borland Pascal's System unit calls these) ---------------------
 * RTM.5: allocate. 14 bytes of arguments: a far pointer that receives the new
 * selector, a word, a dword of attributes, the dword size. AX = 0 on success.
 * RTM.6: free a selector. RTM.21: the largest free block, in DX:AX. */
void RTM_Ord5(CPU *cpu)
{
    uint16_t poff = arg16(cpu, 4), pseg = arg16(cpu, 6);
    uint32_t size = arg16(cpu, 14) | ((uint32_t)arg16(cpu, 16) << 16);
    uint16_t s = cpu_alloc_selector(cpu, size ? size : 0x10000u);
    LOG("[rtm] alloc %u -> %04X\n", size, s);
    mem_write16(cpu, pseg, poff, s);
    cpu->ax = s ? 0 : 8;
    cpu->sp += 4 + 14;
}

void RTM_Ord6(CPU *cpu)
{
    LOG("[rtm] free %04X\n", arg16(cpu, 4));
    cpu->ax = 0;          /* ponytail: the bump heap never reuses; Jazz frees little */
    cpu->sp += 4 + 2;
}

void RTM_Ord21(CPU *cpu)
{
    uint32_t avail = cpu->heap_end - cpu->heap_next;
    if (avail > (16u << 20)) avail = 16u << 20;
    cpu->ax = (uint16_t)avail;
    cpu->dx = (uint16_t)(avail >> 16);
    cpu->sp += 4 + 4;
}

/* ---- interrupts ---------------------------------------------------------------- */

static uint32_t g_pvec[256];        /* protected-mode vectors the game set, sel<<16|off */
static uint32_t g_rvec[256];        /* real-mode vectors, as set (never called) */
static int g_in_irq;
int g_recomp_tick_budget = 1;

/* The vector a game reads before setting its own: an iret that keeps the
 * BIOS tick for the timer. The game calls it with pushf + call far. */
static void default_isr(CPU *cpu, uint16_t vec)
{
    if (vec == 8) { uint32_t *t = (uint32_t *)jazz_real(0x46C); (*t)++; }
    cpu->sp += 6;
}

static uint32_t pvec_get(int v)
{
    return g_pvec[v] ? g_pvec[v] : recomp_host_thunk(default_isr, (uint16_t)v);
}

/* A hardware interrupt into the game's handler. The lifted iret only
 * returns, without popping its frame, so the whole register file comes back
 * from here. */
static void fire(CPU *cpu, int vec)
{
    uint32_t fp = g_pvec[vec];
    if (!fp) { if (vec == 8) { uint32_t *t = (uint32_t *)jazz_real(0x46C); (*t)++; } return; }
    CPU save = *cpu;
    push16(cpu, cpu->flags);
    push16(cpu, 0xFFFF);
    push16(cpu, 0);
    cpu->flags &= ~(FLAG_IF | FLAG_TF);
    g_in_irq = 1;
    dispatch_far(cpu, (uint16_t)(fp >> 16), (uint16_t)fp);
    g_in_irq = 0;
    cpu->eax = save.eax; cpu->ebx = save.ebx; cpu->ecx = save.ecx; cpu->edx = save.edx;
    cpu->esi = save.esi; cpu->edi = save.edi; cpu->ebp = save.ebp; cpu->esp = save.esp;
    cpu->ds = save.ds; cpu->es = save.es; cpu->ss = save.ss; cpu->fs = save.fs; cpu->gs = save.gs;
    cpu->cs = save.cs; cpu->flags = save.flags;
    memcpy(cpu->st, save.st, sizeof save.st);
    cpu->fpu_top = save.fpu_top; cpu->fpu_status = save.fpu_status;
    cpu->fpu_control = save.fpu_control; cpu->fpu_tag = save.fpu_tag;
}

/* ---- the PIT, the keyboard controller, the PIC --------------------------------- */

static uint32_t g_pit_div = 65536;  /* channel 0 */
static int g_pit_lo, g_pit_hilo;
static uint64_t g_next_irq0;
static uint8_t g_port60, g_port61;

static CRITICAL_SECTION g_kq_cs;
static uint8_t g_kq[256];
static int g_kq_r, g_kq_w;
static void key_push(uint8_t sc)
{
    EnterCriticalSection(&g_kq_cs);
    if (((g_kq_w + 1) & 255) != g_kq_r) { g_kq[g_kq_w] = sc; g_kq_w = (g_kq_w + 1) & 255; }
    LeaveCriticalSection(&g_kq_cs);
}
static int key_pop(uint8_t *sc)
{
    int ok = 0;
    EnterCriticalSection(&g_kq_cs);
    if (g_kq_r != g_kq_w) { *sc = g_kq[g_kq_r]; g_kq_r = (g_kq_r + 1) & 255; ok = 1; }
    LeaveCriticalSection(&g_kq_cs);
    return ok;
}

uint8_t port_in8(CPU *cpu, uint16_t port)
{
    (void)cpu;
    uint8_t v;
    if (vga_port_in(port, &v)) return v;
    switch (port) {
    case 0x60: return g_port60;
    case 0x61: return g_port61;
    case 0x64: return 0x14;                 /* nothing pending, self-test passed */
    case 0x40: {                            /* channel 0's count, falling */
        uint32_t c = (uint32_t)(g_pit_div - (jazz_us() * 1193182 / 1000000) % g_pit_div);
        v = g_pit_lo ? (uint8_t)(c >> 8) : (uint8_t)c;
        g_pit_lo ^= 1;
        return v;
    }
    case 0x21: case 0xA1: return 0;
    }
    LOG("[port] in %03X\n", port);
    return 0xFF;
}

void port_out8(CPU *cpu, uint16_t port, uint8_t v)
{
    (void)cpu;
    if (vga_port_out(port, v)) return;
    switch (port) {
    case 0x20: case 0xA0: case 0x21: case 0xA1: return;   /* EOI, masks */
    case 0x43: g_pit_hilo = 0; g_pit_lo = 0; return;
    case 0x40:
        if (!g_pit_hilo) { g_pit_div = (g_pit_div & 0xFF00) | v; g_pit_hilo = 1; }
        else {
            g_pit_div = (g_pit_div & 0xFF) | ((uint32_t)v << 8);
            if (!g_pit_div) g_pit_div = 65536;
            g_pit_hilo = 0;
            printf("[pit] timer %.1f Hz\n", 1193182.0 / g_pit_div);
        }
        return;
    case 0x42: return;                     /* the speaker */
    case 0x61: g_port61 = v; return;
    }
    LOG("[port] out %03X <- %02X\n", port, v);
}

uint16_t port_in16(CPU *cpu, uint16_t port)
{
    return (uint16_t)(port_in8(cpu, port) | (port_in8(cpu, (uint16_t)(port + 1)) << 8));
}

void port_out16(CPU *cpu, uint16_t port, uint16_t v)
{
    port_out8(cpu, port, (uint8_t)v);
    port_out8(cpu, (uint16_t)(port + 1), (uint8_t)(v >> 8));
}

/* ---- test runs: shots, keys, an end ---------------------------------------- */

static uint32_t g_frame[640 * 400];

static void write_bmp(const char *path)
{
    int w, h;
    vga_compose(g_frame, &w, &h);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint32_t stride = (uint32_t)w * 3, size = stride * h;
    BITMAPINFOHEADER bh = { sizeof bh, w, h, 1, 24, BI_RGB };
    BITMAPFILEHEADER fh = { 0x4D42, sizeof fh + sizeof bh + size, 0, 0, sizeof fh + sizeof bh };
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&bh, sizeof bh, 1, f);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++) {
            uint32_t p = g_frame[y * w + x];
            uint8_t b[3] = { (uint8_t)p, (uint8_t)(p >> 8), (uint8_t)(p >> 16) };
            fwrite(b, 3, 1, f);
        }
    fclose(f);
    printf("[shot] %dx%d mode %02X -> %s\n", w, h, vga_mode(), path);
    vga_stats();
    if (getenv("JAZZ_VRAM")) vga_dump(getenv("JAZZ_VRAM"), 84);
}

typedef struct { double at; char what[MAX_PATH]; } due_t;
static due_t g_shots[16], g_keys[64];
static int g_nshots, g_nkeys, g_ishot, g_ikey;
static double g_secs;

static int parse_due(const char *env, due_t *d, int max)
{
    const char *s = getenv(env);
    int n = 0;
    while (s && *s && n < max) {
        char *end;
        d[n].at = strtod(s, &end);
        if (*end != ':') break;
        s = end + 1;
        int k = 0;
        while (*s && *s != ',' && k < MAX_PATH - 1) d[n].what[k++] = *s++;
        d[n].what[k] = 0;
        n++;
        if (*s == ',') s++;
    }
    return n;
}

static int key_code(const char *k)
{
    static const struct { const char *n; int sc; } t[] = {
        {"enter",0x1C},{"space",0x39},{"esc",0x01},{"up",0xE048},{"down",0xE050},
        {"left",0xE04B},{"right",0xE04D},{"ctrl",0x1D},{"alt",0x38},{"shift",0x2A},
        {"tab",0x0F},{"f1",0x3B},{"f2",0x3C},{"f3",0x3D},{"f4",0x3E},{"f5",0x3F},
        {"f6",0x40},{"f7",0x41},{"f8",0x42},{"f9",0x43},{"f10",0x44}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if (!_stricmp(k, t[i].n)) return t[i].sc;
    if (k[0] >= '1' && k[0] <= '9' && !k[1]) return 0x02 + (k[0] - '1');
    if (k[0] && !k[1]) {
        static const char row[] = "qwertyuiop\0\0\0\0asdfghjkl\0\0\0\0\0zxcvbnm";
        const char *p = memchr(row, k[0] | 0x20, sizeof row);
        if (p) return 0x10 + (int)(p - row);
    }
    return 0;
}

static void key_event(int sc, int up)
{
    if (sc > 0xFF) key_push(0xE0);
    key_push((uint8_t)((sc & 0xFF) | (up ? 0x80 : 0)));
}

static int g_held_key;
static double g_release_at;

static void due(void)
{
    double t = jazz_us() / 1e6;
    while (g_ishot < g_nshots && t >= g_shots[g_ishot].at) write_bmp(g_shots[g_ishot++].what);
    if (g_held_key && t >= g_release_at) { key_event(g_held_key, 1); g_held_key = 0; }
    while (!g_held_key && g_ikey < g_nkeys && t >= g_keys[g_ikey].at) {
        char *hold = strchr(g_keys[g_ikey].what, '*');   /* "right*3": held 3 s */
        double secs = hold ? (*hold = 0, atof(hold + 1)) : 0.15;
        int sc = key_code(g_keys[g_ikey].what);
        printf("[keys] %.1fs %s\n", t, g_keys[g_ikey].what);
        g_ikey++;
        if (sc) { key_event(sc, 0); g_held_key = sc; g_release_at = t + secs; }
    }
    if (g_secs && t >= g_secs) {
        void dump_fn_ring(int n);
        printf("[jazz] %.0f s: done\n", g_secs);
        fflush(stdout);
        dump_fn_ring(30);
        fflush(stderr);
        fflush(stdout);
        ExitProcess(0);
    }
}

/* From the lifted code's loop back-edges: deliver what is due. */
void recomp_tick(CPU *cpu)
{
    g_recomp_tick_budget = 500;
    if (g_in_irq) return;
    static uint64_t last_due;
    uint64_t now = jazz_us();
    if (now - last_due > 10000) { last_due = now; due(); }
    if (!(cpu->flags & FLAG_IF)) return;
    uint64_t period = (uint64_t)g_pit_div * 1000000 / 1193182;
    if (now >= g_next_irq0) {
        if (now - g_next_irq0 > period * 8) g_next_irq0 = now;   /* fell behind: don't storm */
        g_next_irq0 += period;
        fire(cpu, 8);
    }
    uint8_t sc;
    if (key_pop(&sc)) { g_port60 = sc; fire(cpu, 9); }
}

/* ---- the window --------------------------------------------------------------- */

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_TIMER: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_PAINT: {
        static uint32_t buf[640 * 400];
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        int fw, fh;
        vga_compose(buf, &fw, &fh);
        RECT r;
        GetClientRect(h, &r);
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), fw, -fh, 1, 32, BI_RGB } };
        StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, fw, fh, buf, &bi, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
        if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && (l & 0x40000000)) return 0;   /* repeat */
        int sc = (int)((l >> 16) & 0xFF) | ((l & 0x01000000) ? 0xE000 : 0);
        key_event(sc, m == WM_KEYUP || m == WM_SYSKEYUP);
        return 0;
    }
    case WM_SYSCHAR: return 0;
    case WM_DESTROY: ExitProcess(0);
    }
    return DefWindowProcA(h, m, w, l);
}

static DWORD WINAPI window_thread(LPVOID unused)
{
    (void)unused;
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "JazzRecomp";
    RegisterClassA(&wc);
    RECT r = { 0, 0, 640, 400 };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND h = CreateWindowA("JazzRecomp", "Jazz Jackrabbit", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, wc.hInstance, NULL);
    SetTimer(h, 1, 14, NULL);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    return 0;
}

/* ---- DPMI (INT 31h) -------------------------------------------------------------- */

static uint32_t g_dos_next = 0x10000;     /* conventional memory handed out, linear */

static void int10(CPU *cpu);

/* DPMI 0300h's register block at es:di, in and out. */
typedef struct {
    uint32_t edi, esi, ebp, res, ebx, edx, ecx, eax;
    uint16_t flags, es, ds, fs, gs, ip, cs, sp, ss;
} rmregs_t;

static void simulate_real_int(CPU *cpu, int vec)
{
    rmregs_t r;
    from_guest(cpu, cpu->es, cpu->di, (uint8_t *)&r, sizeof r);
    CPU t = *cpu;
    t.eax = r.eax; t.ebx = r.ebx; t.ecx = r.ecx; t.edx = r.edx; t.esi = r.esi; t.edi = r.edi;
    t.flags = r.flags;
    LOG("[dpmi] real int %02X ax=%04X bx=%04X\n", vec, t.ax, t.bx);
    switch (vec) {
    case 0x10: int10(&t); break;
    case 0x33: t.ax = 0; break;               /* no mouse */
    case 0x16: t.ax = 0; t.flags |= FLAG_ZF; break;
    default: printf("[dpmi] real-mode int %02X ax=%04X: not handled\n", vec, t.ax); break;
    }
    r.eax = t.eax; r.ebx = t.ebx; r.ecx = t.ecx; r.edx = t.edx; r.esi = t.esi; r.edi = t.edi;
    r.flags = t.flags;
    to_guest(cpu, cpu->es, cpu->di, (const uint8_t *)&r, sizeof r);
}

static void int31(CPU *cpu)
{
    uint16_t fn = cpu->ax;
    cpu->flags &= ~FLAG_CF;
    LOG("[dpmi] %04X bx=%04X cx=%04X dx=%04X\n", fn, cpu->bx, cpu->cx, cpu->dx);
    switch (fn) {
    case 0x0000: {                            /* allocate cx descriptors */
        uint16_t first = cpu->next_sel;
        for (int i = 0; i < (cpu->cx ? cpu->cx : 1); i++) new_sel(cpu, RECOMP_GUARD_BASE);
        cpu->ax = first;
        break;
    }
    case 0x0001: if (cpu->bx >= 0x4000) cpu->sel_base[cpu->bx] = RECOMP_GUARD_BASE; break;
    case 0x0002: cpu->ax = new_sel(cpu, g_real + ((uint32_t)cpu->bx << 4)); break;
    case 0x0003: cpu->ax = 1; break;          /* selectors are consecutive here */
    case 0x0006: {                            /* base, as a host-flat "linear" address */
        uint32_t b = cpu->sel_base[cpu->bx];
        cpu->cx = (uint16_t)(b >> 16); cpu->dx = (uint16_t)b;
        break;
    }
    case 0x0007: cpu->sel_base[cpu->bx] = ((uint32_t)cpu->cx << 16) | cpu->dx; break;
    case 0x0008: case 0x0009: break;          /* limits, rights: everything is 64K data */
    case 0x000A: cpu->ax = new_sel(cpu, cpu->sel_base[cpu->bx]); break;
    case 0x0100: {                            /* DOS memory: bx paragraphs */
        uint32_t n = (uint32_t)cpu->bx << 4;
        if (g_dos_next + n > 0x9F000) { cpu->ax = 8; cpu->bx = (uint16_t)((0x9F000 - g_dos_next) >> 4); cpu->flags |= FLAG_CF; break; }
        cpu->ax = (uint16_t)(g_dos_next >> 4);
        cpu->dx = new_sel(cpu, g_real + g_dos_next);
        for (uint32_t o = 0x10000; o < n; o += 0x10000) new_sel(cpu, g_real + g_dos_next + o);
        g_dos_next += (n + 15) & ~15u;
        break;
    }
    case 0x0101: case 0x0102: break;
    case 0x0200: cpu->cx = (uint16_t)(g_rvec[cpu->bl] >> 16); cpu->dx = (uint16_t)g_rvec[cpu->bl]; break;
    case 0x0201: g_rvec[cpu->bl] = ((uint32_t)cpu->cx << 16) | cpu->dx; break;
    case 0x0202: { uint32_t v = recomp_host_thunk(default_isr, 0x100 + cpu->bl); cpu->cx = (uint16_t)(v >> 16); cpu->dx = (uint16_t)v; break; }
    case 0x0203: break;                       /* exception handlers: nothing faults here */
    case 0x0204: { uint32_t v = pvec_get(cpu->bl); cpu->cx = (uint16_t)(v >> 16); cpu->dx = (uint16_t)v; break; }
    case 0x0205: {
        uint32_t v = ((uint32_t)cpu->cx << 16) | cpu->dx;
        printf("[dpmi] int %02X -> %04X:%04X\n", cpu->bl, cpu->cx, cpu->dx);
        g_pvec[cpu->bl] = (cpu->cx == 0xFFF0) ? 0 : v;   /* a default put back */
        break;
    }
    case 0x0300: simulate_real_int(cpu, cpu->bl); break;
    case 0x0303: {                            /* a real-mode callback: nothing real-mode runs here */
        static uint16_t next = 0x1000;
        printf("[dpmi] real-mode callback for %04X:%04X\n", cpu->ds, cpu->si);
        /* The CRT unit asks for its callback (seg009:38A5) straight after
         * timing its delay loop against one BIOS tick (seg009_37DC): loops
         * per tick / 55 = loops per ms. Native code is far too fast for that
         * 16-bit quotient -- Borland's "Runtime error 200" -- so the divide
         * overflowed and DelayCnt (DS:E9A2) is garbage. The most it can hold. */
        if (cpu->si == 0x38A5) mem_write16(cpu, cpu->es, 0xE9A2, 0xFFFF);
        cpu->cx = 0xF000; cpu->dx = next; next += 0x10;
        break;
    }
    case 0x0304: break;
    case 0x0400: cpu->ax = 0x005A; cpu->bx = 0x0005; cpu->cl = 4; cpu->dx = 0x0870; break;
    case 0x0500: {                            /* free memory information */
        uint32_t info[12];
        uint32_t avail = cpu->heap_end - cpu->heap_next;
        for (int i = 0; i < 12; i++) info[i] = 0xFFFFFFFFu;
        info[0] = avail; info[1] = avail >> 12; info[2] = avail >> 12;
        to_guest(cpu, cpu->es, cpu->di, (const uint8_t *)info, sizeof info);
        break;
    }
    case 0x0600: case 0x0601: case 0x0602: case 0x0603: break;
    case 0x0900: cpu->al = (cpu->flags & FLAG_IF) ? 1 : 0; cpu->flags &= ~FLAG_IF; break;
    case 0x0901: cpu->al = (cpu->flags & FLAG_IF) ? 1 : 0; cpu->flags |= FLAG_IF; break;
    case 0x0902: cpu->al = (cpu->flags & FLAG_IF) ? 1 : 0; break;
    default:
        printf("[dpmi] %04X: not handled\n", fn);
        cpu->flags |= FLAG_CF;
        break;
    }
}

/* ---- the BIOS ------------------------------------------------------------------ */

static void int10(CPU *cpu)
{
    switch (cpu->ah) {
    case 0x00:
        printf("[bios] mode %02X\n", cpu->al);
        vga_set_mode(cpu->al);
        if (getenv("JAZZ_RING_AT_MODE")) { void dump_fn_ring(int n); dump_fn_ring(160); }
        break;
    case 0x0F: cpu->al = (uint8_t)vga_mode(); cpu->ah = 80; cpu->bh = 0; break;
    case 0x10:
        if (cpu->al == 0x10) vga_dac_set(cpu->bx, cpu->dh, cpu->ch, cpu->cl);
        else if (cpu->al == 0x15) vga_dac_get(cpu->bx, &cpu->dh, &cpu->ch, &cpu->cl);
        else if (cpu->al == 0x12 || cpu->al == 0x17) {
            for (int i = 0; i < cpu->cx; i++) {
                uint8_t *p = flat(cpu, cpu->es, (uint16_t)(cpu->dx + i * 3));
                if (cpu->al == 0x12) vga_dac_set(cpu->bx + i, p[0], p[1], p[2]);
                else vga_dac_get(cpu->bx + i, &p[0], &p[1], &p[2]);
            }
        }
        break;
    case 0x12: if (cpu->bl == 0x10) { cpu->bh = 0; cpu->bl = 3; cpu->cx = 0; } break;
    case 0x1A: if (cpu->al == 0) { cpu->al = 0x1A; cpu->bx = 0x0008; } break;
    default: LOG("[bios] int 10 ah=%02X\n", cpu->ah); break;
    }
}

void int_handler(CPU *cpu, int vec)
{
    /* Borland Pascal's Dos.Intr(IntNo, Regs) (seg007_3BD7) patches the number
     * into its own `int 00` at cs:3C1A, through a data alias of its code
     * selector, which is on top of the stack here. The lifted code still says
     * `int 0`; the number is read back from where the patch went. */
    if (vec == 0) {
        uint16_t alias = arg16(cpu, 0);
        vec = *flat(cpu, alias, 0x3C1A);
        LOG("[intr] int %02X ax=%04X\n", vec, cpu->ax);
        if (vec == 0x21) { dos_int21(cpu); return; }
        if (vec == 0) { printf("[intr] int 0: no number patched\n"); return; }
    }
    switch (vec) {
    case 0x31: int31(cpu); return;
    case 0x10: int10(cpu); return;
    case 0x2F:
        LOG("[int2f] ax=%04X\n", cpu->ax);
        if (cpu->ax == 0x1600) cpu->al = 0;   /* not under Windows */
        return;
    case 0x16: cpu->ax = 0; cpu->flags |= FLAG_ZF; return;
    case 0x33: cpu->ax = 0; return;
    }
    static int told[256];
    if (!told[vec & 255]++) printf("[int] %02X ax=%04X: not handled\n", vec, cpu->ax);
}

/* ---- DOS: files that can be written, the console, vectors ------------------------ */

static int g_fd[64];                       /* DOS handle -> CRT fd + 1 */

static void dos_name(const char *in, char *out, size_t n)
{
    const char *leaf = in;
    for (const char *p = in; *p; p++) if (*p == '\\' || *p == '/' || *p == ':') leaf = p + 1;
    snprintf(out, n, "%s", leaf);
}

int recomp_game_int21(CPU *cpu)
{
    char name[MAX_PATH];
    switch (cpu->ah) {
    case 0x09: {                           /* print a $-terminated string */
        const char *s = (const char *)flat(cpu, cpu->ds, cpu->dx);
        const char *e = memchr(s, '$', 4096);
        printf("[dos] %.*s\n", e ? (int)(e - s) : 0, s);
        return 1;
    }
    case 0x19: cpu->al = 2; return 1;      /* C: */
    case 0x0E: cpu->al = 26; return 1;
    case 0x47: *flat(cpu, cpu->ds, cpu->si) = 0; cpu->flags &= ~FLAG_CF; return 1;
    /* From protected mode, DOS's set/get vector are the protected-mode
     * vectors (the extender translates them): Jazz installs its timer and
     * keyboard handlers this way, not with DPMI 0205h. */
    case 0x25:
        printf("[dos] int %02X -> %04X:%04X\n", cpu->al, cpu->ds, cpu->dx);
        g_pvec[cpu->al] = cpu->ds == 0xFFF0 ? 0 : ((uint32_t)cpu->ds << 16) | cpu->dx;
        return 1;
    case 0x35: {
        uint32_t v = pvec_get(cpu->al);
        cpu->es = (uint16_t)(v >> 16); cpu->bx = (uint16_t)v;
        return 1;
    }
    case 0x54: cpu->al = 0; return 1;
    case 0x3C: case 0x3D: {
        dos_name((const char *)flat(cpu, cpu->ds, cpu->dx), name, sizeof name);
        int fl = cpu->ah == 0x3C ? (_O_CREAT | _O_TRUNC | _O_RDWR)
               : (cpu->al & 3) == 0 ? _O_RDONLY : (cpu->al & 3) == 1 ? _O_WRONLY : _O_RDWR;
        int fd = _open(name, fl | _O_BINARY, _S_IREAD | _S_IWRITE);
        int h = 5;
        while (h < 64 && g_fd[h]) h++;
        printf("[dos] %s %s -> %s\n", cpu->ah == 0x3C ? "create" : "open", name, fd < 0 ? "missing" : "ok");
        if (fd < 0 || h == 64) { if (fd >= 0) _close(fd); cpu->ax = 2; cpu->flags |= FLAG_CF; return 1; }
        g_fd[h] = fd + 1;
        cpu->ax = (uint16_t)h;
        cpu->flags &= ~FLAG_CF;
        return 1;
    }
    case 0x3E:
        if (cpu->bx < 64 && g_fd[cpu->bx]) { _close(g_fd[cpu->bx] - 1); g_fd[cpu->bx] = 0; }
        cpu->flags &= ~FLAG_CF;
        return 1;
    case 0x3F: {
        if (cpu->bx >= 64 || !g_fd[cpu->bx]) { cpu->ax = 0; cpu->flags &= ~FLAG_CF; return 1; }
        static uint8_t buf[0x10000];
        int n = _read(g_fd[cpu->bx] - 1, buf, cpu->cx);
        if (n < 0) n = 0;
        LOG("[dos] read %u of %u from %u into %04X:%04X\n", n, cpu->cx, cpu->bx, cpu->ds, cpu->dx);
        to_guest(cpu, cpu->ds, cpu->dx, buf, (uint32_t)n);
        cpu->ax = (uint16_t)n;
        cpu->flags &= ~FLAG_CF;
        return 1;
    }
    case 0x40: {
        static uint8_t buf[0x10000];
        from_guest(cpu, cpu->ds, cpu->dx, buf, cpu->cx);
        if (cpu->bx == 1 || cpu->bx == 2) { fwrite(buf, 1, cpu->cx, stdout); cpu->ax = cpu->cx; }
        else if (cpu->bx < 64 && g_fd[cpu->bx]) {
            int n = _write(g_fd[cpu->bx] - 1, buf, cpu->cx);
            cpu->ax = (uint16_t)(n < 0 ? 0 : n);
        } else cpu->ax = cpu->cx;
        cpu->flags &= ~FLAG_CF;
        return 1;
    }
    case 0x42: {
        if (cpu->bx >= 64 || !g_fd[cpu->bx]) { cpu->ax = 6; cpu->flags |= FLAG_CF; return 1; }
        long off = (long)(((uint32_t)cpu->cx << 16) | cpu->dx);
        long r = _lseek(g_fd[cpu->bx] - 1, off, cpu->al == 0 ? SEEK_SET : cpu->al == 1 ? SEEK_CUR : SEEK_END);
        if (r < 0) { cpu->ax = 25; cpu->flags |= FLAG_CF; return 1; }
        cpu->ax = (uint16_t)r; cpu->dx = (uint16_t)(r >> 16);
        cpu->flags &= ~FLAG_CF;
        return 1;
    }
    case 0x44:
        if (cpu->al == 0) { cpu->dx = cpu->bx <= 2 ? 0x80D3 : 0x0002; cpu->flags &= ~FLAG_CF; return 1; }
        cpu->flags &= ~FLAG_CF;
        return 1;
    case 0x41:
        dos_name((const char *)flat(cpu, cpu->ds, cpu->dx), name, sizeof name);
        if (remove(name)) { cpu->ax = 2; cpu->flags |= FLAG_CF; } else cpu->flags &= ~FLAG_CF;
        return 1;
    case 0x43: {
        dos_name((const char *)flat(cpu, cpu->ds, cpu->dx), name, sizeof name);
        DWORD a = GetFileAttributesA(name);
        if (a == INVALID_FILE_ATTRIBUTES) { cpu->ax = 2; cpu->flags |= FLAG_CF; }
        else { cpu->cx = (a & FILE_ATTRIBUTE_DIRECTORY) ? 0x10 : 0x20; cpu->flags &= ~FLAG_CF; }
        return 1;
    }
    case 0x4C:
        printf("[dos] exit %u\n", cpu->al);
        fflush(stdout);
        ExitProcess(cpu->al);
    }
    LOG("[dos] ah=%02X: to the Win16 host\n", cpu->ah);
    return 0;
}

/* ---- start -------------------------------------------------------------------- */

static void jazz_start(CPU *cpu)
{
    g = cpu;
    cpu->flags |= FLAG_IF;             /* DOS hands a program interrupts on */
    g_log = getenv("JAZZ_LOG") != NULL;
    QueryPerformanceFrequency(&g_qpf);
    QueryPerformanceCounter(&g_qp0);
    InitializeCriticalSection(&g_kq_cs);

    /* 1 MB of conventional memory plus the HMA, in one block. */
    uint16_t s = cpu_alloc_selector(cpu, 0x110000);
    g_real = cpu->sel_base[s];
    static const uint16_t segs[] = { 0xA000, 0xB000, 0xB800, 0x0040, 0xC000, 0xD000, 0xE000, 0xF000 };
    for (unsigned i = 0; i < sizeof segs / sizeof segs[0]; i++)
        cpu->sel_base[segs[i]] = g_real + ((uint32_t)segs[i] << 4);
    cpu->sel_base[0x0040] = g_real + 0x400;
    g_vga = g_real + 0xA0000;
    /* The BIOS data area a VGA BIOS leaves: the CRT unit takes its CRTC port
     * from 0040:0063 (and polls port+6 for its snow check) and its screen size
     * from 004A/0084. */
    uint8_t *bda = jazz_real(0x400);
    bda[0x10] = 0x21; bda[0x11] = 0x00;      /* equipment: 80x25 colour, FPU-less */
    bda[0x13] = 0x80; bda[0x14] = 0x02;      /* 640 KB */
    bda[0x4A] = 80;                          /* columns */
    bda[0x4C] = 0x00; bda[0x4D] = 0x10;      /* page size */
    bda[0x63] = 0xD4; bda[0x64] = 0x03;      /* CRTC at 3D4 */
    bda[0x84] = 24;                          /* rows - 1 */
    bda[0x85] = 16;                          /* character height */
    bda[0x87] = 0x60; bda[0x88] = 0x09;      /* EGA/VGA info */
    vga_init();

    if (_chdir(RECOMP_GAME_DIR)) printf("[jazz] cannot enter %s\n", RECOMP_GAME_DIR);
    g_nshots = parse_due("JAZZ_SHOT", g_shots, 16);
    g_nkeys = parse_due("JAZZ_KEYS", g_keys, 64);
    if (getenv("JAZZ_SECS")) g_secs = atof(getenv("JAZZ_SECS"));
    if (!getenv("JAZZ_HEADLESS")) CloseHandle(CreateThread(NULL, 0, window_thread, NULL, 0, NULL));
    printf("[jazz] conventional memory at %08X, game in %s\n", g_real, RECOMP_GAME_DIR);
}

const RecompGame recomp_game = {
    .title     = "Jazz Jackrabbit",
    .guest_dir = "C:\\JAZZ",
    .start     = jazz_start,
};

/* ---- code the game builds at run time --------------------------------------
 * Sprites and masked tiles are compiled: each frame is x86 the game generates
 * into a data block and calls far, with DS:SI at the screen spot -- a run of
 * `mov [si+d],imm` per plane, `add si,imm` per row, plane switches through the
 * sequencer, then `retf`. Nothing was lifted from there, so the far call lands
 * here and the few instructions such code uses are run directly.
 * ponytail: a subset interpreter; an unknown opcode is reported and the call
 * returns, so a new one shows up as one log line, not a crash. */
static uint16_t *x86_r16(CPU *cpu, int r)
{
    switch (r & 7) {
    case 0: return &cpu->ax; case 1: return &cpu->cx; case 2: return &cpu->dx; case 3: return &cpu->bx;
    case 4: return &cpu->sp; case 5: return &cpu->bp; case 6: return &cpu->si; default: return &cpu->di;
    }
}
static uint8_t *x86_r8(CPU *cpu, int r)
{
    uint16_t *w = x86_r16(cpu, r & 3);
    return (uint8_t *)w + (r >> 2);
}

int recomp_dispatch_miss_hook(CPU *cpu, uint16_t seg, uint16_t off, int far_call)
{
    uint16_t ip = off;
    int cf = 0;
    uint8_t first = mem_read8(cpu, seg, off);
    if (first != 0xC6 && first != 0xC7) return 0;       /* not a compiled sprite: a driver */
#define FB() mem_read8(cpu, seg, ip++)
#define FW() (ip += 2, mem_read16(cpu, seg, (uint16_t)(ip - 2)))
    for (long n = 0; n < 1000000; n++) {
        uint8_t op = FB();
        uint16_t sego = 0; int haveseg = 0;
        if (op == 0x26 || op == 0x2E || op == 0x36 || op == 0x3E) {
            sego = op == 0x26 ? cpu->es : op == 0x2E ? seg : op == 0x36 ? cpu->ss : cpu->ds;
            haveseg = 1; op = FB();
        }
        if (op == 0xC6 || op == 0xC7 || op == 0x88 || op == 0x89 || op == 0x8A || op == 0x8B
            || ((op == 0x81 || op == 0x83) )) {
            uint8_t m = FB();
            int mod = m >> 6, reg = (m >> 3) & 7, rm = m & 7;
            if (mod == 3) {                                  /* register operand */
                if (op == 0x81 || op == 0x83) {
                    uint16_t imm = op == 0x81 ? FW() : (uint16_t)(int8_t)FB();
                    uint16_t *d = x86_r16(cpu, rm);
                    uint32_t r;
                    switch (reg) {
                    case 0: r = (uint32_t)*d + imm; break;
                    case 2: r = (uint32_t)*d + imm + cf; break;
                    case 3: r = (uint32_t)*d - imm - cf; break;
                    case 5: r = (uint32_t)*d - imm; break;
                    default: goto bad;
                    }
                    cf = (r >> 16) & 1; *d = (uint16_t)r;
                    continue;
                }
                if (op == 0x8B) { *x86_r16(cpu, reg) = *x86_r16(cpu, rm); continue; }
                if (op == 0x89) { *x86_r16(cpu, rm) = *x86_r16(cpu, reg); continue; }
                if (op == 0x8A) { *x86_r8(cpu, reg) = *x86_r8(cpu, rm); continue; }
                if (op == 0x88) { *x86_r8(cpu, rm) = *x86_r8(cpu, reg); continue; }
                goto bad;
            }
            uint16_t ea, ds = cpu->ds;
            switch (rm) {
            case 0: ea = cpu->bx + cpu->si; break; case 1: ea = cpu->bx + cpu->di; break;
            case 2: ea = cpu->bp + cpu->si; ds = cpu->ss; break; case 3: ea = cpu->bp + cpu->di; ds = cpu->ss; break;
            case 4: ea = cpu->si; break; case 5: ea = cpu->di; break;
            case 6: if (mod == 0) { ea = FW(); } else { ea = cpu->bp; ds = cpu->ss; } break;
            default: ea = cpu->bx; break;
            }
            if (mod == 1) ea += (uint16_t)(int8_t)FB(); else if (mod == 2) ea += FW();
            if (haveseg) ds = sego;
            switch (op) {
            case 0xC6: mem_write8(cpu, ds, ea, FB()); break;
            case 0xC7: mem_write16(cpu, ds, ea, FW()); break;
            case 0x88: mem_write8(cpu, ds, ea, *x86_r8(cpu, reg)); break;
            case 0x89: mem_write16(cpu, ds, ea, *x86_r16(cpu, reg)); break;
            case 0x8A: *x86_r8(cpu, reg) = mem_read8(cpu, ds, ea); break;
            case 0x8B: *x86_r16(cpu, reg) = mem_read16(cpu, ds, ea); break;
            default: goto bad;
            }
            continue;
        }
        if (op >= 0xB0 && op <= 0xB7) { *x86_r8(cpu, op & 7) = FB(); continue; }
        if (op >= 0xB8 && op <= 0xBF) { *x86_r16(cpu, op & 7) = FW(); continue; }
        if (op >= 0x40 && op <= 0x47) { (*x86_r16(cpu, op & 7))++; continue; }
        if (op >= 0x48 && op <= 0x4F) { (*x86_r16(cpu, op & 7))--; continue; }
        if (op == 0x05) { cpu->ax += FW(); continue; }
        if (op == 0xEE) { port_out8(cpu, cpu->dx, cpu->al); continue; }
        if (op == 0xEF) { port_out16(cpu, cpu->dx, cpu->ax); continue; }
        if (op == 0xD0 || op == 0xD2) {                      /* rol/shl r8, 1 / cl */
            uint8_t m = FB(); uint8_t *d = x86_r8(cpu, m & 7); int c = op == 0xD0 ? 1 : (cpu->cl & 7);
            if ((m >> 6) != 3) goto bad;
            switch ((m >> 3) & 7) {
            case 0: if (c) { *d = (uint8_t)((*d << c) | (*d >> (8 - c))); cf = *d & 1; } break;
            case 1: if (c) { *d = (uint8_t)((*d >> c) | (*d << (8 - c))); cf = *d >> 7; } break;
            case 4: *d = (uint8_t)(*d << c); break;
            case 5: *d = (uint8_t)(*d >> c); break;
            default: goto bad;
            }
            continue;
        }
        if (op == 0x90) continue;
        if (op == 0xCB || op == 0xC3) {
            cpu->sp = (uint16_t)(cpu->sp + (far_call ? 4 : 2));
            return 1;
        }
    bad:
        {
            static int told;
            if (told++ < 20) {
                printf("[x86] %u:%04X (called at %04X): unknown", seg, (uint16_t)(ip - 1), off);
                for (int i = -2; i < 8; i++) printf(" %02X", mem_read8(cpu, seg, (uint16_t)(ip - 1 + i)));
                printf("\n");
            }
        }
        cpu->sp = (uint16_t)(cpu->sp + (far_call ? 4 : 2));
        return 1;
    }
#undef FB
#undef FW
    cpu->sp = (uint16_t)(cpu->sp + (far_call ? 4 : 2));
    return 1;
}
