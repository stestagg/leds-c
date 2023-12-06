#include <stdio.h>
#include <string.h>

#include "esp.h"
#include "vm.h"
#include "led.h"

#define EXIT 0
#define SET 's'
#define PRINT 'p'
#define ADD 'a'
#define SUB 'u'
#define JUMP 'j'
#define JUMP_COND 'z'
#define CMP 'c'
#define CMPZ 'd'
#define CMPNZ 'e'
#define SLEEP 'x'

#define LED_CLEAR 0x09
#define LED_SET_N 0x0a
#define LED_SET_PIXEL 0x0b
#define LED_SYNC 0x0c
#define LED_BLUR 0x0d
#define LED_FILL 0x0e

#define N_REGISTER 4

uint16_t registers[N_REGISTER] = {0};
int cmp_flag = 0;
const uint16_t MAX_LITERAL = (0xffff - N_REGISTER);
#define LIT(x) ((x) % 0xff), ((x) >> 8)
#define REG(x) LIT(MAX_LITERAL + 1 + x)

char *ip = 0;

//#define DO_TRACE 1

#ifdef DO_TRACE

#define REGPRINT "\t[◰ %u, ◳ %u, ◱ %u, ◲ %u, ▶ %i]\n"
#define REGARGS registers[0], registers[1], registers[2], registers[3], (cur_ip - program)
#define TRACE(msg) printf("  ☕ " msg REGPRINT, REGARGS)
#define TRACE1(msg, a) printf("  ☕ " msg REGPRINT, a, REGARGS)
#define TRACE2(msg, a, b) printf("  ☕ " msg REGPRINT, a, b, REGARGS)
#define TRACE3(msg, a, b, c) printf("  ☕ " msg REGPRINT, a, b, c, REGARGS)
#define TRACE4(msg, a, b, c, d) printf("  ☕ " msg REGPRINT, a, b, c, d, REGARGS)
#define TRACE_SAVE_IP char* cur_ip = ip;

#else

#define TRACE(msg)
#define TRACE1(msg, a)
#define TRACE2(msg, a, b)
#define TRACE3(msg, a, b, c)
#define TRACE4(msg, a, b, c, d)
#define TRACE_SAVE_IP ;

#endif


static const char default_prog[] = {
    LED_SET_N, LIT(24),
    SET, 0, LIT(23),
    SET, 1, LIT(0),
    LED_CLEAR,
    //LED_FILL, LIT(0), LIT(0), LIT(0),
    LED_SET_PIXEL, REG(0), LIT(250), LIT(0), LIT(250),
    LED_SET_PIXEL, REG(1), LIT(0), LIT(250), LIT(250),
    //LED_BLUR, LIT(2),
    LED_SYNC,

    SUB, 0, LIT(1),
    ADD, 1, LIT(1),

    SLEEP, LIT(10),
    CMPNZ, REG(0),
    JUMP_COND, LIT(11),
    JUMP, LIT(3),
};

char program[PROG_MEM] = {0};


void load_default_prog() {
    memcpy(program, default_prog, sizeof(default_prog));
}

uint16_t read_u16() {
    uint16_t val = *(uint16_t*)ip;
    ip += 2;
    if (val > MAX_LITERAL) {
        uint16_t reg_num = (val - MAX_LITERAL) - 1;
        return registers[reg_num];
    }
    return val;
}

#define UNARY_REG_OP(msg) reg1 = *ip; ip += 1; val1 = read_u16(); TRACE2("%u "msg" %u", reg1, val1);

void run_prog() {

    if (ip == 0) {
        ip = program;
    }

    int reg1;
    int val1;
    int val2;
    int val3;
    int val4;

    while (1) {
        TRACE_SAVE_IP
        char instruction = *ip;
        ip += 1;
        switch (instruction) {
            case EXIT:
                TRACE1("EXIT: %i", ip - program);
                return;
            case PRINT:
                TRACE("PRINT");
                const char *msg = ip;
                ip += strlen(msg) + 1;
                printf(msg, registers[0], registers[1], registers[2], registers[3]);
                break;
            case ADD:
                UNARY_REG_OP("ADD");
                registers[reg1] += val1;
                break;
            case SET:
                UNARY_REG_OP("SET");
                registers[reg1] = val1;
                break;
            case SUB:
                UNARY_REG_OP("SUB");
                registers[reg1] -= val1;
                break;
            case CMP:
                char op = *(ip++);
                val1 = read_u16();
                val2 = read_u16();
                TRACE3("CMP: %u %c %u", val1, op, val2);
                switch (op) {
                    case '=': 
                        cmp_flag = val1 == val2; 
                        break;
                    case '>': 
                        cmp_flag = val1 > val2; 
                        break;
                    case '<': 
                        cmp_flag = val1 < val2; 
                        break;
                    case '!': 
                        cmp_flag = val1 != val2; 
                        break;
                }
                break;
            case CMPZ:
                val1 = read_u16();
                TRACE1("CMPZ: %u", val1);
                cmp_flag = val1 == 0;
                break;
            case CMPNZ:
                val1 = read_u16();
                TRACE1("CMPNZ: %u", val1);
                cmp_flag = val1 != 0;
                break;
            case SLEEP:
                val1 = read_u16();
                TRACE1("SLEEP: %u ms", val1);
                sleep(val1);
                break;
            case JUMP:
                val1 = read_u16();
                TRACE1("JUMP: %u", val1);
                ip = &program[val1];
                break;
            case JUMP_COND:
                val1 = read_u16();
                if (cmp_flag) {
                    TRACE1("JUMP_COND: %u (taken)", val1);
                    ip = program + val1;
                } else {
                    TRACE1("JUMP_COND: %u (skipped)", val1);
                }
                break;
            case LED_SET_N:
                val1 = read_u16() & 0xff;
                TRACE1("LED_SET_N %u", val1);
                set_n_pixels(val1);
                break;
            case LED_SET_PIXEL:
                val1 = read_u16() & 0xff; // Led num
                val2 = read_u16() & 0xff; // r
                val3 = read_u16() & 0xff; // g
                val4 = read_u16() & 0xff; // b
                TRACE4("LED_SET_PIXEL %u = %u, %u, %u", val1, val2, val3, val4);
                set_pixel(val1, val2, val3, val4);
                break;
            case LED_SYNC:
                TRACE("LED_SYNC");
                sync(true);
                break;
            case LED_CLEAR:
                TRACE("LED_CLEAR");
                led_clear();
                break;
            case LED_BLUR:
                val1 = read_u16();
                TRACE1("LED_BLUR: %u", val1);
                led_blur(val1);
                break;
            case LED_FILL:
                val1 = read_u16() & 0xff; // r
                val2 = read_u16() & 0xff; // g
                val3 = read_u16() & 0xff; // b
                TRACE3("LED_FILL %u, %u, %u", val1, val2, val3);
                for (int i=0; i < leds.n_pixels; ++i) {
                    set_pixel(i, val1, val2, val3);
                }
                break;
            default:
                TRACE1("UNKNOWN OP: %u", instruction);
        }
    }
}