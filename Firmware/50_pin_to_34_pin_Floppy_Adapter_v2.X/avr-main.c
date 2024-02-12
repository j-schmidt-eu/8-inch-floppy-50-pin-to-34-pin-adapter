#include "xc8_header.h"

#define REVISON 01

#define DEBOUNCE_TIME 25
#define LONG_PRESS_TIME 2000

#define MOTOR_ENABLE_A PORTBbits.RB1
#define MOTOR_ENABLE_B PORTAbits.RA2
#define DIRECTION PORTAbits.RA3
#define STEP PORTAbits.RA4
#define HEAD_LOAD PORTAbits.RA7
#define TG43_WRITE_CURRENT PORTAbits.RA5
#define DEVICE_SELECT_0 PORTBbits.RB0
#define DEVICE_SELECT_1 PORTBbits.RB2
#define TRACK_0 PORTBbits.RB3
#define WRITE_GATE PORTAbits.RA0
#define INDEX PORTBbits.RB5
#define CMD_SWITCH PORTBbits.RB6
#define EXT_SIGNAL PORTBbits.RB7

#define OUTPUT_ENABLE PORTAbits.RA6

#define _7SEG_1_DOT PORTBbits.RB4
#define _7SEG_2_DOT PORTAbits.RA1

#define TG_FIRST TG_UseThresholds
enum TGXXMode
{
    TG_UseThresholds,
    TG_AlwaysActive,
    TG_AlwaysInactive,
    TG_FromExternalSignal
};
#define TG_LAST TG_FromExternalSignal

#define HL_FIRST HL_FromMotorEnable
enum HeadLoadMode
{
    HL_FromMotorEnable,
    HL_AlwaysActive,
    HL_AlwaysInactive,
    HL_FromExternalSignal
};
#define HL_LAST HL_FromExternalSignal

#define D_FIRST D_Track
enum DisplayMode
{
    D_Track,
    D_Speed,
    D_Revision,
    D_Dark
};
#define D_LAST D_Dark

#define M_FIRST M_Display
enum MenuMode
{
    M_Hidden,
    M_Display,
    M_MaxTrack_A,
    M_TGXX_A,
    M_HeadLoad_A,
    M_TGWriteTheshold_A,
    M_TGReadTheshold_A,
    M_MaxTrack_B,
    M_TGXX_B,
    M_HeadLoad_B,
    M_TGWriteTheshold_B,
    M_TGReadTheshold_B,
    M_Exit
};
#define M_LAST M_Exit

struct DriveParams {
    // Track counting
    volatile int_fast8_t track;
    volatile int_fast8_t maxTrack;
    
    // RPM
    volatile int_fast8_t speedDeviationPercentTenths;
    
    // Modes
    volatile enum HeadLoadMode headLoadMode;
    volatile enum TGXXMode tgXXMode;
    
    // TGXX
    volatile int_fast8_t tgWriteThreshold;
    volatile int_fast8_t tgReadThreshold;
};

uint_fast8_t EepromRead(uint_fast16_t address);
void EepromWrite(uint_fast16_t address, uint_fast8_t data);

volatile __bit blink = 0;

volatile uint_fast16_t counter6000Hz = 0;
volatile uint_fast16_t counter1000Hz = 0;

#define REFERENCE_SPEED_COUNT_6000HZ 1004
#define REFERENCE_SPEED_COUNT_6000HZ_1_Percent 10
#define REFERENCE_SPEED_COUNT_6000HZ_9_9_Percent 99

volatile struct DriveParams driveParams_A;
volatile struct DriveParams driveParams_B;
volatile struct DriveParams * driveParams = &driveParams_A;

__EEPROM_DATA(0, 76, 0, 0, 44, 99, 76, 0);
__EEPROM_DATA(0, 44, 99,       0,0,0,0,0);

int main(void)
{
    INTCONbits.PEIE = 0;
    INTCONbits.GIE = 0;
    RCONbits.IPEN = 1;
    
    // Set system to 64MHz
    OSCCON = 0b01110000;
    OSCTUNE = 0b01000000;
    
    // Disable Timer 0
    INTCONbits.T0IE = 0;
    INTCON2bits.TMR0IP = 0;
    T0CON = 0x00;
    
    // Disable all analog
    ANSELA = 0x00;
    ANSELB = 0x00;
    ANSELC = 0x00;

    TRISA = 0b01011101; // RA0: IN, RA1: OUT, RA2-4: IN, RA5: OUT, RA6: IN, RA7: OUT
    TRISB = 0b11101111; // RB0-3: IN, RB4: OUT, RB5-7: IN
    TRISC = 0b00000000; // RC0-7: OUT

    // Reset outputs
    PORTA &= 0b11111111;

    // Reset 7 segment display
    PORTC = 0x00;
    _7SEG_1_DOT = 0;
    _7SEG_2_DOT = 0;
    
    // Enable Timer 1
    PIE1bits.TMR1IE = 0;
    IPR1bits.TMR1IP = 0;
    T1CON = 0b00000011;
    
    // Enable Timer 2
    IPR1bits.TMR2IP = 0;
    PR2 = 166 - 1;
    T2CON = 0b01111100;
    
    // Enable Timer 4
    IPR5bits.TMR4IP = 0;
    PR4 = 250 - 1;
    T4CON = 0b01111100;
        
    // Capture
    CCPTMRS0 = 0b00000000;
    CCPTMRS1 = 0b00000000;
    
    // Capture 2 (RB3) -> TRACK_0
    IPR2bits.CCP2IP = 1;
    CCP2CON = 0b00000100;
    
    // Capture 3 (RB5) -> INDEX
    IPR4bits.CCP3IP = 0;
    CCP3CON = 0b00000100;
    
    // Capture 5 (RA4) -> STEP
    IPR4bits.CCP5IP = 1;
    CCP5CON = 0b00000100;
    
    // Interrupt 0 (RB0) -> DEVICE_SELECT_0
    INTCON2bits.INTEDG0 = 0;
    // INT0 is always high priority
    
    // Interrupt 2 (RB2) -> DEVICE_SELECT_1
    INTCON2bits.INTEDG2 = 0;
    INTCON3bits.INT2IP = 1;
    
    
    // Inputs
    static __bit externalSignal = 0;
    static __bit cmdSwitch = 0;

    // Outputs
    static __bit headLoad = 0;
    static __bit tg43 = 0;
    static uint_fast8_t bcd1 = 0;
    static uint_fast8_t bcd2 = 0;
    static __bit dot1 = 0;
    static __bit dot2 = 0;

    // Variables
    static uint_fast16_t cmdSwitchStartCount = 0;
    static __bit cmdSwitchLast = 0;
    static __bit cmdSwitchShort = 0;
    static __bit cmdSwitchLong = 0;
    static __bit menuEdit = 0;
    static int_fast8_t speed = 0;
    
    static enum MenuMode menuMode = M_Hidden;
    static enum DisplayMode startDisplayMode = D_FIRST;
    static enum DisplayMode displayMode = D_FIRST;
    
    // EEPROM
    static volatile uint_fast8_t eepromValue = 0;
    static uint_fast16_t eepromAddress = 0;
    
    // Set defaults
    driveParams_A.track = 0;
    driveParams_A.maxTrack = 76;
    driveParams_A.speedDeviationPercentTenths = 0;
    driveParams_A.headLoadMode = HL_FIRST;
    driveParams_A.tgXXMode = TG_FIRST;
    driveParams_A.tgWriteThreshold = 44;
    driveParams_A.tgReadThreshold = 99;
    driveParams_B.track = 0;
    driveParams_B.maxTrack = 76;
    driveParams_B.speedDeviationPercentTenths = 0;
    driveParams_B.headLoadMode = HL_FIRST;
    driveParams_B.tgXXMode = TG_FIRST;
    driveParams_B.tgWriteThreshold = 44;
    driveParams_B.tgReadThreshold = 99;
    
    // Read values to EEPROM
    
    // Global
    eepromValue = EepromRead(eepromAddress++);
    startDisplayMode = (enum DisplayMode)eepromValue;
    
    // Drive A
    eepromValue = EepromRead(eepromAddress++);
    driveParams_A.maxTrack = (int_fast8_t)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_A.tgXXMode = (enum TGXXMode)eepromValue;
            
    eepromValue = EepromRead(eepromAddress++);
    driveParams_A.headLoadMode = (enum HeadLoadMode)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_A.tgWriteThreshold = (int_fast8_t)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_A.tgReadThreshold = (int_fast8_t)eepromValue;

    // Drive B
    eepromValue = EepromRead(eepromAddress++);
    driveParams_B.maxTrack = (int_fast8_t)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_B.tgXXMode = (enum TGXXMode)eepromValue;
            
    eepromValue = EepromRead(eepromAddress++);
    driveParams_B.headLoadMode = (enum HeadLoadMode)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_B.tgWriteThreshold = (int_fast8_t)eepromValue;
    
    eepromValue = EepromRead(eepromAddress++);
    driveParams_B.tgReadThreshold = (int_fast8_t)eepromValue;

    displayMode = startDisplayMode;

    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    
    PIE1bits.TMR2IE = 1;
    PIE5bits.TMR4IE = 1;
    
    PIE2bits.CCP2IE = 1;
    PIE4bits.CCP3IE = 1;
    PIE4bits.CCP4IE = 1;
    PIE4bits.CCP5IE = 1;
    
    INTCONbits.INT0IE = 1;
    INTCON3bits.INT2IE = 1;
    
    while (1)
    {
        if (!DEVICE_SELECT_0 || !DEVICE_SELECT_1)
        {
            externalSignal = EXT_SIGNAL;

            // TG43 to FDD
            {
                switch (driveParams->tgXXMode)
                {
                case TG_UseThresholds:
                    if(!WRITE_GATE)
                        tg43 = (driveParams->track >= driveParams->tgWriteThreshold);
                    else
                        tg43 = (driveParams->track >= driveParams->tgReadThreshold);
                    break;
                case TG_AlwaysActive:
                    tg43 = 1;
                    break;
                case TG_AlwaysInactive:
                    tg43 = 0;
                    break;
                case TG_FromExternalSignal:
                    tg43 = externalSignal;
                    break;
                }
                TG43_WRITE_CURRENT = !tg43;
            }

            // Head load to FDD
            {
                switch (driveParams->headLoadMode)
                {
                case HL_FromMotorEnable:
                    if(!DEVICE_SELECT_0)
                        headLoad = !MOTOR_ENABLE_A;
                    else
                        headLoad = !MOTOR_ENABLE_B;
                    break;
                case HL_AlwaysActive:
                    headLoad = 1;
                    break;
                case HL_AlwaysInactive:
                    headLoad = 0;
                    break;
                case HL_FromExternalSignal:
                    headLoad = externalSignal;
                    break;
                }
                HEAD_LOAD = !headLoad;
            }
            
            OUTPUT_ENABLE = 1;
        }
        else
        {
            OUTPUT_ENABLE = 0;
            
            HEAD_LOAD = 1;
            TG43_WRITE_CURRENT = 1;
            
            // Command button
            {
                cmdSwitch = CMD_SWITCH;

                if (cmdSwitch)
                {
                    if (!cmdSwitchLast)
                    {
                        cmdSwitchStartCount = counter1000Hz;

                        cmdSwitchLast = 1;
                    }
                    else
                    {
                        if(!cmdSwitchLong)
                        {
                            uint_fast16_t counts = counter1000Hz - cmdSwitchStartCount;
                            if(counts >= LONG_PRESS_TIME)
                            {
                                cmdSwitchShort = 0;
                                cmdSwitchLong = 1;
                            }
                            else if(!cmdSwitchShort && counts >= DEBOUNCE_TIME)
                            {
                                cmdSwitchShort = 1;
                            }
                        }
                    }
                }
            }

            // Process commands
            {
                if (!cmdSwitch && cmdSwitchLast)
                {
                    if (cmdSwitchLong)
                    {
                        if (menuMode == M_Hidden)
                        {
                            menuMode = M_FIRST;
                        }
                        else if (menuMode == M_Exit)
                        {
                            menuMode = M_Hidden;
                        }
                        else
                        {
                            menuEdit = !menuEdit;
                            
                            if(!menuEdit)
                            {
                                // Save values to EEPROM

                                eepromAddress = 0x00;
                                
                                eepromValue = (uint_fast8_t)startDisplayMode;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_A.maxTrack;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_A.tgXXMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)driveParams_A.headLoadMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)driveParams_A.tgWriteThreshold;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_A.tgReadThreshold;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_B.maxTrack;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_B.tgXXMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)driveParams_B.headLoadMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)driveParams_B.tgWriteThreshold;
                                EepromWrite(eepromAddress++, eepromValue);
                                
                                eepromValue = (uint_fast8_t)driveParams_B.tgReadThreshold;
                                EepromWrite(eepromAddress++, eepromValue);
                            }
                        }

                        cmdSwitchLong = 0;
                    }
                    else if (cmdSwitchShort)
                    {
                        if (menuMode != M_Hidden)
                        {
                            if (!menuEdit)
                            {
                                if(menuMode == M_LAST)
                                    menuMode = M_FIRST;
                                else
                                    menuMode++;
                            }
                            else
                            {
                                switch(menuMode)
                                {
                                case M_Display:
                                    if(startDisplayMode == D_LAST)
                                        startDisplayMode = D_FIRST;
                                    else
                                        startDisplayMode++;
                                    break;
                                case M_MaxTrack_A:
                                    if(driveParams_A.maxTrack == 80)
                                        driveParams_A.maxTrack = 32;
                                    else
                                        driveParams_A.maxTrack++;
                                    break;
                                case M_TGXX_A:
                                    if(driveParams_A.tgXXMode == TG_LAST)
                                        driveParams_A.tgXXMode = TG_FIRST;
                                    else
                                        driveParams_A.tgXXMode++;
                                    break;
                                case M_HeadLoad_A:
                                    if(driveParams_A.headLoadMode == HL_LAST)
                                        driveParams_A.headLoadMode = HL_FIRST;
                                    else
                                        driveParams_A.headLoadMode++;
                                    break;
                                case M_TGWriteTheshold_A:
                                    if(driveParams_A.tgWriteThreshold == 99)
                                        driveParams_A.tgWriteThreshold = 0;
                                    else
                                        driveParams_A.tgWriteThreshold++;
                                    break;
                                case M_TGReadTheshold_A:
                                    if(driveParams_A.tgReadThreshold == 99)
                                        driveParams_A.tgReadThreshold = 0;
                                    else
                                        driveParams_A.tgReadThreshold++;
                                    break;
                                case M_MaxTrack_B:
                                    if(driveParams_B.maxTrack == 80)
                                        driveParams_B.maxTrack = 32;
                                    else
                                        driveParams_B.maxTrack++;
                                    break;
                                case M_TGXX_B:
                                    if(driveParams_B.tgXXMode == TG_LAST)
                                        driveParams_B.tgXXMode = TG_FIRST;
                                    else
                                        driveParams_B.tgXXMode++;
                                    break;
                                case M_HeadLoad_B:
                                    if(driveParams_B.headLoadMode == HL_LAST)
                                        driveParams_B.headLoadMode = HL_FIRST;
                                    else
                                        driveParams_B.headLoadMode++;
                                    break;
                                case M_TGWriteTheshold_B:
                                    if(driveParams_B.tgWriteThreshold == 99)
                                        driveParams_B.tgWriteThreshold = 0;
                                    else
                                        driveParams_B.tgWriteThreshold++;
                                    break;
                                case M_TGReadTheshold_B:
                                    if(driveParams_B.tgReadThreshold == 99)
                                        driveParams_B.tgReadThreshold = 0;
                                    else
                                        driveParams_B.tgReadThreshold++;
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                        else
                        {
                            if (displayMode == D_LAST)
                                displayMode = D_FIRST;
                            else
                                displayMode++;
                        }

                        cmdSwitchShort = 0;
                    }

                    cmdSwitchLast = 0;
                }
            }
        }

        // Display
        {
            if (menuMode != M_Hidden)
            {
                bcd1 = (uint_fast8_t) menuMode;

                switch(menuMode)
                {
                case M_Display:
                    bcd2 = (uint_fast8_t) startDisplayMode;
                    break;
                case M_MaxTrack_A:
                    bcd1 = (uint_fast8_t) (driveParams_A.maxTrack / 10);
                    bcd2 = (uint_fast8_t) (driveParams_A.maxTrack % 10);
                    break;
                case M_TGXX_A:
                    bcd2 = (uint_fast8_t) driveParams_A.tgXXMode;
                    break;
                case M_HeadLoad_A:
                    bcd2 = (uint_fast8_t) driveParams_A.headLoadMode;
                    break;
                case M_TGWriteTheshold_A:
                    bcd1 = (uint_fast8_t) (driveParams_A.tgWriteThreshold / 10);
                    bcd2 = (uint_fast8_t) (driveParams_A.tgWriteThreshold % 10);
                    break;
                case M_TGReadTheshold_A:
                    bcd1 = (uint_fast8_t) (driveParams_A.tgReadThreshold / 10);
                    bcd2 = (uint_fast8_t) (driveParams_A.tgReadThreshold % 10);
                    break;
                case M_MaxTrack_B:
                    bcd1 = (uint_fast8_t) (driveParams_B.maxTrack / 10);
                    bcd2 = (uint_fast8_t) (driveParams_B.maxTrack % 10);
                    break;
                case M_TGXX_B:
                    bcd2 = (uint_fast8_t) driveParams_B.tgXXMode;
                    break;
                case M_HeadLoad_B:
                    bcd2 = (uint_fast8_t) driveParams_B.headLoadMode;
                    break;
                case M_TGWriteTheshold_B:
                    bcd1 = (uint_fast8_t) (driveParams_B.tgWriteThreshold / 10);
                    bcd2 = (uint_fast8_t) (driveParams_B.tgWriteThreshold % 10);
                    break;
                case M_TGReadTheshold_B:
                    bcd1 = (uint_fast8_t) (driveParams_B.tgReadThreshold / 10);
                    bcd2 = (uint_fast8_t) (driveParams_B.tgReadThreshold % 10);
                    break;
                default:
                    bcd2 = 0xFF;
                    break;
                }

                if (menuEdit ^ cmdSwitchLong)
                {
                    dot1 = dot2 = blink;
                }
                else
                {
                    dot1 = dot2 = 1;
                }
            }
            else
            {
                switch (displayMode)
                {
                case D_Track:
                    bcd1 = (uint_fast8_t) (driveParams->track / 10);
                    bcd2 = (uint_fast8_t) (driveParams->track % 10);
                    dot1 = 0;
                    dot2 = 1;
                    break;

                case D_Speed:
                    speed = driveParams->speedDeviationPercentTenths;
                    if(speed < 0)
                    {
                        dot1 = blink;
                        speed = -speed;
                    }
                    else
                    {
                        dot1 = 1;
                    }
                    bcd1 = (uint_fast8_t) (speed / 10);
                    bcd2 = (uint_fast8_t) (speed % 10);
                    dot2 = 0;
                    break;

                case D_Revision:
                    bcd1 = (uint_fast8_t) (REVISON / 10);
                    bcd2 = (uint_fast8_t) (REVISON % 10);
                    dot1 = blink;
                    dot2 = !blink;
                    break;

                case D_Dark:
                    bcd2 = bcd1 = 0xFF;
                    dot2 = dot1 = 0;
                    break;
                }
            }

            PORTC = (bcd1 & 0xF) | ((bcd2 << 4) & 0xF0);
            _7SEG_1_DOT = !dot1;
            _7SEG_2_DOT = !dot2;
        }
    }
}

void __interrupt(low_priority) isrLowPrio(void)
{
    // Timer 2 ISR
    if (PIR1bits.TMR2IF)
    {
        PIR1bits.TMR2IF = 0;
        
        if(counter6000Hz < (UINT_FAST16_MAX - 1))
            counter6000Hz++;
    }
    
    // Timer 4 ISR
    if (PIR5bits.TMR4IF)
    {
        PIR5bits.TMR4IF = 0;
        
        static uint_fast8_t clockDivider;
        static uint_fast16_t counter500;
        
        if(++clockDivider >= 4)
        {
            clockDivider = 0;
            
            counter1000Hz++;
            
            if(++counter500 >= 500)
            {
                counter500 = 0;
                blink = !blink;
            }
        }
    }
    
    // Capture 3 ISR (INDEX)
    if (PIR4bits.CCP3IF)
    {
        PIR4bits.CCP3IF = 0;
        
        if ((!DEVICE_SELECT_0 || !DEVICE_SELECT_1) && !INDEX)
        {
            TMR2 = 0;

            char sign = REFERENCE_SPEED_COUNT_6000HZ > counter6000Hz;
            uint_fast16_t deviation = sign ? 
                (REFERENCE_SPEED_COUNT_6000HZ - counter6000Hz) :
                (counter6000Hz - REFERENCE_SPEED_COUNT_6000HZ);
            uint_fast16_t percent = deviation >= REFERENCE_SPEED_COUNT_6000HZ_9_9_Percent ?
                99 :
                ((deviation * 10) / REFERENCE_SPEED_COUNT_6000HZ_1_Percent);

            driveParams->speedDeviationPercentTenths = sign ? (int_fast16_t)percent : -((int_fast16_t)percent);

            counter6000Hz = 0;
            TMR2 = 0;
        }
    }
}

void __interrupt(high_priority) isrHighPrio(void)
{
    // Capture 5 ISR (STEP)
    if (PIR4bits.CCP5IF)
    {
        PIR4bits.CCP5IF = 0;
        
        if(!DEVICE_SELECT_0 || !DEVICE_SELECT_1)
        {
            // Wait ~1µs
            Nop();Nop();Nop();Nop();
            Nop();Nop();Nop();Nop();
            Nop();Nop();Nop();Nop();
            //Nop();Nop();Nop();Nop();

            if (!STEP && WRITE_GATE)
            {
                if (!DIRECTION)
                {
                    if (++(driveParams->track) > driveParams->maxTrack)
                        driveParams->track = driveParams->maxTrack;
                }
                else
                {
                    if (--(driveParams->track) < 0)
                        driveParams->track = 0;
                }
            } 
        }
    }
    
    // Capture 2 ISR (TRACK_0)
    if (PIR2bits.CCP2IF)
    {
        PIR2bits.CCP2IF = 0;
        
        if (!DEVICE_SELECT_0 || !DEVICE_SELECT_1)
        {
            // Wait ~250ns
            Nop();Nop();//Nop();Nop();

            if (!TRACK_0)
            {
                driveParams->track = 0;
            } 
        }
    }
    
    // External Interrupt 0 ISR (DEVICE_SELECT_0)
    if (INTCONbits.INT0IF)
    {
        INTCONbits.INT0IF = 0;
        
        if (!DEVICE_SELECT_0)
        {
            // Wait ~250ns
            Nop();Nop();//Nop();Nop();
        
            if (!TRACK_0)
            {
                driveParams_A.track = 0;
            }
            
            driveParams = &driveParams_A;
        }
    }
    
    // External Interrupt 2 ISR (DEVICE_SELECT_1)
    if (INTCON3bits.INT2IF)
    {
        INTCON3bits.INT2IF = 0;
        
        if (!DEVICE_SELECT_1)
        {
            // Wait ~250ns
            Nop();Nop();//Nop();Nop();
        
            if (!TRACK_0)
            {
                driveParams_B.track = 0;
            }
            
            driveParams = &driveParams_B;
        }
    }
}

uint_fast8_t EepromRead(uint_fast16_t address)
{
    EEADR = address & 0xFF;
    EEADRH = (address << 8) & 0xFF;
    
    EECON1bits.EEPGD=0;
    EECON1bits.CFGS=0;
    EECON1bits.RD=1;
    
    return EEDATA;
}

void EepromWrite(uint_fast16_t address, uint_fast8_t data)
{
    EEADR = address & 0xFF;
    EEADRH = (address << 8) & 0xFF;
    
    EEDATA = data;
    
    EECON1bits.EEPGD = 0;
    EECON1bits.CFGS = 0;
    EECON1bits.WREN = 1;
    INTCONbits.GIE = 0;
    
    EECON2 = 0x55;
    EECON2 = 0xAA;
    
    EECON1bits.WR = 1;
    
    INTCONbits.GIE = 1;
    
    while (EECON1bits.WR == 1);
    EECON1bits.WREN = 0;
}