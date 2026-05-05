#include "keyboard.h"
#include "../arch/x86/irq.h"
#include "../arch/x86/idt.h"
#include <stdint.h>

/* US QWERTY scancode set 1 */
static const char sc_ascii[128] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const char sc_ascii_shift[128] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char    kb_buf[KB_BUFFER_SIZE];
static int     kb_head, kb_tail;
static int     shift_down;

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void kb_irq_handler(registers_t *regs) {
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { shift_down = 1; return; }
    if (sc == 0xAA || sc == 0xB6) { shift_down = 0; return; }
    if (sc & 0x80) return; /* key release */

    char c = shift_down ? sc_ascii_shift[sc] : sc_ascii[sc];
    if (!c) return;

    int next = (kb_tail + 1) % KB_BUFFER_SIZE;
    if (next != kb_head) { /* buffer not full */
        kb_buf[kb_tail] = c;
        kb_tail = next;
    }
}

void keyboard_init(void) {
    kb_head = kb_tail = 0;
    shift_down = 0;
    irq_register(1, kb_irq_handler);
}

int keyboard_available(void) { return kb_head != kb_tail; }

char keyboard_getchar(void) {
    while (!keyboard_available()) __asm__ volatile("sti; hlt");
    char c = kb_buf[kb_head];
    kb_head = (kb_head + 1) % KB_BUFFER_SIZE;
    return c;
}
