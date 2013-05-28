
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     KEY_TARGET = 258,
     KEY_DEPENDS_ON = 259,
     KEY_UPDATE_SCRIPT = 260,
     KEY_END_SCRIPT = 261,
     TKN_IDENT = 262,
     TKN_CONTEXT_VAR = 263,
     TKN_STRING = 264,
     TKN_SINT8 = 265,
     TKN_UINT8 = 266,
     TKN_SINT16 = 267,
     TKN_UINT16 = 268,
     TKN_SINT32 = 269,
     TKN_UINT32 = 270,
     TKN_SINT64 = 271,
     TKN_UINT64 = 272,
     TKN_DOUBLE = 273,
     TKN_PARENTH_OPEN = 274,
     TKN_PARENTH_CLOSE = 275,
     TKN_COMMA = 276,
     TKN_EQUAL = 277,
     TKN_ERROR = 278
   };
#endif
/* Tokens.  */
#define KEY_TARGET 258
#define KEY_DEPENDS_ON 259
#define KEY_UPDATE_SCRIPT 260
#define KEY_END_SCRIPT 261
#define TKN_IDENT 262
#define TKN_CONTEXT_VAR 263
#define TKN_STRING 264
#define TKN_SINT8 265
#define TKN_UINT8 266
#define TKN_SINT16 267
#define TKN_UINT16 268
#define TKN_SINT32 269
#define TKN_UINT32 270
#define TKN_SINT64 271
#define TKN_UINT64 272
#define TKN_DOUBLE 273
#define TKN_PARENTH_OPEN 274
#define TKN_PARENTH_CLOSE 275
#define TKN_COMMA 276
#define TKN_EQUAL 277
#define TKN_ERROR 278




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 21 "resolver/scripting/simple/simple-parser.y"

    tkn_any_t       any;
    tkn_string_t    string;
    tkn_s8_t        sint8;
    tkn_u8_t        uint8;
    tkn_s16_t       sint16;
    tkn_u16_t       uint16;
    tkn_s32_t       sint32;
    tkn_u32_t       uint32;
    tkn_s64_t       sint64;
    tkn_u64_t       uint64;
    tkn_dbl_t       dbl;
    tkn_strarr_t   *strarr;
    tkn_string_t    error;
    tkn_args_t      args;
    tkn_expr_t      expr;
    tkn_value_t     value;



/* Line 1676 of yacc.c  */
#line 119 "resolver/scripting/simple/simple-parser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yy_smpl_lval;


