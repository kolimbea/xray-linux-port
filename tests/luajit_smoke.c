#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdio.h>
int main(void){ lua_State* L=luaL_newstate(); luaL_openlibs(L);
 const char* code="local s=0 for i=1,1e7 do s=s+i%7 end local ffi=require('ffi') ffi.cdef[[int abs(int);]] return s, jit.version, jit.status(), ffi.C.abs(-5), ('%s'):format(string.upper('zona'))";
 if(luaL_dostring(L,code)){ printf("ERR %s\n",lua_tostring(L,-1)); return 1; }
 printf("sum=%.0f ver=%s jit=%d abs=%d str=%s\n",lua_tonumber(L,-5),lua_tostring(L,-4),lua_toboolean(L,-3),(int)lua_tointeger(L,-2),lua_tostring(L,-1));
 int ok = lua_tonumber(L,-5)==29999997 && lua_toboolean(L,-3) && lua_tointeger(L,-2)==5; lua_close(L); return !ok; }
