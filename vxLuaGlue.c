/**
 * This module is the glue layer between the software running on vxWorks and
 * the Lua scripting engine. 
 * All functions in the symbol table of vxWorks are available in lua scripts
 * via the "vxDo" command. To execute a function from a lua script 
 * works like this:
 * result = vxDo ("my_c_function", 1, 2, 5)
 * The function my_c_function will be called with 1, 2 and 5 as parameter.
 * The retun value will be converted as a number. 
 * This number can be used as an int, double or pointer. See the test lua script
 * how to use it. 
 *
 * For C++ function, the exact manging function name is required. 
 * There is an enhancement to do symbol partial match in order to call C++
 * manging functions if the function is static and name match is unique. 
 * For example - a target system has following C++ functions: 
 *   _Z10runTestAllv --> runTestAll()
 *   _Z7runTesti     --> runTest(int)
 *   vxDo ("runTestAll")
 *      will be OK since the "runTestAll" is part of "_Z10runTestAllv"
 *      and the "_Z10runTestAllv" is unique in the symbol table.
 *   vxDo ("runTest",0)
 *      will fail since there are many symbols in the system having "runTest" 
 *   vxDo ("_Z7runTesti",0)
 *      wil be Ok since the manging symbol is unique in the system
 *
 * The advantage of this approach is that no wrapper functions need to be 
 * created for all C functions and they do not need to be registered within 
 * the lua space. This can save allot code and memory space when you have many
 * C functions. 
 *
 * Since version 1.2 are global variables supported. These can be accessed via
 * the vxGet and vxSet lua functions. 
 * To set a variable in C do vxSet("varname", 123) in lua.
 * To get a variable value do value = vxGet("varname") in lua.
 * 
 * How to use it, at least how I did it:
 *  Get Lua 5.0.2 (the only one I tested) from http://www.lua.org .
 *  Copy the Source files and header files in your own vxWorks project.
 *  I got rid of the directory structure.
 *  Add the next line to the top of lua.h 
 *  #include <vxWorks.h>
 *  Add the next line to the top of ldo.c
 *  #include <time.h>   
 *  time.h was needed to avoid some strange compiler warnings. Don't ask...
 *  Copy vxLuaGlue.c and vxLuaGlue.h to the same directory. 
 *  Add the new sources to your vxworks 'make' file and compile your project. 
 *  
 *  Get Lua 5.2.1 (the one I tested) from http://www.lua.org .
 *  Copy the Source files and header files in your own vxWorks project.
 *  I got rid of the directory structure.
 *  Add the next line to the top of lua.h 
 *       #include <vxWorks.h>
 *
 *  Add the next line to the top of ldo.c / lua.c 
 *       #include <time.h>   
 *  time.h was needed to avoid some strange compiler warnings. Don't ask...
 *
 *  Add the next line in lctype.h after #include "lua.h"
 *       #ifdef _WRS_VXWORKS_MAJOR
 *       #define LUA_USE_CTYPE	1
 *       #endif
 *  this is required to avoid compile error.
 * 
 *  Copy vxLuaGlue.c and vxLuaGlue.h to the same directory. 
 *  Add the new sources to your vxworks 'make' file and compile your project. 
 *
 *  Somewhere in your application call 'TSysStartLua()'. A good place is during
 *  initialisation of your application.
 *  Then, when you want a lua script to be executed add 
 *  'TSysRunLuaScript("/c0/script.lua")' to your source code. Change the
 *  path with your own script location. You can also call this from the
 *  vxWorks target shell to execute a script. 
 *  Call TSysStopLua() to unload lua when you do not need it anymore and
 *  want to free it's allocated memory.
 * 
 * Limits:
 *  - Only strings, boolean and integer numbers are supported. (More in next version?)
 *  - The number of parameters is limited to 15. (Can be extended, see code)
 *  - function name length maximum of 127 chars. (Can be extended, see code)
 *  - Only tested on Lua 5.0.2, VxWorks 5.3 for X86 platforms 
 *    (HP-UX compile environment).
 *  - Tested on Lua 5.1.3, VxWorks 5.5.1 for MIPS
 *  - Tested on Lua 5.2.1, VxWorks 6.1 for PPC/vxsim and VxWorks 6.8.1 for vxsim
 *
 * Technology: It uses the same technology that the target shell uses.
 *
 * Note:       Although this vxWorks to Lua glue layer almost certainly 
 *             contains bugs it might be useful to you. 
 * 
 * Author: Dennis Kuppens <d.kuppens@zonnet.nl>
 * Date  : Friday the 13th Jan 2006 (no kidding)
 *
 * Modified by:
 *         Rogerz Zhang <rogerz.zhang@gmail.com>
 *         Bin Li <bin_li71@yahoo.com>
 *
 * License: Public Domain.
 *          This means you can do whatever you want to do with it. 
 *          In return I am not responsible in any way for this source code. 
 *
 * Version : 1.4	26 Oct 2012		binl
 * 			 Modified a few lines for Lua 5.2.1
 * 			 Added symbol partial match to call C++ functions
 *                       The symbol partial search can be disabled by removing 
 *                         #define INCLUDE_SYM_PART_MATCH
 *
 * Version : 1.3	11 Jun 2008		rogerz
 * 			 Modified a few lines for Lua 5.1.3
 * 			 Fixed memcpy bug in Sample LUA Script
 *
 * Version : 1.2    17 Feb 2006
 *           Added functions to access global variables. 
 *
 * Version : 1.1    13 Feb 2006
 *           Added return value for the vxDo function.
 *           
 * Version : 1.0    13 Jan 2006 
 *           Initial version.
 *
 * Credits:
 * - Thanks to Robert Geer (Creator of bgsh, a VxWorks Shell), for inspiring 
 *   me.
 *
 * Sample LUA Script (save it as script.lua):

    -- Sample Lua script
    -- print "Hello world from script.lua"
	print "Starting tests..."
	vxDo("LuaStringTest","Hello You!")
	print ""
	vxDo("LuaDecimalTest", 25)
	print ""
	vxDo("LuaStringDecimalTest", "This is a string.", 91)
	print ""
	vxDo("LuaDecimalStringTest", 65, "Another string.")
	print ""
	vxDo("printf", "Displaying a number from printf: %d", 300)
	print ""
	pointer = vxDo("malloc", 100)
	vxDo("memcpy", pointer, "Just a string.", 15)
	vxDo("printf", "Pointer test: copied string: %s", pointer)
	print ""
	vxDo("free", pointer)
	print("The value of LuaTestVariable is "..vxGet("LuaTestVariable"))
	vxSet("LuaTestVariable", 321)
	print("The value of LuaTestVariable is now "..vxGet("LuaTestVariable"))
	print "Tests ended."
 *
 */

#include <vxWorks.h>
#include <symLib.h>
#include <sysSymTbl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "a_out.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* On our system, all functions get a leading underscore. 
   Some other systems (ELF) do not get this leading underscore.
   Uncomment the #define below if you do not need the leading underscore.
*/
#undef LEADING_UNDERSCORE 

/* To show verbose info */
#undef VERBOSE


#if (502==LUA_VERSION_NUM)
#define LUA_5_2
#elif (501==LUA_VERSION_NUM)
#define  LUA_5_1
#else
#endif

#undef INCLUDE_SYM_PART_MATCH
#define INCLUDE_SYM_PART_MATCH

#ifdef INCLUDE_SYM_PART_MATCH
LOCAL int      matchCnt = 0;
LOCAL SYMBOL * matchSym  = NULL;

LOCAL BOOL symSysTblFindPartMatch
    (
    char *	name,		/* symbol name */
    int		val,		/* symbol value */
    INT8	type,		/* symbol type */
    char *	substr,		/* substring to match */
    UINT16      group,		/* symbol's module group */
    SYMBOL *    pSymbol		/* symbol structure */
    )
{
   int         subStrLen = 0;
   char *      symName = name;
   int         ntries	= 0;

   if ( NULL == symName || NULL == substr )
   {
       return FALSE;
   }

   subStrLen = strlen (substr);
   ntries    = strlen (symName) - subStrLen;

   for (; ntries >= 0; symName++, --ntries)
   {
       if (strncmp (symName, substr, subStrLen) == 0)
       {
          matchCnt++;	/* we've found a match */
          matchSym = pSymbol; 
          break;
       }
   }

   return (TRUE);
}

STATUS symFindByName_4cppMangingName(SYMTAB_ID symTblId, char * name, char ** pValue, SYM_TYPE * pType)
{
   matchCnt = 0;
   matchSym = NULL;
   symEach (symTblId, (FUNCPTR) symSysTblFindPartMatch, (int) name);

   if ( 1 != matchCnt)
   {
#ifdef VERBOSE       
      printf("Failed - Symbol [%s]: %s\n", name, (0==matchCnt)?"not found":"not unique");
#endif
      return ERROR; 
   }

   if (pValue != NULL)
        *pValue = (char *) matchSym->value;
    
   if (pType != NULL)
        *pType = matchSym->type;

   return OK;
}
#endif

typedef int32_t bool_t;

typedef int (*wrap_func)(lua_State*);

/* Structure for command table */
typedef struct {
  const char *name;
  wrap_func wrapper;
} lua_command_info;

/* Forward declares */
int l_myLuaFunction(lua_State* luaVM);
int l_ExecuteLuaCommand(lua_State* luaVM);
int l_vxSet(lua_State* luaVM);
int l_vxGet(lua_State* luaVM);
/**
 * Function table that will be registered within Lua. 
 */
static lua_command_info lua_commands[] = 
{
    { "myLuaFunction",          l_myLuaFunction         },
    { "vxDo",                   l_ExecuteLuaCommand     },
    { "vxSet",                  l_vxSet                 },
    { "vxGet",                  l_vxGet                 },
    {0,0}
};

/* Lua state  */
static lua_State *luaVM=NULL;

/**
 * Executes a function called from lua. 
 */
int l_ExecuteLuaCommand(lua_State* luaVM)
{
    int i;
    FUNCPTR function_address=NULL;
    int function_status;
    unsigned int arg[15];  
    char*    symbol_value;
    SYM_TYPE symbol_type;
    int nArgs;
    char symbol_name[128]="\"no function\"";
    const char * symName=NULL;

    /* Clear argument list */
    for (i = 0; i < 15; i++)
    {
        arg[i] = 0 ;    
    }
    
    nArgs = lua_gettop(luaVM);    
    
    if (nArgs>15) nArgs = 15;   /* Clip to max 15 args */
    
    /* First argument is always the function name */
    if(nArgs>=1)
    {
        if(lua_isstring(luaVM, 1))
        {
            symName = lua_tostring(luaVM, 1);
        }
    }
    
#ifdef VERBOSE       
    printf("Function name : %s\n", lua_tostring(luaVM, 1));
    printf("Number of args: %d\n", nArgs);
#endif
    
    /* Process the function arguments */
    for(i=2; i<=nArgs; i++)
    {
#ifdef VERBOSE        
        printf("arg %d: %s\n", i, lua_tostring(luaVM, i));
#endif
        if(lua_isnil(luaVM, i))
        {
            arg[i-2] = (int)NULL;
#ifdef VERBOSE        
            printf("Type is NULL\n");
#endif
        }
        else if(lua_isboolean(luaVM, i))
        {
            arg[i-2] = (bool_t) lua_toboolean(luaVM, i);
#ifdef VERBOSE        
            printf("Type is boolean\n");
#endif
        }
        else if(lua_isnumber(luaVM, i))
        {
            arg[i-2] = (int32_t) lua_tonumber(luaVM, i);
#ifdef VERBOSE        
            printf("Type is number\n");
#endif
        }
        else if(lua_isstring(luaVM, i))
        {
            arg[i-2] = (unsigned int) lua_tostring(luaVM, i);
#ifdef VERBOSE        
            printf("Type is string\n");
#endif
        }
        else if(lua_istable(luaVM, i))
        {
            /* Not supported yet */
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is table\n");
#endif
        }
        else if(lua_isfunction(luaVM, i))
        {
            /* Not supported yet */
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is function\n");
#endif
        }
        else if(lua_iscfunction(luaVM, i))
        {
            /* Not supported yet */
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is cfunction\n");
#endif
        }
        else if(lua_isuserdata(luaVM, i))
        {
            /* Not supported yet */
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is userdata\n");
#endif
        }
        else if(lua_islightuserdata(luaVM, i))
        {
            /* Not supported yet */
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is lightuserdata\n");
#endif
        }
        else 
        {
            arg[i-2] = 0;
#ifdef VERBOSE        
            printf("Type is not valid!\n");
#endif
        }            
    }
  
/* 
    On our system, all functions get a leading underscore. 
    Some other systems (ELF) do not get this leading underscore.
*/
#ifdef LEADING_UNDERSCORE    
    if(symName)
    {
        strcpy( symbol_name, "_" ) ;
        strcat( symbol_name, symName ) ;
    }
#else
    if(symName)
    {
        strcpy( symbol_name, symName) ;
    }
#endif
    
    if ( OK != symFindByName( sysSymTbl,
    	      symbol_name,
    	      &symbol_value,
    	      &symbol_type ) )
    {
#ifdef INCLUDE_SYM_PART_MATCH
        if ( OK != symFindByName_4cppMangingName( sysSymTbl,
    	           symbol_name,
    	           &symbol_value,
    	           &symbol_type ) )
        {
           printf("Error: Function %s does not exist or not unique!\n", symbol_name);
           return 0;
        }
#else
        printf("Error: Function %s does not exist!\n", symbol_name);   
        return 0;
#endif
    }

    function_address = (FUNCPTR) symbol_value;
    function_status = (*function_address)( arg[0],
					   arg[1],
					   arg[2],
					   arg[3],
					   arg[4],
					   arg[5],
					   arg[6],
					   arg[7],
					   arg[8],
					   arg[9],
					   arg[10],
					   arg[11],
					   arg[12],
					   arg[13],
					   arg[14] ) ;       
    lua_pushnumber(luaVM, function_status);
    return 1;   /* One value pushed onto the lua stack */	

}


/**
 * Gets the value of a global variable.
 */
int l_vxGet(lua_State* luaVM)
{
    char*    symbol_value;
    SYM_TYPE symbol_type;
    int nArgs;
    char symbol_name[128]="\"no global variable.\"";
    const char * symName=NULL;
    int  error = 1;
    
    nArgs = lua_gettop(luaVM);    
    
    /* First argument is always the function name */
    if(nArgs>=1)
    {
        if(lua_isstring(luaVM, 1))
        {
            symName = lua_tostring(luaVM, 1);
            error = 0;
        }
    }
    
#ifdef VERBOSE       
    printf("Function name : %s\n", lua_tostring(luaVM, 1));
    printf("Number of args: %d\n", nArgs);
#endif
   
   if(!error)
   { 
        if(symName)
        {
            strcpy( symbol_name, symName) ;
        }
        if ( OK == symFindByName( sysSymTbl,
        	      symbol_name,
        	      &symbol_value,
        	      &symbol_type ) )
        {
            lua_pushnumber(luaVM, *((unsigned int*)symbol_value));
            return 1;   /* One value pushed onto the lua stack */	
        }
        else    
        {   /* Try again with underscore */
            /* Both symbols with and without underscore are possible because 
               type N_BSS does not need an underscore but a N_DATA does needs an
               underscore.
            */
            strcpy( symbol_name, "_" ) ;
            strcat( symbol_name, symName ) ;
            if ( OK == symFindByName( sysSymTbl,
            	      symbol_name,
            	      &symbol_value,
            	      &symbol_type ) )
            {
                lua_pushnumber(luaVM, *((unsigned int*)symbol_value));
                return 1;   /* One value pushed onto the lua stack */	
            }
            else
            {            
                printf("Error: Symbol %s does not exist!\n", symbol_name);   
                return 0;
            }
        }
    }
    return 0;
}


/**
 * Gets the value of a global variable.
 */
int l_vxSet(lua_State* luaVM)
{
    char*    symbol_value;
    SYM_TYPE symbol_type;
    int nArgs;
    char symbol_name[128]="\"no global variable.\"";
    const char * symName=NULL;
    int  error = 1;
    
    nArgs = lua_gettop(luaVM);    
    
    /* First argument is always the function name */
    if(nArgs>=1)
    {
        if(lua_isstring(luaVM, 1))
        {
            symName = lua_tostring(luaVM, 1);
            error = 0;
        }
    }
    if(nArgs<2) error = 1;
    
#ifdef VERBOSE       
    printf("Function name : %s\n", lua_tostring(luaVM, 1));
    printf("Number of args: %d\n", nArgs);
#endif
   
   if(!error)
   { 
        if(symName)
        {
            strcpy( symbol_name, symName) ;
        }
        if ( OK == symFindByName( sysSymTbl,
        	      symbol_name,
        	      &symbol_value,
        	      &symbol_type ) )
        {
            ((unsigned int*)symbol_value)[0]=lua_tonumber(luaVM, 2);                        
            return 0;   /* One value pushed onto the lua stack */	
        }
        else
        {   /* Try again with underscore */
            /* Both symbols with and without underscore are possible because 
               type N_BSS does not need an underscore but a N_DATA does needs an
               underscore.
            */
            strcpy( symbol_name, "_" ) ;
            strcat( symbol_name, symName ) ;
            if ( OK == symFindByName( sysSymTbl,
            	      symbol_name,
            	      &symbol_value,
            	      &symbol_type ) )
            {
                ((unsigned int*)symbol_value)[0]=lua_tonumber(luaVM, 2);                        
                return 0;   /* One value pushed onto the lua stack */	
            }
            else
            {
                printf("Error: Symbol %s does not exist!\n", symbol_name);   
                return 0;
            }
        }
    }
    return 0;
}


/**
 * You can add c functions to Lua like this.
 */
int l_myLuaFunction(lua_State* luaVM)
{
    return 0;   
}

/**
 * Starts Lua and opens lua libraries.
 */
void TSysStartLua()
{
    int i;

#if defined(LUA_5_2)
    luaVM = luaL_newstate();  /* Open Lua */
    luaL_openlibs(luaVM);
#elif defined(LUA_5_1)
    luaVM = lua_open();       /* Open Lua */
    luaL_openlibs(luaVM);
#else
    luaopen_base(luaVM);     
    luaopen_table(luaVM);    
    luaopen_io(luaVM);       
    luaopen_string(luaVM);   
    luaopen_math(luaVM);     
#endif
    if (NULL == luaVM)
    {
    	printf("Error Initializing lua\n");
        return;
    }
    
    /* Register all commands */
    for (i = 0; lua_commands[i].name; i++)
    {
        lua_register(luaVM,lua_commands[i].name,lua_commands[i].wrapper);
    }    
}

/**
 * Stops lua.
 */
void TSysStopLua()
{
    if(luaVM!=NULL) lua_close(luaVM); /* Close Lua */	    
}

/**
 * Runs a lua script.
 */
void TSysRunLuaScript(char * luaScriptPath)
{
    if(luaVM!=NULL) 
    {
#if defined(LUA_5_2)
       printf("TSysRunLuaScript(): run %s\n", luaScriptPath);
       if ( ! ( luaL_dofile(luaVM, luaScriptPath) ) )    
          printf("TSysRunLuaScript(): done");

#elif defined(LUA_5_1)
       luaL_dofile(luaVM, luaScriptPath);    
#else
       lua_dofile(luaVM, luaScriptPath);
#endif
    }
    else
        printf("TSysRunLuaScript(): Lua not initialized!\n");
}

/****************************************************************************** 
 * Test functions. 
 *****************************************************************************/
void LuaStringTest(char * string)
{
    printf("string: \"%s\"", string);    
}

void LuaDecimalTest(int value)
{
    printf("intval: %d", value);
}

void LuaStringDecimalTest(char * string, int value)
{
    printf("string: \"%s\" intval: %d", string, value);
}

void LuaDecimalStringTest(int value, char * string)
{
    printf("intval: %d string: \"%s\"", value, string);
}

int LuaTestVariable=0;
