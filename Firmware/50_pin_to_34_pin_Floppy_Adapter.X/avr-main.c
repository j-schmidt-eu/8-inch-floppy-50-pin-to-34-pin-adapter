#include "xc8_header.h"

#define REVISON 01

#define DEBOUNCE_TIME 25
#define LONG_PRESS_TIME 2000

#define MOTOR_ENABLE PORTAbits.RA0
#define DIRECTION PORTAbits.RA1
#define READY PORTAbits.RA2
#define READY_DISK_CHANGE PORTAbits.RA3
#define STEP PORTAbits.RA4
#define HEAD_LOAD PORTAbits.RA7
#define DISK_CHANGE PORTAbits.RA6
#define TG43_WRITE_CURRENT PORTAbits.RA5
#define DEVICE_SELECT PORTBbits.RB0
#define TRACK_0 PORTBbits.RB3
#define WRITE_GATE PORTBbits.RB4
#define INDEX PORTBbits.RB5
#define CMD_SWITCH PORTBbits.RB6
#define EXT_SIGNAL PORTBbits.RB7

#define _7SEG_1_DOT PORTBbits.RB1
#define _7SEG_2_DOT PORTBbits.RB2

#define TG_FIRST TG_Write_Active_44_XX
enum TG43Mode
{
    TG_Write_Active_44_XX,
    TG_Read_Active_44_XX,
    TG_Write_Active_44_XX_Read_Active_44_XX,
    TG_Read_Active_60_XX,
    TG_Write_Active_44_XX_Read_Active_60_XX,
    TG_AlwaysActive,
    TG_AlwaysInactive,
    TG_FromExternalSignal
};
#define TG_LAST TG_FromExternalSignal

#define HL_FIRST HL_FromMotorBEnable
enum HeadLoadMode
{
    HL_FromMotorBEnable,
    HL_AlwaysActive,
    HL_AlwaysInactive,
    HL_FromExternalSignal
};
#define HL_LAST HL_FromExternalSignal

#define RDC_FIRST RDC_FromReady
enum ReadyDiskChangeMode
{
    RDC_FromReady,
    RDC_FromNotDiskChange,
    RDC_FromReadyAndNotDiskChange,
    RDC_FromReadyOrNotDiskChange,
    RDC_FromNotReady,
    RDC_FromDiskChange,
    RDC_FromNotReadyAndDiskChange,
    RDC_FromNotReadyOrDiskChange,
    RDC_AlwaysActive,
    RDC_AlwaysInactive,
    RDC_FromExternalSignal
};
#define RDC_LAST RDC_FromExternalSignal

#define D_FIRST D_Track
enum DisplayMode
{
    D_Track,
    D_Speed,
    D_Revision,
    D_Dark
};
#define D_LAST D_Dark

#define M_FIRST M_TG43
enum MenuMode
{
    M_Hidden,
    M_TG43,
    M_HeadLoad,
    M_ReadyDiskChange,
    M_Display,
    M_MaxTrack,
    M_Exit
};
#define M_LAST M_Exit

__EEPROM_DATA(0,0,0,0,76,0,0,0);

uint_fast8_t EepromRead(uint_fast16_t address);
void EepromWrite(uint_fast16_t address, uint_fast8_t data);

volatile __bit blink = 0;
volatile int_fast8_t track = 0;
volatile int_fast8_t maxTrack = 0;

volatile uint_fast16_t counter6000Hz = 0;
volatile uint_fast16_t counter1000Hz = 0;

#define REFERENCE_SPEED_COUNT_6000HZ 1004
#define REFERENCE_SPEED_COUNT_6000HZ_1_Percent 10
#define REFERENCE_SPEED_COUNT_6000HZ_9_9_Percent 99
volatile int_fast8_t speedDeviationPercentTenths = 0;

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

    TRISA = 0b01010111; // RA0-2: IN, RA3: OUT, RA4: IN, RA5: OUT, RA6: IN, RA7: OUT
    TRISB = 0b11111001; // RB0: IN, RB1-2: OUT, RB3-7: IN
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
    
    // Capture 2
    IPR2bits.CCP2IP = 1;
    CCP2CON = 0b00000100;
    
    // Capture 3
    IPR4bits.CCP3IP = 0;
    CCP3CON = 0b00000100;
    
    // Capture 4
    IPR4bits.CCP4IP = 1;
    CCP4CON = 0b00000100;
    
    // Capture 5
    IPR4bits.CCP5IP = 1;
    CCP5CON = 0b00000100;
    
    // Inputs
    static __bit externalSignal = 0;
    static __bit ready = 0;
    static __bit diskChange = 0;
    static __bit cmdSwitch = 0;

    // Outputs
    static __bit headLoad = 0;
    static __bit readyDiskChange = 0;
    static __bit tg43 = 0;
    static uint_fast8_t bcd1 = 0;
    static uint_fast8_t bcd2 = 0;
    static __bit dot1 = 0;
    static __bit dot2 = 0;

    // Modes
    static enum HeadLoadMode headLoadMode = HL_FIRST;
    static enum ReadyDiskChangeMode readyDiskChangeMode = RDC_FIRST;
    static enum TG43Mode tg43Mode = TG_FIRST;
    static enum MenuMode menuMode = M_Hidden;
    static enum DisplayMode startDisplayMode = D_FIRST;
    static enum DisplayMode displayMode = D_FIRST;

    // Variables
    static uint_fast16_t cmdSwitchStartCount = 0;
    static __bit cmdSwitchLast = 0;
    static __bit cmdSwitchShort = 0;
    static __bit cmdSwitchLong = 0;
    static __bit menuEdit = 0;
    static int_fast8_t speed = 0;
    
    // EEPROM
    static volatile uint_fast8_t eepromValue = 0;
    static uint_fast16_t eepromAddress = 0;
    
    // Read values to EEPROM
    eepromValue = EepromRead(eepromAddress++);
    tg43Mode = (enum TG43Mode)eepromValue;
            
    eepromValue = EepromRead(eepromAddress++);
    headLoadMode = (enum HeadLoadMode)eepromValue;

    eepromValue = EepromRead(eepromAddress++);
    readyDiskChangeMode = (enum ReadyDiskChangeMode)eepromValue;

    eepromValue = EepromRead(eepromAddress++);
    startDisplayMode = (enum DisplayMode)eepromValue;

    eepromValue = EepromRead(eepromAddress++);
    maxTrack = (int_fast8_t)eepromValue;
    
    displayMode = startDisplayMode;

    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    
    PIE1bits.TMR2IE = 1;
    PIE5bits.TMR4IE = 1;
    
    PIE2bits.CCP2IE = 1;
    PIE4bits.CCP3IE = 1;
    PIE4bits.CCP4IE = 1;
    PIE4bits.CCP5IE = 1;
    
    while (1)
    {
        if (!DEVICE_SELECT)
        {
            externalSignal = EXT_SIGNAL;

            // TG43 to FDD
            {
                switch (tg43Mode)
                {
                case TG_Write_Active_44_XX:
                    tg43 = !WRITE_GATE && track >= 44;
                    break;
                case TG_Read_Active_44_XX:
                    tg43 = WRITE_GATE && track >= 44;
                    break;
                case TG_Write_Active_44_XX_Read_Active_44_XX:
                    tg43 = track >= 44;
                    break;
                case TG_Read_Active_60_XX:
                    tg43 = WRITE_GATE && track >= 60;
                    break;
                case TG_Write_Active_44_XX_Read_Active_60_XX:
                    tg43 = (!WRITE_GATE ? track >= 44 : track >= 60);
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
                switch (headLoadMode)
                {
                case HL_FromMotorBEnable:
                    headLoad = !MOTOR_ENABLE;
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

            // Ready / Disk change to CTRL
            {
                ready = !READY;
                diskChange = !DISK_CHANGE;

                switch (readyDiskChangeMode)
                {
                case RDC_FromReady:
                    readyDiskChange = ready;
                    break;
                case RDC_FromNotDiskChange:
                    readyDiskChange = !diskChange;
                    break;
                case RDC_FromReadyAndNotDiskChange:
                    readyDiskChange = ready && !diskChange;
                    break;
                case RDC_FromReadyOrNotDiskChange:
                    readyDiskChange = ready || !diskChange;
                    break;
                case RDC_FromNotReady:
                    readyDiskChange = !ready;
                    break;
                case RDC_FromDiskChange:
                    readyDiskChange = diskChange;
                    break;
                case RDC_FromNotReadyAndDiskChange:
                    readyDiskChange = !ready && diskChange;
                    break;
                case RDC_FromNotReadyOrDiskChange:
                    readyDiskChange = !ready || diskChange;
                    break;
                case RDC_AlwaysActive:
                    readyDiskChange = 1;
                    break;
                case RDC_AlwaysInactive:
                    readyDiskChange = 0;
                    break;
                case RDC_FromExternalSignal:
                    readyDiskChange = externalSignal;
                    break;
                }
                
                READY_DISK_CHANGE = !readyDiskChange;
            }
        }
        else
        {
            PORTA = 0b11111111;
            
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
                                eepromValue = (uint_fast8_t)tg43Mode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)headLoadMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)readyDiskChangeMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)startDisplayMode;
                                EepromWrite(eepromAddress++, eepromValue);

                                eepromValue = (uint_fast8_t)maxTrack;
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
                                case M_TG43:
                                    if(tg43Mode == TG_LAST)
                                        tg43Mode = TG_FIRST;
                                    else
                                        tg43Mode++;
                                    break;
                                case M_HeadLoad:
                                    if(headLoadMode == HL_LAST)
                                        headLoadMode = HL_FIRST;
                                    else
                                        headLoadMode++;
                                    break;
                                case M_ReadyDiskChange:
                                    if(readyDiskChangeMode == RDC_LAST)
                                        readyDiskChangeMode = RDC_FIRST;
                                    else
                                        readyDiskChangeMode++;
                                    break;
                                case M_Display:
                                    if(startDisplayMode == D_LAST)
                                        startDisplayMode = D_FIRST;
                                    else
                                        startDisplayMode++;
                                    break;
                                case M_MaxTrack:
                                    if(maxTrack == 80)
                                        maxTrack = 32;
                                    else
                                        maxTrack++;
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
                case M_TG43:
                    bcd2 = (uint_fast8_t) tg43Mode;
                    break;
                case M_HeadLoad:
                    bcd2 = (uint_fast8_t) headLoadMode;
                    break;
                case M_ReadyDiskChange:
                    bcd2 = (uint_fast8_t) readyDiskChangeMode;
                    break;
                case M_Display:
                    bcd2 = (uint_fast8_t) startDisplayMode;
                    break;
                case M_MaxTrack:
                    bcd1 = (uint_fast8_t) (maxTrack / 10);
                    bcd2 = (uint_fast8_t) (maxTrack % 10);
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
                    bcd1 = (uint_fast8_t) (track / 10);
                    bcd2 = (uint_fast8_t) (track % 10);
                    dot1 = 0;
                    dot2 = 1;
                    break;

                case D_Speed:
                    speed = speedDeviationPercentTenths;
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
    
    // Capture 3 ISR
    if (PIR4bits.CCP3IF)
    {
        PIR4bits.CCP3IF = 0;
        
        if (!DEVICE_SELECT && !INDEX)
        {
            TMR2 = 0;

            char sign = REFERENCE_SPEED_COUNT_6000HZ > counter6000Hz;
            uint_fast16_t deviation = sign ? 
                (REFERENCE_SPEED_COUNT_6000HZ - counter6000Hz) :
                (counter6000Hz - REFERENCE_SPEED_COUNT_6000HZ);
            uint_fast16_t percent = deviation >= REFERENCE_SPEED_COUNT_6000HZ_9_9_Percent ?
                99 :
                ((deviation * 10) / REFERENCE_SPEED_COUNT_6000HZ_1_Percent);

            speedDeviationPercentTenths = sign ? (int_fast16_t)percent : -((int_fast16_t)percent);

            counter6000Hz = 0;
            TMR2 = 0;
        }
    }
}

void __interrupt(high_priority) isrHighPrio(void)
{
    // Capture 5 ISR
    if (PIR4bits.CCP5IF)
    {
        PIR4bits.CCP5IF = 0;
        
        if(!DEVICE_SELECT)
        {
            // Wait ~1µs
            Nop();Nop();Nop();Nop();
            Nop();Nop();Nop();Nop();
            Nop();Nop();Nop();Nop();
            Nop();//Nop();Nop();Nop();

            if (!STEP && WRITE_GATE)
            {
                if (!DIRECTION)
                {
                    if (++track > maxTrack)
                        track = maxTrack;
                }
                else
                {
                    if (--track < 0)
                        track = 0;
                }
            } 
        }
    }
    
    // Capture 2 ISR
    if (PIR2bits.CCP2IF)
    {
        PIR2bits.CCP2IF = 0;
        
        if (!DEVICE_SELECT)
        {
            // Wait ~250ns
            Nop();Nop();//Nop();Nop();

            if (!TRACK_0)
            {
                track = 0;
            } 
        }
    }
    
    // Capture 4 ISR
    if (PIR4bits.CCP4IF)
    {
        PIR4bits.CCP4IF = 0;
        
        if (!DEVICE_SELECT)
        {
            // Wait ~250ns
            Nop();Nop();//Nop();Nop();
        
            if (!TRACK_0)
            {
                track = 0;
            }
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