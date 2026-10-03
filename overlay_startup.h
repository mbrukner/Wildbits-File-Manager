/*
 * overlay_startup.h
 *
 *  Created on: Mar 11, 2024
 *      Author: micahbly
 */

#ifndef OVERLAY_STARTUP_H_
#define OVERLAY_STARTUP_H_

/* about this class
 *
 *  Routines for starting up Wildbits File Manager, including show splash screen(s)
 *    Some code here originated in sys.c and other places before being moved here
 *
 */

/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

#include "app.h"
#include "text.h"
#include <stdint.h>


/*****************************************************************************/
/*                            Macro Definitions                              */
/*****************************************************************************/


/*****************************************************************************/
/*                               Enumerations                                */
/*****************************************************************************/

/*****************************************************************************/
/*                                 Structs                                   */
/*****************************************************************************/


/*****************************************************************************/
/*                       Public Function Prototypes                          */
/*****************************************************************************/


// load strings into memory and set up string pointers
void Startup_LoadString(void);

// Show the animated Wildbits title screen; any key skips it.
void Startup_ShowLogo(void);

// enable the random number generator, and seed it
void Startup_InitializeRandomNumGen(void);

// initialize or re-initialize the global dialog box for standard 2-button entry
// some routine may change it temporarily to 3-button format/size
void App_InitializeDialogBox(void);


#endif /* OVERLAY_STARTUP_H_ */
