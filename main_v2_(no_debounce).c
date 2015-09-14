#include <asf.h>

    /**********   Start Macro switches   **********/
//#define RUN_CHECK
//#define RUN_SOFT_CHECK
    /**********   End Macro switches    **********/

    /**********   Start Macro defines   **********/

#define MAX_DIGITS    4u // Maximum digits supported
#define MAX_MAGNITUDE 4u // Maximum input size supported
#define MAX_PRECISION (MAX_DIGITS - MAX_MAGNITUDE)

    // These are codes for special key combinations.
#define TERMINATION_KEY     0xF
#define TERMINATION_KEY2    0xE
#define CONTINUE_KEY        0xD

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
#define term_input      16u

#define BOOLEAN__     UINT8
#define TRUE__        1
#define FALSE__       0

#define NULL_DIG 255

typedef struct {
        // Store both operands as four digit numbers in an array. The operands
        //  will follow big endian format, i.e. the most significant digits
        //  will sit at the front of the array. Store individual digits to ease
        //  displaying.
        //  Use NULL_DIG to indicate an empty slot.
    UINT8       operand[MAX_DIGITS*0x2];
    UINT8       index;
    UINT8       op_code;
        // bit0 ~ operand 1 | bit 1 ~ operand 2
    UINT8       is_neg;
} expression_data;

struct calculator_information_packet{
        // Let a negative number indicate that the storage is empty
    expression_data exp;
    UINT8 magnitude, num_to_display;
    state_type state;
};

    /**********   End type aliasing     **********/

    /**********   Start function prototypes   **********/

    // Debugging programs
#ifdef RUN_CHECK
    void test_hardware(void);
#endif
#ifdef RUN_SOFT_CHECK
    void test_software(void);
#endif

    // Main program
void run_calculator(void);

void delete_last_entry(void);

    // Find last significant ON bit
UINT32 find_lsob(UINT32);

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
void display_dig(
    UINT32 add_delay, UINT8 dig_to_display, UINT8 select,
    BOOLEAN__ show_dot, BOOLEAN__ show_sign
);

    // Wait for keypad input and return the key that was pressed.
    //  0 - 15 denotes key on keypad from bottom to top then left to right
    //  TERMINATION_KEY2 denotes combo key to terminate program
    //  TERMINATION_KEY denotes combo key to terminate test program
    //    13 denotes the clear key
    //    5 denotes the enter key
void check_key(UINT8* row_dest, UINT8* col_dest);
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
    // An indicator
void blink_rdy(UINT32 add_delay);
    /**********   End function prototypes   **********/

    /**********   Start global variables   **********/
        // Since the majority of functions in this file depend
        //  on modifying common variables, go ahead and make them
        //  global.
static struct calculator_information_packet cip;

        // These variables are shared by multiple IO functions.
static Port* port;
static PortGroup *bankA, *bankB;
    /**********    End global variables    **********/

    /**********   Start function definitions   **********/
int main(void){

    configure_ports();

    volatile BOOLEAN__ start = TRUE__;
        // Force an infinite loop. In a future implementation using
        //  interrupts, have the device go to sleep while waiting
        //  for prompt.
    while(TRUE__){
        if(start){
            delay_ms(50);

            set_initial_state();

        #ifdef RUN_SOFT_CHECK
            test_software();
        #endif

        #ifdef RUN_CHECK
            test_hardware();
        #endif

        run_calculator();

            shutdown();
        }
        bankA->OUT.reg = ~0x10;
        start = ((bankA->IN.reg >> 16u) & 0xF) == 0xF;
    }

    return 0;
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
        display_dig(5, lit_bit, 1, FALSE__, FALSE__);
        delay_ms(TEST_DELY__ * 3);
    }
    // Run semi-infinite loop to test keypad input
    UINT8 row = 0x0, col_byte = 0x0, button = 0xFF;
    while(
        check_key(&row, &col_byte),
        (decode_input_type(&button, row, col_byte) != term_input
            && button != TERMINATION_KEY2)
    ){
        if(col_byte)
            display_dig(333, row*4u+find_lsob(col_byte), row, FALSE__, FALSE__);
        display_dig(0, NULL_DIG, row, FALSE__, FALSE__);
    }
}
#endif

#ifdef RUN_SOFT_CHECK
void test_software(void){
        // compute
    set_initial_state();
    UINT8 ounter = 0x0;
    for(; ounter < MAX_DIGITS; ++ounter){
        cip.exp.operand[ounter] = ounter+0x1;
        cip.exp.operand[ounter+MAX_DIGITS] = ounter+MAX_DIGITS+0x1;
    }
    cip.exp.op_code = SUB_GLYPH;
    compute();

        // store_op
    set_initial_state();
    cip.state = ent_op_state;
    volatile BOOLEAN__ success = store_op(MUL_GLYPH);

    set_initial_state();
    cip.state = ent_num_state;
    success = store_op(MUL_GLYPH);  // Should fail

        // store_dig
    set_initial_state();
    cip.state = ent_fin_state;
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
            display_dig(100, r*4u+c, 3, FALSE__, FALSE__);
            display_dig(2000, in, 0, FALSE__, FALSE__);
        }
    }

    set_initial_state();
}
#endif

void run_calculator(){
    set_initial_state();

    UINT8 button = 0x0;
    UINT8 row = 0xF0, col_byte = 0xF0;
    input_type in_type = no_input;
        // Start processing key presses
    while(
        check_key(&row, &col_byte),
        ((in_type = decode_input_type(&button, row, col_byte)) != term_input
            && button != TERMINATION_KEY)
    ){
#define COM_DELAY__ 2
        switch(in_type){
            case del_input:
                delay_ms(COM_DELAY__);
                delete_last_entry();
                continue;
            case dig_input:
                display_dig(COM_DELAY__, button, MAX_DIGITS-1, FALSE__, FALSE__);
                if (!store_dig(button)){
                    /*Consider doing something*/
                }
                break;
            case op_input:
                delay_ms(COM_DELAY__);
                if (!store_op(button)){
                    /*Consider doing something*/
                }
                break;
            case ent_input:
                delay_ms(COM_DELAY__);
                if(
                    cip.state != ent_fin_state &&
                    cip.exp.index >= MAX_DIGITS
                ){
                    compute();
                    cip.state = ent_fin_state;
                    cip.num_to_display = 0u;
                }
                break;
            case no_input:  break;
            default:    break;
        }
        display_dig(
            3,
            cip.exp.operand[cip.num_to_display*4u+MAX_DIGITS-1u-row],
            row,
            row == MAX_DIGITS-1-((cip.exp.index-1u)%MAX_DIGITS)+MAX_PRECISION,
            (cip.exp.is_neg >> cip.num_to_display) & 0x1
            );
        display_dig(1, NULL_DIG, MAX_DIGITS-1u-row, FALSE__, FALSE__);
    }
}

void delete_last_entry(void){
    UINT8 counter = MAX_DIGITS;
    switch(cip.exp.index){
        case 0x0:
            cip.exp.operand[cip.exp.index] = NULL_DIG;
            cip.exp.is_neg = 0x0;
            cip.magnitude = 0x0;
            return;
        case 0x4:
            if(cip.state == ent_op_state){
                cip.exp.operand[MAX_DIGITS - 1] = NULL_DIG;
                cip.state = ent_num_state;
            } else {    // Operator already entered but digit not
                cip.state = ent_op_state;
            }
            for(
                counter = MAX_DIGITS, cip.magnitude = MAX_DIGITS;
                counter > 0x0 && cip.exp.operand[counter-1] == NULL_DIG;
                --counter
            )   --cip.magnitude;
            cip.exp.index = cip.magnitude;
            cip.num_to_display = 0x0;
            break;
        default:
            --cip.exp.index;
            cip.exp.operand[cip.exp.index] = NULL_DIG;
            cip.state = ent_num_state;
                // Should never be 0 before decrement
            --cip.magnitude;
            break;
    }
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
    blink_rdy(100);
}

BOOLEAN__ compute(){
        // Retrieve the actual operands
    INT32 op1=0x0, op2=0x0;
    UINT32 hold = 0x0;
    UINT8 counter = MAX_DIGITS;
    INT32 factor = 1;
        // Need to use two separate for loop due to factor being dynamic
    for(; counter > 0x0; --counter){
        hold = cip.exp.operand[counter - 0x1];
        if(hold == NULL_DIG) continue;
        op1 += hold * factor;
        factor *= 10;
    }
    for(counter = MAX_DIGITS, factor = 1; counter > 0x0; --counter){
        hold = cip.exp.operand[counter - 0x1 + MAX_DIGITS];
        if(hold == NULL_DIG) continue;
        op2 += hold * factor;
        factor *= 10;
    }
    op1 *= (INT32)(cip.exp.is_neg & 0x1)*(-2) + 1;
    op2 *= (INT32)(cip.exp.is_neg & 0x2)*(-2) + 1;

    switch(cip.exp.op_code){
        case ADD_GLYPH: op1 += op2;         break;
        case SUB_GLYPH: op1 -= op2;         break;
        case MUL_GLYPH: op1 *= op2;         break;
        case DIV_GLYPH: op1 = op2 / op1;    break;
        default:                            return FALSE__;
    }

    if(op1 < 0){
        cip.exp.is_neg |= 0x1;
        op2 = (op1 *= -1);
    } else {
        op2 = op1;
    }
        // Reset some values and store the result in operand 1's slot
    for(counter = MAX_DIGITS; counter > 0x0; --counter, op1 /= 10){
        cip.exp.operand[counter-0x1] = op1 % 10;
        cip.exp.operand[counter-0x1+MAX_DIGITS] = NULL_DIG;
    }
    cip.magnitude = cip.exp.index = MAX_DIGITS;
    while(cip.exp.operand[0] == 0x0 && cip.exp.index > 0x0){
        for(counter = 0x1; counter < MAX_DIGITS; ++counter){
            cip.exp.operand[counter-1] = cip.exp.operand[counter];
            cip.exp.operand[counter] = NULL_DIG;
        }
        --cip.magnitude;
        --cip.exp.index;
    }
    cip.exp.op_code = 0u;
    cip.exp.is_neg &= ~(0x2);

    return op2 >= factor*0xA;
}

BOOLEAN__ store_dig(UINT8 new_dig){
    switch(cip.state){
        case ent_fin_state:
            // Calculation was finished, so reset all values
            //  and store new digit. Also go to the entering
            //  number state.
            reset_info_pack();
        case ent_num_state:
            // Program is ready to accept a new digit.
            if (cip.magnitude == MAX_MAGNITUDE){
                // There were already MAX_MAGNITUDE digits stored
                return FALSE__;
            }
            if (!(cip.magnitude || new_dig)){
                    // 0 condition
                return TRUE__;
            }
            cip.exp.operand[cip.exp.index] = new_dig;
                // Update magnitude and index.
            ++cip.magnitude;
            ++cip.exp.index;
            if (cip.exp.index > MAX_DIGITS)
                cip.num_to_display = 0x1;
            if(cip.magnitude == MAX_MAGNITUDE){
                cip.magnitude = 0u;
                cip.state = ent_op_state;
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
    switch (cip.state){
        case ent_op_state:
            cip.exp.op_code = new_op;
            cip.state = ent_num_state;
            cip.num_to_display = 0x1;
            return TRUE__;
        case ent_num_state:
            if (cip.exp.index >= MAX_DIGITS){
                // Either the current operand has no digits
                //  or the program is already on the second
                //  operand.
                return FALSE__;
            }
            cip.exp.op_code = new_op;
            cip.exp.index = MAX_DIGITS;
            cip.magnitude = 0u;
            cip.num_to_display = 0x1;
            return TRUE__;
        default:
            return FALSE__;
    }
}

input_type decode_input_type(UINT8* i_code, UINT8 row, UINT8 col){
    if (IS_NULL(i_code)){
        return no_input;
    }

        // Check if a button was pressed
    switch(col){
        case 0xB:
            if(row != 3)    break;
            *i_code = TERMINATION_KEY;      // Terminate the program 
            return term_input;
        case 0xD:
            if(row != 3)    break;
            *i_code = TERMINATION_KEY2;     // Terminate the test program
            return term_input;
        case 0x0:
            return no_input;
        default:    break;
    }
    col = find_lsob(col);

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

void display_dig(
    UINT32 add_delay, UINT8 num, UINT8 select,
    BOOLEAN__ show_dot, BOOLEAN__ show_sign
){
        // Active low logic
    bankB->OUT.reg |= 0x000000FF;
        // Provide power to one specific SSD
    bankA->OUT.reg |= 0x000000F0;
    bankA->OUT.reg &= ~(1 << (select + 4u));
    switch(num){
                //  GEF DCBA
        case 0: // 0100 0000
            bankB->OUT.reg &= ~0xBF;
            break;
        case 1: // 0111 1001
            bankB->OUT.reg &= ~0x86;
            break;
        case 2: // 0010 0100
            bankB->OUT.reg &= ~0xDB;
            break;
        case 3: // 0011 0000
            bankB->OUT.reg &= ~0xCF;
            break;
        case 4: // 0001 1001
            bankB->OUT.reg &= ~0xE6;
            break;
        case 5: // 0001 0010
            bankB->OUT.reg &= ~0xED;
            break;
        case 6: // 0000 0010
            bankB->OUT.reg &= ~0xFD;
            break;
        case 7: // 0111 1000
            bankB->OUT.reg &= ~0x87;
            break;
        case 8: // 0000 0000
            bankB->OUT.reg &= ~0x7F;
            break;
        case 9: // 0001 0000
            bankB->OUT.reg &= ~0xEF;
            break;
        case 10: // 0000 1000
            bankB->OUT.reg &= ~0xF7;
            break;
        case 11: // 0000 0011
            bankB->OUT.reg &= ~0xFC;
            break;
        case 12: // 0100 0110
            bankB->OUT.reg &= ~0xB9;
            break;
        case 13: // 0010 0001
            bankB->OUT.reg &= ~0xDE;
            break;
        case 14: // 0000 0110
            bankB->OUT.reg &= ~0xF9;
            break;
        case 15: // 0000 1110
            bankB->OUT.reg &= ~0xF1;
            break;
        default:    // Non-hexadecimal digit or negative
            bankB->OUT.reg |= 0xFF;
            show_dot = FALSE__;
            break;
    }
    if(show_dot)    bankB->OUT.reg &= ~0x00000080;
    else            bankB->OUT.reg |=  0x00000080;
    if(show_sign)   bankB->OUT.reg &= ~0x00000200;
    else            bankB->OUT.reg |=  0x00000200;
    delay_ms(add_delay);
}

void check_key(UINT8* row_dest, UINT8* col_dest){
    if(IS_NULL(row_dest) || IS_NULL(col_dest))  return;

    static UINT8 cur_row = 0u;
    // Provide power to one specific row
    bankA->OUT.reg |= 0x000000F0;
    bankA->OUT.reg &= ~(1u << (4u + cur_row));
        // Extract the four bits we're interested in from
        //   the keypad.
    *col_dest = (bankA->IN.reg >> 16u) & 0xF;
    *row_dest = cur_row;

        // Prepare for the next row. If we were on the last
        //  row, cycle back to the first row.
        // Take modulous to retrieve current value of cur_row when
        //  we were not on the last row. If that is the case,
        //  reset row_bit to 0.
    cur_row = (cur_row+1u)%4u;
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
        cip.exp.operand[counter-0x1] = NULL_DIG;
    cip.exp.index = cip.exp.op_code = cip.exp.is_neg = 0u;
    cip.magnitude = cip.num_to_display = 0u;
    cip.state = ent_num_state;
}

void shutdown(void){
#define LIGHT_DELAY__ 5
    UINT8 counter_last = 100;
    for(; counter_last > 0; --counter_last){
        display_dig(LIGHT_DELAY__, 1, 3, FALSE__, FALSE__);
        display_dig(LIGHT_DELAY__, 3, 2, FALSE__, FALSE__);
        display_dig(LIGHT_DELAY__, 3, 1, FALSE__, FALSE__);
        display_dig(LIGHT_DELAY__, 7, 0, FALSE__, FALSE__);
    }
#undef LIGHT_DELAY__

    set_initial_state();
    blink_rdy(333);
}

void blink_rdy(UINT32 add_delay){
    // Blink three times to indicate shutdown
    bankA->DIR.reg |= (1<14u);
    UINT8 counter = 0x0;
    bankB->OUT.reg = 0xFF;
    for(; counter < 3; ++counter){
        bankA->OUT.reg |= (1<<14u);
        bankB->OUT.reg &= ~0x200;
        delay_ms(add_delay);
        bankA->OUT.reg &= ~(1<<14u);
        bankB->OUT.reg |= 0x200;
        delay_ms(add_delay);
    }
    bankB->OUT.reg = 0xFF;
}

    /**********   End function definitions      **********/