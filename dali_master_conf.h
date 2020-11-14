#pragma once

/* in case of inverted RX path define INVERTED_RX */
//#define INVERTED_RX

#define STICT_TIMING

#define TIMEOUT_INFINITE     0xFFFFFFFF

/***********************************************************/
/* Type definitions and defines                            */
/***********************************************************/

#define MAX_BF_EDGES      18     // max 18 edges per backward frame

/* protocol timing definitions */
#define TE          (417)                   // half bit time = 417 usec
#ifdef STRICT_TIMING /* strict receive timing according to specification (+/- 10%) */
#define MIN_TE      (TE     - (TE/10))      // minimum half bit time
#define MAX_TE      (TE     + (TE/10))      // maximum half bit time
#define MIN_2TE     ((2*TE) - ((2*TE)/10))  // minimum full bit time
#define MAX_2TE     ((2*TE) + ((2*TE)/10))  // maximum full bit time
#else /* More relaxed receive timing (+/- 20%) */
#define MIN_TE      (TE     - (TE/5)) 		// minimum half bit time
#define MAX_TE      (TE     + (TE/5))  		// maximum half bit time
#define MIN_2TE     ((2*TE) - ((2*TE)/5))   // minimum full bit time
#define MAX_2TE     ((2*TE) + ((2*TE)/5))   // maximum full bit time
#endif
