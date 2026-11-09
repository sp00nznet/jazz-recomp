/* jazz.h - the DOS machine under Jazz Jackrabbit's lifted code. */
#ifndef JAZZ_H
#define JAZZ_H
#include <stdint.h>
#include <string.h>
#include "cpu.h"
#include "recomp16/platform/font8x8.h"

/* Conventional memory, real-mode linear address -> host pointer. */
uint8_t *jazz_real(uint32_t lin);
uint64_t jazz_us(void);              /* microseconds since start */
void dos_int21(CPU *cpu);

void    vga_init(void);
void    vga_set_mode(int m);
int     vga_mode(void);
void    vga_write(uint32_t off, uint8_t v);
uint8_t vga_read(uint32_t off);
int     vga_port_in(uint16_t port, uint8_t *v);
int     vga_port_out(uint16_t port, uint8_t v);
void    vga_dac_set(int i, uint8_t r, uint8_t g, uint8_t b);
void    vga_dac_get(int i, uint8_t *r, uint8_t *g, uint8_t *b);
void    vga_compose(uint32_t *out, int *w, int *h);
void    vga_stats(void);
void    vga_dump(const char *path, unsigned row);

#endif
