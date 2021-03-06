/*
  HarwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  Modified 23 November 2006 by David A. Mellis
*/

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "serials.h"

// The UART buffers
unsigned char uart0_buffer[UART0_BUFFER_SIZE];
unsigned char uart1_buffer[UART1_BUFFER_SIZE];

static unsigned char *buffer[] = {uart0_buffer, uart1_buffer};
static uint8_t size[UART_NUM] = {UART0_BUFFER_SIZE, UART1_BUFFER_SIZE};
static uint8_t limit[UART_NUM] = {UART0_BUFFER_THLIMIT, UART1_BUFFER_THLIMIT};

static uint8_t head[UART_NUM];
static uint8_t tail[UART_NUM];
static uint8_t lines[UART_NUM];
static uint8_t bytes[UART_NUM];
uint8_t uart_intr[UART_NUM];

// NOTE this function must be called with interrupts disabled
void initSerials(void) {
    
//-------- UART0
	// Single speed operations
	cbi(UCSR0A, U2X0);
	// Set baud rate
	UBRR0H = (uint8_t)(UART_BAUD_CALC(UART0_BAUD_RATE,F_CPU)>>8);
	UBRR0L = (uint8_t)UART_BAUD_CALC(UART0_BAUD_RATE,F_CPU);
	// Asynchronous 8N1
	cbi(UCSR0C, UMSEL0);
	cbi(UCSR0C, UPM01);
	cbi(UCSR0C, UPM00);
	cbi(UCSR0C, USBS0);
	sbi(UCSR0C, UCSZ01);
	sbi(UCSR0C, UCSZ00);
	// Enable receiver and transmitter; enable RX interrupt
	cbi(UCSR0B, UCSZ02);
	sbi(UCSR0B, RXCIE0);
	sbi(UCSR0B, RXEN0);
	sbi(UCSR0B, TXEN0);

	memset(uart0_buffer, 0, UART0_BUFFER_SIZE);
	head[UART0] = 0;
	tail[UART0] = 0;
	lines[UART0] = 0;
	bytes[UART0] = 0;
	uart_intr[UART0] = 0;

//-------- UART1
	// Single speed operations
	cbi(UCSR1A, U2X1);
	// Set baud rate
	UBRR1H = (uint8_t)(UART_BAUD_CALC(UART1_BAUD_RATE,F_CPU)>>8);
	UBRR1L = (uint8_t)UART_BAUD_CALC(UART1_BAUD_RATE,F_CPU);
	// Asynchronous 8N1
	cbi(UCSR1C, UMSEL1);
	cbi(UCSR1C, UPM11);
	cbi(UCSR1C, UPM10);
	cbi(UCSR1C, USBS1);
	sbi(UCSR1C, UCSZ11);
	sbi(UCSR1C, UCSZ10);
	// Enable receiver and transmitter; enable RX interrupt
	cbi(UCSR1B, UCSZ12);
	sbi(UCSR1B, RXCIE1);
	sbi(UCSR1B, RXEN1);
	sbi(UCSR1B, TXEN1);

	memset(uart1_buffer, 0, UART1_BUFFER_SIZE);
	head[UART1] = 0;
	tail[UART1] = 0;
	lines[UART1] = 0;
	bytes[UART1] = 0;
	uart_intr[UART1] = 0;
}

uint8_t available(uart_port_t port) {
	return (size[port]+head[port]-tail[port])%size[port];
}

char look(uart_port_t port) {
	// if the head isn't ahead of the tail, we don't have any characters
	if (head[port] == tail[port]) {
		return -1;
	} else {
		return buffer[port][tail[port]];
	}
}

char read(uart_port_t port) {
	char byte;
	
	// if the head isn't ahead of the tail, we don't have any characters
	if (head[port] == tail[port]) {
		return -1;
	} else {
		byte = buffer[port][tail[port]];
		tail[port] = (tail[port]+1)%size[port];
		
		if (byte==LINE_TERMINATOR )
			lines[port]--;
		
		bytes[port]--;
		return byte;
	}
}

//NOTE: this method could return a NON 0-terminated buffer if size
//	is less than the present buffer-line
int readLine(uart_port_t port, char *buff, unsigned short len) {
	unsigned short i = 0;
	
	// if the head isn't ahead of the tail, we don't have any characters
	// OR
	// if we have not yet received a LINE_TERMINATOR, we don't have any complete line
	if (!lines[port]) {
		buff[0] = 0;
		return -1;
	} else {
		len--; // Reserve space for NULL termiator
		do {
			buff[i] = buffer[port][tail[port]];
			tail[port] = (tail[port]+1)%size[port];
		} while ( buff[i]!=LINE_TERMINATOR && (++i<len) );
		
		// Check if we returned a line
		if ( i<len ) {
			// A complete line is in the buffer
			lines[port]--;
			// Moving pointer to NULL terminator position
			i++;
		}
		buff[i] = 0; // Adding terminator to buffer
	}
	return i;
}

void flush(uart_port_t port) {
	// don't reverse this or there may be problems if the RX interrupt
	// occurs after reading the value of rx_buffer_head but before writing
	// the value to rx_buffer_tail; the previous value of rx_buffer_head
	// may be written to rx_buffer_tail, making it appear as if the buffer
	// were full, not empty.
	head[port] = tail[port];
	lines[port] = 0;
}

void print(uart_port_t port, char c) {
    
	if (port == UART0) {
		// wait until UDR ready
		while(!(UCSR0A & (unsigned char)_BV(UDRE0)));
		// Send character
		UDR0 = c;
	} else {
		// wait until UDR ready
		while(!(UCSR1A & (unsigned char)_BV(UDRE1)));
		// Send character
		UDR1 = c;
	}

}

void printStr(uart_port_t port, const char *str) {
	while (*str)
		print(port, *str++);
}

void printLine(uart_port_t port, const char *str) {
	while (*str)
		print(port, *str++);
	print(port, '\n');
	print(port, '\r');
	
}

//----- Interrupt handlers

/// UART bottom-halve interrupt handler
inline void rxByte(uart_port_t port, unsigned char c ) {
	uint8_t i = (head[port]+1)%size[port];

	// if we should be storing the received character into the location
	// just before the tail (meaning that the head would advance to the
	// current location of the tail), we're about to overflow the buffer
	// and so we don't write the character or advance the head.
	if (i != tail[port]) {
// sbi(PORTA, PA2);
		buffer[port][head[port]] = c;
		head[port] = i;
		bytes[port]++;
		// Look if we are at end-of-line to schedule the top-halve handler
		if ( c == LINE_TERMINATOR ) {
// sbi(PORTA, PA3);
			lines[port]++;
			// Scheduling top-halves only when we have a complete line
			// in the buffer
			scheduleTopHalve(port);
		}
		// Safety schedule the top-halve if the buffer is filled for more than limit value
		if ( bytes[port] > limit[port] ) {
			scheduleTopHalve(port);
		}
	} else {
		scheduleTopHalve(port);
	}
}

SIGNAL (SIG_UART0_RECV) { // UART0 RX interrupt
// sbi(PORTA, PA1);
	rxByte(UART0, UDR0);
}

SIGNAL (SIG_UART1_RECV) { // UART1 RX interrupt
	rxByte(UART1, UDR1);
}
