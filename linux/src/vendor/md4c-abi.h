/* Minimal declarations from MD4C 0.5.2, https://github.com/mity/md4c.
 * Copyright (c) 2016-2024 Martin Mitáš
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef NOTEMD_MD4C_ABI_H
#define NOTEMD_MD4C_ABI_H
typedef unsigned MD_SIZE;
typedef unsigned MD_OFFSET;
typedef enum { MD_BLOCK_DOC, MD_BLOCK_QUOTE, MD_BLOCK_UL, MD_BLOCK_OL,
    MD_BLOCK_LI, MD_BLOCK_HR, MD_BLOCK_H, MD_BLOCK_CODE, MD_BLOCK_HTML,
    MD_BLOCK_P, MD_BLOCK_TABLE, MD_BLOCK_THEAD, MD_BLOCK_TBODY, MD_BLOCK_TR,
    MD_BLOCK_TH, MD_BLOCK_TD } MD_BLOCKTYPE;
typedef enum { MD_SPAN_EM, MD_SPAN_STRONG, MD_SPAN_A, MD_SPAN_IMG, MD_SPAN_CODE,
    MD_SPAN_DEL, MD_SPAN_LATEXMATH, MD_SPAN_LATEXMATH_DISPLAY, MD_SPAN_WIKILINK,
    MD_SPAN_U } MD_SPANTYPE;
typedef enum { MD_TEXT_NORMAL, MD_TEXT_NULLCHAR, MD_TEXT_BR, MD_TEXT_SOFTBR,
    MD_TEXT_ENTITY, MD_TEXT_CODE, MD_TEXT_HTML, MD_TEXT_LATEXMATH } MD_TEXTTYPE;
typedef enum { MD_ALIGN_DEFAULT, MD_ALIGN_LEFT, MD_ALIGN_CENTER, MD_ALIGN_RIGHT } MD_ALIGN;
typedef struct { const char *text; MD_SIZE size; const MD_TEXTTYPE *substr_types;
    const MD_OFFSET *substr_offsets; } MD_ATTRIBUTE;
typedef struct { int is_tight; char mark; } MD_BLOCK_UL_DETAIL;
typedef struct { unsigned start; int is_tight; char mark_delimiter; } MD_BLOCK_OL_DETAIL;
typedef struct { int is_task; char task_mark; MD_OFFSET task_mark_offset; } MD_BLOCK_LI_DETAIL;
typedef struct { unsigned level; } MD_BLOCK_H_DETAIL;
typedef struct { MD_ATTRIBUTE info, lang; char fence_char; } MD_BLOCK_CODE_DETAIL;
typedef struct { unsigned col_count, head_row_count, body_row_count; } MD_BLOCK_TABLE_DETAIL;
typedef struct { MD_ALIGN align; } MD_BLOCK_TD_DETAIL;
typedef struct { MD_ATTRIBUTE href, title; int is_autolink; } MD_SPAN_A_DETAIL;
typedef struct { MD_ATTRIBUTE src, title; } MD_SPAN_IMG_DETAIL;
typedef struct { MD_ATTRIBUTE target; } MD_SPAN_WIKILINK_DETAIL;
#define MD_FLAG_NOHTMLBLOCKS 0x0020
#define MD_FLAG_WIKILINKS 0x2000
#define MD_DIALECT_GITHUB (0x0004 | 0x0008 | 0x0100 | 0x0200 | 0x0400 | 0x0800)
typedef struct {
    unsigned abi_version, flags;
    int (*enter_block)(MD_BLOCKTYPE, void *, void *);
    int (*leave_block)(MD_BLOCKTYPE, void *, void *);
    int (*enter_span)(MD_SPANTYPE, void *, void *);
    int (*leave_span)(MD_SPANTYPE, void *, void *);
    int (*text)(MD_TEXTTYPE, const char *, MD_SIZE, void *);
    void (*debug_log)(const char *, void *);
    void (*syntax)(void);
} MD_PARSER;
#endif
