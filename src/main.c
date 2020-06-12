/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "commands.h"

#define HISTORY_FILE ".bthistory"

char* stripline( char *str ) {
  char *start, *end;

  for ( start = str; *start && isblank(*start) ; start++ ) ;

  for ( end = start + strlen(start) -1;
        end > start && isblank(*end);
        end-- ) {

    *end = '\0';
  }

return start;
}

#define TOKENIZEARGMAX 80
char* tokenizestr( char *data, char *argv[TOKENIZEARGMAX], int *argc ) {

  int quotes = 0;
  char *line = NULL;
  int i;

  if ( !data || !data[0] || !argv || !argc ) return 0;

// initialize argv list
  for ( i = 0 ; i < TOKENIZEARGMAX ; i++ ) argv[i] = 0;
  *argc = 0;

// skip white chars at beginning
  for ( line = data ; *line && *line <= ' ' ; line++ ) ;
  if ( !(*line) ) return 0;

  for ( i = 1, argv[0] = line ; *line ; line++ ) {

    // end of line?
    if ( *line == '\n' || *line == '\r' ) {
      *line++ = '\0';
      break;
    }

    if ( *line == '\"' ) {
      quotes++;
      continue;
    }

    if ( !(quotes&1) && *line <= ' ' && i < TOKENIZEARGMAX ) {
      *line++ = '\0';

      // skip white chars between args
      while ( *line && *line <= ' ' )
        if ( *line == '\n' || *line == '\r' )
          break;
        else
          line++;

      if ( !(*line) ) break;
      if ( *line == '\n' || *line == '\r' ) {
        *line++ = '\0';
        break;
      }

      argv[i++] = line;

      quotes = 0;
      if ( *line == '\"' ) quotes++;
    }
  }

  *argc = i;

return line;
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *command_generator(
    const char *text,
    int state
    ) {

  static int list_index, len;
  char *name;

  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  /* Return the next name which partially matches from the command list. */
  while( (name = commands[list_index].name) ) {
      list_index++;

      if (strncmp(name, text, len) == 0)
        return strdup(name);
  }

  /* If no names matched, then return NULL. */
return ((char *)NULL);
}

/* Attempt to complete on the contents of TEXT.  START and END bound the
   region of rl_line_buffer that contains the word to complete.  TEXT is
   the word to complete.  We can use the entire contents of rl_line_buffer
   in case we want to do some simple parsing.  Return the array of matches,
   or NULL if there aren't any. */
char **command_completion(
    const char *text, int start, int end
    ) {

return rl_completion_matches(text, command_generator);
}

command_t* command_search( char *name ) {

  int i;

  for (i = 0; commands[i].name; i++) {

    if ( strcmp(name, commands[i].name) == 0 ) {
      return &commands[i];
    }
  }

  fprintf(stderr, "%s: No such command, try 'help'\n", name);

return (command_t *)NULL;
}

// Execute a command line.
int execute_line(char *line) {
  
  int argc;
  char *argv[TOKENIZEARGMAX];
  command_t *command = (command_t *)NULL;

  if ( !tokenizestr(line, argv, &argc) ) return -1;

  command = command_search(argv[0]);
  if ( !command ) return -1;

  // Call the function
  return ((*(command->cmd))(argc, argv));
}

void help( char *name ) {

  printf("\nBluetooth Exposure Notification beacons manipulation TOOL\n\n");
  printf("Usage:\n\t%s [command] [command parameters]\n", name);

}

int main(int argc, char *argv[]) {
  char *line, *s;

  // execute single command
  if ( argc > 1 ) {
    command_t *command = command_search(argv[1]);
    if ( !command ) {
      help(argv[0]);
      return 1;
    }

    return ((*(command->cmd))(argc-1, argv+1));
  }

  // initialize readline
  rl_readline_name = argv[0];
  rl_attempted_completion_function = command_completion;

  read_history(HISTORY_FILE);

  while ( 1 ) {
 
    if ( !(line = readline("> ")) ) {

      char *quit_args[2] = {"quit", "" };
      cmd_quit(1, quit_args );

      break;
    }

    if ( (s = stripline(line)) ) {
      add_history(s);
      append_history(1, HISTORY_FILE);

      execute_line(s);
    }

    free(line);
  }

return 0;
}

