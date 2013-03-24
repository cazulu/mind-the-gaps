#ifndef CONFIG_H_
#define CONFIG_H_

#include "hardware_board.h"

//REMEMBER: ACLK is 32.768KHz
#define TIME_STACK		66		// 2ms
#define	TIME_BLINK		16383	// 500ms

void InitializeClocks(void);
void InitializeTimers(void);
void InitializeEthSpi(void);


#endif /*CONFIG_H_*/
