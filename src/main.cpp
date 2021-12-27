#include "mbed.h"
#include "ds3231.h"
#include <chrono>
#include <cstdint>


DigitalOut ANODE_H1(PB_0);
DigitalOut ANODE_H2(PB_7);
DigitalOut ANODE_M1(PB_6);
DigitalOut ANODE_M2(PB_1);

DigitalOut CATODE_BCD_A(PA_1);
DigitalOut CATODE_BCD_B(PA_0);
DigitalOut CATODE_BCD_C(PA_4);
DigitalOut CATODE_BCD_D(PA_3);

DigitalOut SECS(PB_5);

InterruptIn exti1(PA_7);
InterruptIn exti2(PA_6);

DigitalIn in_minutes(PA_7);
DigitalIn in_hours(PA_6);

Ds3231 rtc(PA_10, PA_9);
ds3231_time_t rtc_time;
ds3231_time_t rtc_time_sm_wrapper;

bool increment_hours = false;
bool increment_minutes = false;

bool enable_slot_machine = true;
bool run_slot_machine = false;

bool nixi_burning_mode = false;


unsigned char rtc_address = 0x68;
int nixi_pose = 0;





bool nixi_is_glowing(int x) {
    return x == nixi_pose;
}

int number_to_bcd(int number, int pose) {
    return (number >> pose) & 0x0001;
}

void set_all_catodes(int c) {
    CATODE_BCD_A = c;
    CATODE_BCD_B = c;
    CATODE_BCD_C = c;
    CATODE_BCD_D = c;
}

void anodes_off() {
    ANODE_H1 = 0;
    ANODE_H2 = 0;
    ANODE_M1 = 0;
    ANODE_M2 = 0;
}

void anodes_on() {
    ANODE_H1 = 1;
    ANODE_H2 = 1;
    ANODE_M1 = 1;
    ANODE_M2 = 1;
}

void starting_sequence() {
    int sleep_time = 1000;
    anodes_off();
    SECS = 0;
    set_all_catodes(0);
    
    ANODE_H1 = 1;
    ThisThread::sleep_for(sleep_time);

    ANODE_H2 = 1;
    ThisThread::sleep_for(sleep_time);

    SECS = 1;
    ThisThread::sleep_for(sleep_time);

    ANODE_M1 = 1;
    ThisThread::sleep_for(sleep_time);

    ANODE_M2 = 1;
    ThisThread::sleep_for(sleep_time);
}





void init_rtc_time(ds3231_time_t& rtc_time) {

    ds3231_cntl_stat_t rtc_control_status = {0,0}; 
    rtc.set_cntl_stat_reg(rtc_control_status);
    rtc.get_time(&rtc_time);
    rtc_time.mode = false;
    rtc_time.am_pm = false;
    rtc.set_time(rtc_time);
    rtc_time_sm_wrapper = rtc_time;
}

void init_input_buttons() {
    in_minutes.mode(PullUp);
    in_hours.mode(PullUp);
}

void rtcThreadFnc() {
    while(true) {
        rtc.get_time(&rtc_time);
        if (rtc_time.seconds == 0) {
            if (enable_slot_machine) {
                run_slot_machine = true;
            }
        }
        ThisThread::sleep_for(1000);
    }
}

void updateTimeThreadFnc() {
    while (true) {
        if(increment_hours) {
            rtc_time.hours = (rtc_time.hours+1) % 24;
            rtc.set_time(rtc_time);
            increment_hours = false;
        }

        if(increment_minutes) {
            rtc_time.minutes = (rtc_time.minutes+1) % 60;
            rtc.set_time(rtc_time);
            increment_minutes = false;
        }
        ThisThread::sleep_for(100);
    }
}


void inputButtonsThreadFnc() {

    bool enable_increment_minutes = false;
    bool enable_increment_hours = false;     

    uint32_t minutes_button_hold_counter = 0;

    while (true) {
        if(enable_increment_minutes && in_minutes.read() == 0) {
            increment_minutes = true;
            enable_increment_minutes = false;
        }
        if(enable_increment_hours && in_hours.read() == 0) {
            increment_hours = true;
            enable_increment_hours = false;
        }
        
        if (in_minutes.read() == 1) { 
            enable_increment_minutes = true;
            minutes_button_hold_counter = 0;
        } 
        else {
            minutes_button_hold_counter += 1;
        }

        if (in_hours.read() == 1) { 
            enable_increment_hours = true;
        }

        if (minutes_button_hold_counter == 30) {
            nixi_burning_mode = !nixi_burning_mode;
        }

        ThisThread::sleep_for(100);
    }
}

void secondTogglerThreadFnc() {
    while(true) {
        SECS = rtc_time.seconds % 2 == 0;
        ThisThread::sleep_for(100);
    }
} 

void nixiThreadFnc() {
    
    uint8_t nixi_burning_counter = 0;

    while (true) {


        if (!nixi_burning_mode) {

            anodes_off();

            int number = 0;
            if (nixi_is_glowing(0)) {
                number = rtc_time_sm_wrapper.minutes % 10;
            } else if (nixi_is_glowing(1)) {
                number = (rtc_time_sm_wrapper.minutes % 100) / 10;
            } else if (nixi_is_glowing(2)) {
                number = rtc_time_sm_wrapper.hours % 10;
            } else if (nixi_is_glowing(3)){
                number = (rtc_time_sm_wrapper.hours % 100) / 10;  
            }
            
            CATODE_BCD_A = number_to_bcd(number, 0);
            CATODE_BCD_B = number_to_bcd(number, 1);
            CATODE_BCD_C = number_to_bcd(number, 2);
            CATODE_BCD_D = number_to_bcd(number, 3);

            ANODE_H1 = nixi_is_glowing(3);
            ANODE_H2 = nixi_is_glowing(2);
            ANODE_M1 = nixi_is_glowing(1);
            ANODE_M2 = nixi_is_glowing(0);

            nixi_pose++;
            if (nixi_pose > 3) {nixi_pose = 0;}

            ThisThread::sleep_for(1);
        
        } else {

            anodes_off();
            SECS = 0;
            if (nixi_burning_counter < 10) {
                ANODE_M2 = true;
            } else if (nixi_burning_counter < 20) {
                ANODE_M1 = true;
            } else if (nixi_burning_counter < 30) {
                ANODE_H2 = true;
            } else {
                ANODE_H1 = true;
            }

            uint8_t number = nixi_burning_counter%10;
            CATODE_BCD_A = number_to_bcd(number, 0);
            CATODE_BCD_B = number_to_bcd(number, 1);
            CATODE_BCD_C = number_to_bcd(number, 2);
            CATODE_BCD_D = number_to_bcd(number, 3);
            ThisThread::sleep_for(1000);
        }
        nixi_burning_counter += 1;
        if (nixi_burning_counter == 40) {nixi_burning_counter = 0;}
    }
}



void roll_slot_machine(uint32_t& wrapper, uint32_t& source, uint8_t max_tens, uint8_t max_ones) {

    uint32_t sleep_time = 100;

    int8_t target_tens = source / 10;
    int8_t target_ones = source - (target_tens * 10);

    int8_t current_tens = wrapper / 10;
    int8_t current_ones = wrapper - (current_tens * 10);

    
    while(true) {
        current_ones -= 1;
        if (current_ones < 0) {
            current_ones += 10;
        }

        wrapper = current_ones + current_tens * 10;

        if (current_ones == target_ones) {
            break;
        }
        ThisThread::sleep_for(sleep_time);
    }
    ThisThread::sleep_for(sleep_time);


    while(true) {
        current_tens -= 1;
        if (current_tens < 0) {
            current_tens += 10;
        }

        wrapper = current_ones + current_tens * 10;

        if (current_tens == target_tens) {
            break;
        }
        ThisThread::sleep_for(sleep_time);
    }
    ThisThread::sleep_for(sleep_time);
}


void slotMachineThreadFnc() {
    while(true) {

        if (run_slot_machine) {    

            roll_slot_machine(rtc_time_sm_wrapper.minutes, rtc_time.minutes, 5, 9);
            roll_slot_machine(rtc_time_sm_wrapper.hours, rtc_time.hours, 2, 3);
            run_slot_machine = false;
        
        } 
        else {
        
            rtc_time_sm_wrapper = rtc_time;
        
        }
        ThisThread::sleep_for(100);
    }
}


int main() {
    

    printf("Starting\n");

    init_rtc_time(rtc_time);
    init_input_buttons();

    Thread rtcThread;
    Thread updateTimeThread;
    Thread inputButtonsThread;
    Thread secondTogglerThread;
    Thread nixiThread;
    Thread slotMachineThread;

    starting_sequence();

    rtcThread.start(rtcThreadFnc);
    updateTimeThread.start(updateTimeThreadFnc);
    inputButtonsThread.start(inputButtonsThreadFnc);
    secondTogglerThread.start(secondTogglerThreadFnc);
    nixiThread.start(nixiThreadFnc);
    slotMachineThread.start(slotMachineThreadFnc);

    while (true) {
        //printf("%d %d %d %d\n", rtc_time.hours, rtc_time.minutes, rtc_time.seconds, run_slot_machine);
        ThisThread::sleep_for(1000);
    }
}



