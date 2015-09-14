#include <asf.h>

    /**********   Start Macro switches   **********/
#define RUN_CHECK
//#define RUN_SOFT_CHECK
    /**********   End Macro switches    **********/

    /**********   Start Macro defines   **********/

#define MAX_DIGITS    4u // Maximum digits supported
#define MAX_MAGNITUDE 4u // Maximum input size supported
#define MAX_PRECISION (MAX_DIGITS - MAX_MAGNITUDE)

    // These are codes for special key combinations.
#define TERMINATION_KEY     0x0
#define TERMINATION_KEY2    0x1
#define CONTINUE_KEY        0x2

    // These defines represent operation codes
#define ADD_GLYPH           '+'
#define SUB_GLYPH           '-'
#define MUL_GLYPH           '*'
#define DIV_GLYPH           '/'

    // Short macro functions for inlining common expressions
#define EMBED32(A1, A2, B1, B2) ( A1 | (A2<<4) | (B1<<8) | (B2<<12))
#define IS_NULL(P) (P == NULL)
    /**********   End Macro defines     **********/

    /**********   Start type aliasing   **********/
#include <stdint.h>

#define INT32   int32_t
#define UINT32  uint32_t
#define UINT8   uint8_t

#define state_type      UINT8
#define ent_num_state   0u
#define ent_op_state    1u
#define ent_fin_state   2u

#define input_type      UINT8
#define dig_input       0u
#define op_input        1u
#define ent_input       2u
#define no_input        3u
#define del_input       4u

#define BOOLEAN__     UINT8
#define TRUE__        1
#define FALSE__       0

typedef struct {
        // Store both operands as four digit numbers in an array. The operands
        //  will follow big endian format, i.e. the most significant digits
        //  will sit at the front of the array. Store individual digits to ease
        //  displaying.
    UINT8       operand[MAX_DIGITS*0x2];
    UINT8       index;
    UINT8       op_code;
        // bit0 ~ operand 1 | bit 1 ~ operand 2
    UINT8       is_neg;
} expression_data;

struct calculator_information_packet{
    UINT8 key_code;

        // Let a negative number indicate that the storage is empty
    expression_data exp;
    UINT8 magnitude, num_to_display;
    state_type state;
};

    /**********   End type aliasing     **********/

    /**********   Start function prototypes   **********/
    // Find last significant ON bit
UINT32 find_lsob(UINT32);

    // Debugging programs
#ifdef RUN_CHECK
    void test_hardware(void);
#endif
#ifdef RUN_SOFT_CHECK
    void test_software(void);
#endif

    // Main program
void run_calculator(void);

    // Configure pins for IO as well as enable
    //  other settings such as pull-up/down resistors.
void configure_ports(void);

    // Compute an expression based on its data and operation code.
    // Because an entire number cannot simply be display in one call,
    //  use comptue to store the resulting value as the first operand
    //  in the information packet. This might also allow extension into
    //  chained expressions.
    // Return whether or not an overload has occurred.    
BOOLEAN__ compute(void);

    // Store digit attempts to push digit to lsd of current number
    //  being used based on available space and the current state
    //  of the calculator.
    //  This function returns whether or not the storing of the
    //  digit was successful.
BOOLEAN__ store_dig(UINT8 new_dig);
BOOLEAN__ store_op(UINT8 new_op);

    // Determine type of input and its associated code number.
    //    For digit input, the code is the digit itself.
    //    For operation input:
    //        - Addition          --> ADD_GLYPH
    //        - Subtraction       --> SUB_GLYPH
    //        - Multiplication    --> MUL_GLYPH
    //        - Division          --> DIV_GLYPH
    //    For Enter and Clear inputs, the code is 0.
input_type decode_input_type(UINT8* i_code, UINT8 row, UINT8 col);

        /**********   Start IO functions   **********/
    // Display number to the seven segment displays
void display_dig(UINT32 add_delay, UINT8 dig_to_display, UINT8 select);

    // Wait for keypad input and return the key that was pressed.
    //  0 - 15 denotes key on keypad from bottom to top then left to right
    //  TERMINATION_KEY2 denotes combo key to terminate program
    //  TERMINATION_KEY denotes combo key to terminate test program
    //    13 denotes the clear key
    //    5 denotes the enter key
UINT8 wait_for_key(UINT32 delay_ms, UINT8* row_dest, UINT8* col_dest);
        /**********    End IO functions    **********/

    // Initial state is defined as all seven segment displays turned off.
    //  However, power is still present. All data in the global information
    //  packet is reset to predefined values.
void set_initial_state(void);
    // Ensure the structure values are set to default values.
void reset_info_pack(void);
    // Shutdown state is defined similarly to set_initial_state with the
    //  exception that power is switched off.
void shutdown(void);
    /**********   End function prototypes   **********/

    /**********   Start global variables   **********/
        // Since the majority of functions in this file depend
        //  on modifying common variables, go ahead and make them
        //  global.
static struct calculator_information_packet ci_pack;

        // These variables are shared by multiple IO functions.
static Port* port;
static PortGroup *bankA, *bankB;
    /**********    End global variables    **********/

    /**********   Start function definitions   **********/
int main(void){
    configure_ports();

    set_initial_state();

#ifdef RUN_SOFT_CHECK
    test_software();
#endif

#ifdef RUN_CHECK
    test_hardware();
#endif

//    run_calculator();

    shutdown();

    return 0;
}

void configure_ports(void){
	delay_init();

    port = (Port*)(0x41004400UL);
    bankA = &(port->Group[0]);
    bankB = &(port->Group[1]);
        // Controls power to the keypad and SSDs. 0000 1111 0000
    bankA->DIR.reg |= 0x000000F0;

    bankB->DIR.reg
            // Controls which segment turn on. 0000 1111 1111
        |= 0x000000FF
            // Controls sign indicator. 0010 0000 0000
        |  0x00000200
        ;
    // Set high drive strength ?
    unsigned short i = 0;
    for(; i < 8; ++i){
        bankA->PINCFG[i].reg |= (1<<6);
    }

        // For reading input from keypad. 1111 0000 0000 0000 0000 
        //  Active high logic for input.
    bankA->DIR.reg &= ~0x000F0000;
    for(i = 16; i < 20; ++i){
            // Enable input (bit 1).
            //  Note that the pins are externally pulled low,
            //  so disable pull up.
        bankA->PINCFG[i].reg = PORT_PINCFG_INEN;
    }
        // Turn off extra dots
    bankB->OUT.reg &= ~0x10;
}

BOOLEAN__ compute(){
        // Retrieve the actual operands
    INT32 op1=0x0, op2=0x0;
    UINT8 counter = MAX_DIGITS;
    INT32 factor = 1;
    for(; counter > 0x0; --counter, factor *= 10){
        op1 += ci_pack.exp.operand[counter-0x1] * factor;
        op2 += ci_pack.exp.operand[counter+MAX_DIGITS-0x1] * factor;
    }
    op1 *= (INT32)(ci_pack.exp.is_neg & 0x1)*(-2) + 1;
    op2 *= (INT32)(ci_pack.exp.is_neg & 0x2)*(-2) + 1;

    switch(ci_pack.exp.op_code){
        case ADD_GLYPH: op1 += op2; break;
        case SUB_GLYPH: op1 -= op2; break;
        case MUL_GLYPH: op1 *= op2; break;
        case DIV_GLYPH: op1 /= op2; break;
        default:        return false;
    }

    if(op1 < 0){
        ci_pack.exp.is_neg |= 0x1;
        op2 = (op1 *= -1);
    } else {
        op2 = op1;
    }
        // Reset some values and store the result in operand 1's slot
    for(counter = MAX_DIGITS; counter > 0x0; --counter, op1 /= 10){
        ci_pack.exp.operand[counter-0x1] = op1 % 0xA;
        ci_pack.exp.operand[counter+MAX_DIGITS-0x1] = 0x0;
    }
    ci_pack.exp.index = ci_pack.exp.op_code = 0u;
    ci_pack.exp.is_neg &= ~(0x2);

    return op2 >= factor*0xA;
}

BOOLEAN__ store_dig(UINT8 new_dig){
    switch(ci_pack.state){
        case ent_fin_state:
            // Calculation was finished, so reset all values
            //  and store new digit. Also go to the entering
            //  number state.
            reset_info_pack();
        case ent_num_state:
            // Program is ready to accept a new digit.
            if (ci_pack.magnitude == MAX_MAGNITUDE){
                // There were already MAX_MAGNITUDE digits stored
                return FALSE__;
            }
            if (!(ci_pack.magnitude || new_dig)){
                    // 0 condition
                return TRUE__;
            }
            ci_pack.exp.operand[ci_pack.exp.index] = new_dig;
                // Update magnitude and index.
            ++ci_pack.magnitude;
            ++ci_pack.exp.index;
            if (ci_pack.exp.index > MAX_DIGITS)
                ci_pack.num_to_display = 0x1;
            if(ci_pack.magnitude == MAX_MAGNITUDE){
                ci_pack.magnitude = 0u;
                ci_pack.state = ent_op_state;
            }
            return TRUE__;
        case ent_op_state:
            // Program was expecting an operator.
            //  Guarantee that expression is not affected.
            return FALSE__;
        default:    // Should never happen
            return FALSE__;
    }
}

BOOLEAN__ store_op(UINT8 new_op){
    switch (ci_pack.state){
        case ent_op_state:
            ci_pack.exp.op_code = new_op;
            ci_pack.state = ent_num_state;
            return TRUE__;
        case ent_num_state:
            if (ci_pack.exp.index < MAX_DIGITS || !ci_pack.magnitude){
                // Either the current operand has no digits
                //  or the program is already on the second
                //  operand.
                return FALSE__;
            }
            ci_pack.exp.op_code = new_op;
            ci_pack.exp.index = MAX_DIGITS;
            ci_pack.magnitude = 0u;
            return TRUE__;
        default:
            return FALSE__;
    }
}

input_type decode_input_type(UINT8* i_code, UINT8 row, UINT8 col){
    if (IS_NULL(i_code)){
        return no_input;
    }

    // Active high operation

    switch (row*4u+col){
    // Operations
        case 0x0:
            *i_code = DIV_GLYPH;
            return op_input;
        case 0x4:
            *i_code = MUL_GLYPH;
            return op_input;
        case 0x3:
            *i_code = SUB_GLYPH;
            return op_input;
        case 0x1:
            *i_code = ADD_GLYPH;
            return op_input;
    // Enter and clear
        case 0xC:
            *i_code = 0;
            return ent_input;
        case 0x8:
            *i_code = 0;
            return del_input;
    // Digits
        case 0x2:
            *i_code = 0;
            return dig_input;
        default:
           *i_code = (0x3 - row) * 0x3 + (0x4 - col);
            return dig_input;
    }
}

UINT32 find_lsob(UINT32 target){
    UINT32 toreturn = 0u;
    while(!(target & 0x1)){
        ++toreturn;
        target >>= 1;
    }
    return toreturn;
}

#ifdef RUN_CHECK
void test_hardware(void){
    // Run visual check on seven segment displays
    // Turn on all LEDs
    bankA->OUT.reg &= ~0xF0;
    bankB->OUT.reg &= ~0xFF;
    delay_ms(1000);

    // Turn off one segment at a time
    UINT32 lit_bit = 1;
#define TEST_DELY__ 50
    for (; lit_bit < (1u << 8); lit_bit <<= 1){
        bankB->OUT.reg |= lit_bit;
        delay_ms(TEST_DELY__);
    }

    // Turn on one number at a time
    lit_bit = 0u;
    for (; lit_bit < 16u; ++lit_bit){
        display_dig(5, lit_bit, 1);
        delay_ms(TEST_DELY__ * 3);
    }

    // Run semi-infinite loop to test keypad input
    UINT8 row = 0x0, col = 0x0;
    while((wait_for_key(1, &row, &col)) != TERMINATION_KEY2){
        display_dig(333, row*4u+col, row);
    }
}
#endif

#ifdef RUN_SOFT_CHECK
void test_software(void){
        // compute
    set_initial_state();
    UINT8 ounter = 0x0;
    for(; ounter < MAX_DIGITS; ++ounter){
        ci_pack.exp.operand[ounter] = ounter+0x1;
        ci_pack.exp.operand[ounter+MAX_DIGITS] = ounter+MAX_DIGITS+0x1;
    }
    ci_pack.exp.op_code = SUB_GLYPH;
    compute();

        // store_op
    set_initial_state();
    ci_pack.state = ent_op_state;
    volatile BOOLEAN__ success = store_op(MUL_GLYPH);

    set_initial_state();
    ci_pack.state = ent_num_state;
    success = store_op(MUL_GLYPH);  // Should fail

        // store_dig
    set_initial_state();
    ci_pack.state = ent_fin_state;
    success = store_dig(0x8);
    success = store_dig(0x4);
    success = store_dig(0x2);
    success = store_dig(0x1);
    success = store_dig(0x0);   // Should fail
    success = store_op(ADD_GLYPH);
    success = store_dig(0x1);
    success = store_dig(0x0);
    success = store_dig(0x2);
    success = store_dig(0x3);

        // decode_input_type
    UINT8 r = 0x0, c = 0x0;
    volatile input_type in = no_input;
    UINT8 dummy = 0x0;
    for(; r < 0x4; ++r){
        for(c = 0x0; c < 0x4; ++c){
            in = decode_input_type(&dummy, r, c);
            display_dig(100, r*4u+c, 3);
            display_dig(2000, in, 0);
        }
    }

    set_initial_state();
}
#endif

void display_dig(UINT32 add_delay, UINT8 num, UINT8 select){
        // Active low logic
    bankB->OUT.reg &= ~0x0000007F;
        // Provide power to one specific SSD
    bankA->OUT.reg |= 0x000000F0;
    bankA->OUT.reg &= ~(1 << (select + 4u));
    switch(num){
                //  GEF DCBA
        case 0: // 0100 0000
            bankB->OUT.reg |= 0x40;
            break;
        case 1: // 0111 1001
            bankB->OUT.reg |= 0x79;
            break;
        case 2: // 0010 0100
            bankB->OUT.reg |= 0x24;
            break;
        case 3: // 0011 0000
            bankB->OUT.reg |= 0x30;
            break;
        case 4: // 0001 1001
            bankB->OUT.reg |= 0x19;
            break;
        case 5: // 0001 0010
            bankB->OUT.reg |= 0x12;
            break;
        case 6: // 0000 0010
            bankB->OUT.reg |= 0x02;
            break;
        case 7: // 0111 1000
            bankB->OUT.reg |= 0x78;
            break;
        case 8: // 0000 0000
            break;
        case 9: // 0001 0000
            bankB->OUT.reg |= 0x10;
            break;
        case 10: // 0000 1000
            bankB->OUT.reg |= 0x08;
            break;
        case 11: // 0000 0011
            bankB->OUT.reg |= 0x03;
            break;
        case 12: // 0100 0110
            bankB->OUT.reg |= 0x46;
            break;
        case 13: // 0010 0001
            bankB->OUT.reg |= 0x21;
            break;
        case 14: // 0000 0110
            bankB->OUT.reg |= 0x06;
            break;
        case 15: // 0000 1110
            bankB->OUT.reg |= 0x0E;
            break;
        default:    // Non-hexadecimal digit
            break;
    }
    delay_ms(add_delay);
}

UINT8 wait_for_key(UINT32 add_delay, UINT8* row_dest, UINT8* col_dest){
    if(IS_NULL(row_dest) || IS_NULL(col_dest))  return CONTINUE_KEY;

    static UINT8 row_bit = 1u;
    *row_dest = 0u;
        // Turn on negative sign if needed
    bankB->OUT.reg |= 0x00000200;
    bankB->OUT.reg &=
        ~((UINT32)((ci_pack.exp.is_neg >> ci_pack.num_to_display) & 0x1) << 9u);
        // Polling method
    for(;;){
	    // Active low logic
	    bankB->OUT.reg |= 0x0000007F;
	    // Provide power to one specific row
	    bankA->OUT.reg |= 0x000000F0;
	    bankA->OUT.reg &= ~(row_bit << 4u);
            // Extract the four bits we're interested in from
            //   the keypad.
        *col_dest = (bankA->IN.reg >> 16u) & 0xF;
        *row_dest = find_lsob(row_bit);
            // Multiply the delay to set an artificial, rough duty cycle
        display_dig(add_delay*4, ci_pack.exp.operand[*row_dest + 0x4*ci_pack.num_to_display], *row_dest);
            // Check if a button was pressed
        switch(*col_dest){
            case 0xB: return TERMINATION_KEY;   // Terminate the program 
            case 0xD: return TERMINATION_KEY2;  // Terminate the test program
            case 0x0: break;                    // No buttons pressed
            default:
                // Count only the least significant ON bit
                    // Already assigned appropriate value to row_dest
                *col_dest = find_lsob(*col_dest);
                delay_ms(add_delay);
                return CONTINUE_KEY;
        }

            // Prepare for the next row. If we were on the last
            //  row, cycle back to the first row.
        row_bit <<= 1;
            // Take modulous to retrieve current value of row_bit when
            //  we were not on the last row. If that is the case,
            //  reset row_bit to 1.
        row_bit = (row_bit%(1u<<4)) | (row_bit==(1u<<4));
    }
}

void set_initial_state(void){
        // Active low logic
    bankA->OUT.reg |= 0x000000F0;
    bankB->OUT.reg |= 0x000000FF;
    bankB->OUT.reg |= 0x200;

    reset_info_pack();
}

void reset_info_pack(void){
    // Initialize packet information
        // Decrementing migt be faster like in other ARM architectures?
    UINT8 counter = MAX_DIGITS*0x2;
    for(; counter > 0x0; --counter)
        ci_pack.exp.operand[counter-0x1] = 0x0;
    ci_pack.key_code = TERMINATION_KEY;
    ci_pack.exp.index = ci_pack.exp.op_code = ci_pack.exp.is_neg = 0u;
    ci_pack.magnitude = ci_pack.num_to_display = 0u;
    ci_pack.state = ent_num_state;
}

void shutdown(void){
#define LIGHT_DELAY__ 5
    UINT8 counter_last = 100;
    for(; counter_last > 0; --counter_last){
        display_dig(LIGHT_DELAY__, 1, 3);
        display_dig(LIGHT_DELAY__, 3, 2);
        display_dig(LIGHT_DELAY__, 3, 1);
        display_dig(LIGHT_DELAY__, 7, 0);
    }
#undef LIGHT_DELAY__

    set_initial_state();
    // Blink three times to indicate shutdown
    bankA->DIR.reg |= (1<14u);
    UINT8 counter = 0x0;
    for(; counter < 3; ++counter){
        bankA->OUT.reg |= (1<<14u);
        bankB->OUT.reg &= ~0x200;
        delay_ms(500);
        bankA->OUT.reg &= ~(1<<14u);
        bankB->OUT.reg |= 0x200;
        delay_ms(500);
    }
}

    /**********   End function definitions      **********/