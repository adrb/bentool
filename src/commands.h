/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

typedef int (tf_command) ( int, char** );

typedef struct {
  tf_command *cmd;      // Function to call to do the job
  char *name;           // User printable name of the function
  char *desc;           // Short description for this function
} command_t;

int cmd_quit( int argc, char **argv);   // exported for main()

extern command_t commands[];  // defined at end of the commands.c file

#endif // __COMMANDS_H__
