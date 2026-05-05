#include "vga.h"
#include <stdint.h>

#define VGA_BUF ((volatile uint16_t *)0xB8000)

static int  g_row, g_col;
static uint8_t g_color;

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void update_hw_cursor(void) {
    uint16_t pos = g_row * VGA_WIDTH + g_col;
    /* CRT index port 0x3D4, data port 0x3D5 */
    __asm__ volatile(
        "outb %0, %1" : : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    __asm__ volatile(
        "outb %0, %1" : : "a"((uint8_t)(pos & 0xFF)), "Nd"((uint16_t)0x3D5));
    __asm__ volatile(
        "outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    __asm__ volatile(
        "outb %0, %1" : : "a"((uint8_t)(pos >> 8)), "Nd"((uint16_t)0x3D5));
}

void vga_init(void) {
    g_color = (VGA_BLACK << 4) | VGA_LIGHT_GREY;
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUF[i] = vga_entry(' ', g_color);
    g_row = g_col = 0;
    update_hw_cursor();
}

static void scroll(void) {
    for (int r = 1; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            VGA_BUF[(r - 1) * VGA_WIDTH + c] = VGA_BUF[r * VGA_WIDTH + c];
    for (int c = 0; c < VGA_WIDTH; c++)
        VGA_BUF[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = vga_entry(' ', g_color);
    g_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        g_col = 0;
        if (++g_row >= VGA_HEIGHT) scroll();
    } else if (c == '\r') {
        g_col = 0;
    } else if (c == '\t') {
        g_col = (g_col + 8) & ~7;
        if (g_col >= VGA_WIDTH) { g_col = 0; if (++g_row >= VGA_HEIGHT) scroll(); }
    } else if (c == '\b') {
        if (g_col > 0) {
            g_col--;
            VGA_BUF[g_row * VGA_WIDTH + g_col] = vga_entry(' ', g_color);
        }
    } else {
        VGA_BUF[g_row * VGA_WIDTH + g_col] = vga_entry(c, g_color);
        if (++g_col >= VGA_WIDTH) { g_col = 0; if (++g_row >= VGA_HEIGHT) scroll(); }
    }
    update_hw_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    g_color = (uint8_t)((bg << 4) | fg);
}

void vga_set_cursor(int row, int col) {
    g_row = row; g_col = col;
    update_hw_cursor();
}

int vga_get_row(void) { return g_row; }
int vga_get_col(void) { return g_col; }
