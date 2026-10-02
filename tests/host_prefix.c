#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define FILE_MAX_PATHNAME_SIZE 255
#define FILE_MAX_EXTENSION_SIZE 8
#define MAX_SEARCH_PHRASE_LEN 32
#define General_ToLower tolower
#define COMM_BUFFER_NUM_COLS 78
#define COMM_BUFFER_NUM_ROWS 3
#define STORAGE_FILE_BUFFER_1 file_buffer
#define STORAGE_FILE_BUFFER_1_LEN 256
#define ID_STR_ERROR_GENERIC_DISK 0
#define SCREEN_NUM_ROWS 60
#define SCREEN_NUM_COLS 80
#define CH_ESC 27
#define CH_RUNSTOP 3
#define CH_ENTER 13
#define CH_CURS_LEFT 2
#define CH_CURS_RIGHT 6
#define CH_CURS_UP 16
#define CH_CURS_DOWN 14
#define CH_BKSP 8
#define CH_DEL 127
#define CH_SPACE 32
#define CH_LINE_BREAK 10
#define CH_LINE_RETURN 13
#define COLOR_BRIGHT_WHITE 15
#define COLOR_BLACK 0
#define PARAM_USE_OVERWRITE_MODE true
#define LOG_ERR(x)
static uint8_t file_buffer[256];
static char rows[3][79];
static char *row[3] = {rows[0], rows[1], rows[2]};
static char **comm_row_ptr[3] = {&row[0], &row[1], &row[2]};
static int shown, hidden, refreshes;
static char drawn[8192];
static bool cursor_visible;
static const unsigned char *keys;
static size_t key_index;
static void Buffer_RefreshDisplay(void) { ++refreshes; }
static void App_ShowProgressBar(void) { ++shown; }
static void App_HideProgressBar(void) { ++hidden; }
static void App_UpdateProgressBar(uint8_t percent) { assert(percent <= 100); }
static char *General_GetString(int id) { return "Disk error"; }
static void Sys_EnableTextModeCursor(bool visible) { cursor_visible = visible; }
static void Text_FillBox(int a,int b,int c,int d,int e,int f,int g) {}
static void Text_SetXY(int x, int y) {}
static void Text_DrawStringAtXY(int x,int y,char *s,int a,int b) { assert(strlen(drawn)+strlen(s)<sizeof(drawn)); strcat(drawn,s); }
static uint8_t Keyboard_GetChar(void) { assert(key_index < strlen((const char *)keys)); return keys[key_index++]; }
typedef struct WB2KList { struct WB2KList *next_item_, *prev_item_; void *payload_; } WB2KList;
/* File I/O faults: source/target streams are deliberately separate handles. */
static unsigned char input[1025], output[1025];
static size_t input_size, input_pos, output_size;
static int close_count, fault;
static FILE *source_handle = (FILE *)(uintptr_t)1, *target_handle = (FILE *)(uintptr_t)2;
static FILE *test_fopen(const char *name, const char *mode) { return fault==1 ? NULL : source_handle; }
static FILE *Folder_GetTargetHandleForWriting(const char *name) { return fault==2 ? NULL : target_handle; }
static size_t test_fread(void *dst, size_t size, size_t count, FILE *f) {
    assert(f==source_handle && size==1);
    if (fault==3) return 0;
    if (count>input_size-input_pos) count=input_size-input_pos;
    memcpy(dst,input+input_pos,count); input_pos+=count; return count;
}
static size_t test_fwrite(const void *src, size_t size, size_t count, FILE *f) {
    assert(f==target_handle && size==1);
    if (fault==4) return count-1;
    assert(output_size+count<=sizeof(output));
    memcpy(output+output_size,src,count); output_size+=count; return count;
}
static int test_ferror(FILE *f) { return fault==3; }
static int test_fclose(FILE *f) { ++close_count; return fault==5 && f==target_handle ? -1 : 0; }
#define fopen test_fopen
#define fread test_fread
#define fwrite test_fwrite
#define ferror test_ferror
#define fclose test_fclose
