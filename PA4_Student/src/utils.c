#include "utils.h"

void _removeOutputDir() {
  pid_t pid = fork();
  if (pid == 0) {
    char *argv[] = {"rm", "-rf", "output", NULL};
    if (execvp(*argv, argv) < 0) {
      printf("ERROR: exec failed\n");
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  } else {
    wait(NULL);
  }
}

void _createOutputDir() { mkdir("output", ACCESSPERMS); }

void bookeepingCode() {
  _removeOutputDir();
  sleep(1);
  _createOutputDir();
}

// --- BONUS: Caesar shift-right-by-2 cipher ---
// Used to lightly obfuscate the query sent by ENC_SEARCH_ITEM.
// Shifts letters only; non-letters are passthrough.
// Wraps at the end of the alphabet: 'y'->'a', 'z'->'b', 'Y'->'A', 'Z'->'B'.
void encrypt_str(char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') {
      s[i] = (c - 'a' + 2) % 26 + 'a';
    } else if (c >= 'A' && c <= 'Z') {
      s[i] = (c - 'A' + 2) % 26 + 'A';
    }
  }
}

void decrypt_str(char *s) {
  for (size_t i = 0; s[i] != '\0'; i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') {
      s[i] = (c - 'a' - 2 + 26) % 26 + 'a';
    } else if (c >= 'A' && c <= 'Z') {
      s[i] = (c - 'A' - 2 + 26) % 26 + 'A';
    }
  }
}
