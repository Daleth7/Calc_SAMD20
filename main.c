#include <asf.h>

    /**********   Start Macro switches   **********/
#define RUN_CHECK
#define NLOG
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

    // Choose whether or not to log progress
#ifndef NLOG
    #include <stdio.h>

    #define LOG(STR) printf(STR)
    #define LOGNUM(NUM) printf("0x%x",(unsigned)NUM)
    #define LOGCHAR(CH) printf("%c",CH)
    #define LOGFLOAT(FNUM) printf("%f", (float)FNUM)

    #define LOGNUMRETURN(NUM) LOGNUM(NUM); LOG("\n")
    #define LOGCHARRETURN(CH) LOGCHAR(CH); LOG("\n")
    #define LOGRETURN(STR) LOG(STR); LOG("\n")
    #define LOGFLOATRETURN(FNUM) LOGFLOAT(FNUM); LOG("\n")
#else
    #define LOG(STR)
    #define LOGRETURN(STR)
    #define LOGNUM(STR)
    #define LOGNUMRETURN(STR)
    #define LOGCHAR(STR)
    #define LOGCHARRETURN(STR)
    #define LOGFLOAT(FNUM)
    #define LOGFLOATRETURN(FNUM)
#endif


    // Short macro functions for inlining common expressions
#define EMBED32(A1, A2, B1, B2) ( A1 | (A2<<4) | (B1<<8) | (B2<<12))
#define IS_NULL(P) (P == NULL)
    /**********   End Macro defines     **********/

    /**********   Start type aliasing   **********/
#include <stdint.h>

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
#define clr_input       4u

#define BOOLEAN__     UINT8
#define TRUE__        1
#define FALSE__       0

typedef struct {
        // Store both operands as four digit numbers in an array. The operands
        //  will follow big endian format, i.e. the most significant digits
        //  will sit at the front of the array. Store individual digits to ease
        //  displaying.
    UINT8 operand[MAX_DIGITS*0x2];
    UINT8 index;
    UINT8 op_code;
} expression_data;

struct calculator_information_packet{
    UINT8 key_code;

        // Let a negative number indicate that the storage is empty
    expression_data exp;
    UINT8 magnitude, num_to_display;
    state_type state;
    BOOLEAN__ cleared_already;
};

    /**********   End type aliasing     **********/

    /**********   Start function prototypes   **********/
UINT32 find_lsob(UINT32);

    // Debugging program
void test_hardware(void);

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
void display_dig(UINT8 dig_to_display, UINT8 select);

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

#ifdef RUN_CHECK
    test_hardware();
#endif

//    run_calculator();

    shutdown();

    delay_ms(100);
    LOGRETURN("Program Terminated.");

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
    /* Set high drive strength ?
    unsigned short i = 0;
    for(; i < 8; ++i){
            // Enable input (bit 1).
            //  Note that the pins are externally pulled low,
            //  so there is no need to internally pull the pin logic.
        bankA->PINCFG[i].reg |= (1<<6);
    }
    */

        // For reading input from keypad. 1111 0000 0000 0000 0000 
        //  Active high logic for input.
    bankA->DIR.reg &= ~0x000F0000;
    unsigned short i = 16;
    for(; i < 20; ++i){
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
    UINT32 op1=0x0, op2=0x0;
    UINT8 counter = MAX_DIGITS;
    UINT32 factor = 1;
    for(; counter > 0x0; ++counter, factor *= 10){
        op1 += ci_pack.exp.operand[counter-0x1] * factor;
        op2 += ci_pack.exp.operand[counter+0x3] * factor;
    }

    switch(ci_pack.exp.op_code){
        case ADD_GLYPH: op1 += op2; break;
        case SUB_GLYPH: op1 -= op2; break;
        case MUL_GLYPH: op1 *= op2; break;
        case DIV_GLYPH: op1 /= op2; break;
        default:
            LOGRETURN("Invalid operation code.");
            return;
    }
    op2 = op1;
        // Reset some values and store the result in operand 1's slot
    for(counter = MAX_DIGITS; counter > 0x0; --counter, op1 /= 10){
        ci_pack.exp.operand[counter] = op1 % 0xA;
        ci_pack.exp.operand[counter+0x3] = 0x0;
    }
    ci_pack.exp.operand[1u] = -1.0f;
    ci_pack.exp.index = ci_pack.exp.op_code = 0u;

    return op2 >= factor*0xA;
}

BOOLEAN__ store_dig(UINT8 new_dig){
    switch(ci_pack.state){
        case ent_fin_state:
            // Calculation was finished, so reset all values
            //  and store new digit. Also go to the entering
            //  number state.
            ci_pack.magnitude = 0u;
            {
                UINT8 counter = 0u;
                for(; counter < MAX_DIGITS; ++counter)
                    ci_pack.exp.operand[counter] = 0x0;
            }
            ci_pack.state = ent_num_state;
            ci_pack.num_to_display = 0u;
            ci_pack.exp.index = 0u;
        case ent_num_state:
            // Program is ready to accept a new digit.
            if (ci_pack.magnitude == MAX_MAGNITUDE){
                // There were already MAX_MAGNITUDE digits stored
                return FALSE__;
            }
            if (!(ci_pack.magnitude || new_dig)){
                ci_pack.exp.operand[ci_pack.exp.index] = 0.0f;
                return TRUE__;
            }
                // Push digits left if necessary then insert new digit
            ci_pack.exp.operand[ci_pack.exp.index] *= (ci_pack.magnitude > 0u)*10.0f;
            ci_pack.exp.operand[ci_pack.exp.index] += new_dig;
                // Update magnitude. If magnitude is at max, update
                //  index as well.
            ++(ci_pack.magnitude);
            if (ci_pack.exp.index)    ci_pack.num_to_display = ci_pack.exp.index;
            if(ci_pack.magnitude == MAX_MAGNITUDE){
                ci_pack.magnitude = 0u;
                if(ci_pack.exp.index){
                    ++ci_pack.exp.index;
                }
                ci_pack.state = ent_op_state;
            }
            return TRUE__;
        case ent_op_state:
            // Program was expecting an operator.
            //  Guarantee that expression is not affected.
            return FALSE__;
        default:    // Should never happen
            LOGRETURN("Invalid state");
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
            if (ci_pack.exp.index || !ci_pack.magnitude){
                // Either the current operand has no digits
                //  or the program is already on the second
                //  operand.
                return FALSE__;
            }
            ci_pack.exp.op_code = new_op;
            ci_pack.exp.index = 1u;
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
            LOGRETURN("Pressed division key");
            *i_code = DIV_GLYPH;
            return op_input;
        case 0x4:
            LOGRETURN("Pressed multiplication key");
            *i_code = MUL_GLYPH;
            return op_input;
        case 0x3:
            LOGRETURN("Pressed subtraction key");
            *i_code = SUB_GLYPH;
            return op_input;
        case 0x1:
            LOGRETURN("Pressed addition key");
            *i_code = ADD_GLYPH;
            return op_input;
    // Enter and clear
        case 0xC:
            *i_code = 0;
            return ent_input;
        case 0x8:
            *i_code = 0;
            return clr_input;
    // Digits
        case 0x2:
            *i_code = 0;
            LOGRETURN("Entered number: 0");
            return dig_input;
        default:
           *i_code = (0x3 - row) * 0x3 + (0x4 - col);
            LOG("Entered number: ");
            LOGNUMRETURN(*i_code);
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

void test_hardware(void){
    // Run visual check on seven segment displays
    LOGRETURN("Running visual check...");
    // Turn on all LEDs
    bankA->OUT.reg &= ~0xF0;
    bankB->OUT.reg &= ~0xFF;
    delay_ms(2000);

    // Turn off one segment at a time
    UINT32 lit_bit = 1;
#define TEST_DELY__ 100
    for (; lit_bit < (1u << 8); lit_bit <<= 1){
        bankB->OUT.reg |= lit_bit;
        delay_ms(TEST_DELY__);
    }
    LOGRETURN("Displaying 1 number at a time.");

    // Turn on one number at a time
    lit_bit = 0u;
    for (; lit_bit < 16u; ++lit_bit){
        display_dig(lit_bit, 1);
        delay_ms(TEST_DELY__ * 3);
    }

    // Run semi-infinite loop to test keypad input
    LOGRETURN("Reading keypad...");
    UINT8 row = 0x0, col = 0x0;
    while((wait_for_key(5, &row, &col)) != TERMINATION_KEY2){
        display_dig(row*4u+col, row);
        delay_ms(500);
#ifndef NLOG
        UINT8 sub_code;
        LOG("\tInput type was a");
        switch (decode_input_type(key_code, &sub_code)){
            case dig_input:
                LOG(" digit ( ");
                LOGNUM(sub_code);
                LOG(" )");
                break;
            case op_input:
                LOG("n operation ( ");
                switch (sub_code){
                    case 1: LOG("+"); break;
                    case 2: LOG("-"); break;
                    case 3: LOG("*"); break;
                    case 4: LOG("/"); break;
                    default:    break;
                }
                LOG(" )");
                break;
            case ent_input:
                LOGRETURN("n ENTER");
                break;
            case clr_input:
                LOGRETURN(" clear");
                break;
            default: break;
        }
        LOGRETURN("");
#endif
    }
}

void display_dig(UINT8 num, UINT8 select){
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
            LOGRETURN("\tInvalid digit.");
            break;
    }
    delay_ms(5);
}

UINT8 wait_for_key(UINT32 add_delay, UINT8* row_dest, UINT8* col_dest){
    if(IS_NULL(row_dest) || IS_NULL(col_dest))  return CONTINUE_KEY;

    static UINT8 row_bit = 1u;
    *row_dest = 0u;
        // Polling method
    for(;;){
	    // Active low logic
	    bankB->OUT.reg |= 0x0000007F;
	    // Provide power to one specific row
	    bankA->OUT.reg |= 0x000000F0;
	    bankA->OUT.reg &= ~(row_bit << 4u);
        delay_ms(5);
            // Extract the four bits we're interested in from
            //   the keypad.
        *col_dest = (bankA->IN.reg >> 16u) & 0xF;
            // Quickly display the current digit while assigning value
            //  to *row_dest. Inline the expression so that row_dest
            //  is dereferenced once instead of twice. This is important since
            //  row_dest is dereferenced each iteration.
        diaplay_dig(ci_pack.exp.operand[*row_dest = find_lsob(row_bit)], row);
            // Check if a button was pressed
        switch(*col_dest){
            case 0xB: return TERMINATION_KEY;   // Terminate the program 
            case 0xD: return TERMINATION_KEY2;  // Terminate the test program
            case 0x0: break;                    // No buttons pressed
            default:
                    // Count only the least significant ON bit
                LOG("Input is: ");
                LOGNUMRETURN(bankA->IN.reg);
                LOG("Pressed key #");
                // Already assigned appropriate value to row_dest
                *col_dest = find_lsob(*col_dest);
                    LOGNUM((*row_dest-1) * 4u + *col_dest - 1u);
                    LOG("\t(Column: ");
                    LOGNUM(*row_dest);
                    LOG(", Row: ");
                    LOGNUM(*col_dest);
                    LOGRETURN(")");
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

    // Initialize packet information
        // Decrementing migt be faster like in other ARM architectures?
    UINT8 counter = 0x8;
    for(; counter > 0x0; --counter)
        ci_pack.exp.operand[counter-0x1] = 0x0;
    ci_pack.key_code = TERMINATION_KEY;
    ci_pack.exp.index = ci_pack.exp.op_code = 0u;
    ci_pack.magnitude = 0u;
    ci_pack.num_to_display = 0u;
    ci_pack.state = ent_num_state;
    ci_pack.cleared_already = FALSE__;
}

void shutdown(void){
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