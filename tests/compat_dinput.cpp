#include "include/dinput.h"
#include <SDL3/SDL.h>
#include <cstdio>
int fails=0;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); fails++; } }while(0)
int main(){
  IDirectInput8* di=nullptr; CHECK(DirectInput8Create(nullptr,0x800,IID_IDirectInput8,(void**)&di,nullptr)==DI_OK);
  LPDIRECTINPUTDEVICE8 kb=nullptr,ms=nullptr;
  CHECK(di->CreateDevice(GUID_SysKeyboard,&kb,nullptr)==DI_OK); CHECK(di->CreateDevice(GUID_SysMouse,&ms,nullptr)==DI_OK);
  CHECK(kb->SetDataFormat(&c_dfDIKeyboard)==DI_OK); CHECK(ms->SetDataFormat(&c_dfDIKeyboard)!=DI_OK);
  DIDEVICEOBJECTDATA od[16]; uint32_t n=16;
  CHECK(kb->GetDeviceData(sizeof(od[0]),od,&n,0)==DIERR_NOTACQUIRED);
  SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.scancode=SDL_SCANCODE_W; dinput_sdl_feed(e); // dropped: not acquired
  kb->Acquire(); ms->Acquire();
  dinput_sdl_feed(e); e.key.repeat=true; dinput_sdl_feed(e); e.key.repeat=false; e.type=SDL_EVENT_KEY_UP; e.key.scancode=SDL_SCANCODE_LCTRL; dinput_sdl_feed(e);
  n=16; CHECK(kb->GetDeviceData(sizeof(od[0]),od,&n,0)==DI_OK);
  CHECK(n==2 && od[0].dwOfs==DIK_W && od[0].dwData==0x80 && od[1].dwOfs==DIK_LCONTROL && od[1].dwData==0);
  SDL_Event m{}; m.type=SDL_EVENT_MOUSE_MOTION; m.motion.xrel=0.6f; m.motion.yrel=-2.f; dinput_sdl_feed(m); dinput_sdl_feed(m);
  SDL_Event w{}; w.type=SDL_EVENT_MOUSE_WHEEL; w.wheel.y=-1.f; dinput_sdl_feed(w);
  SDL_Event b{}; b.type=SDL_EVENT_MOUSE_BUTTON_DOWN; b.button.button=SDL_BUTTON_RIGHT; dinput_sdl_feed(b);
  n=16; ms->GetDeviceData(sizeof(od[0]),od,&n,0);
  // motion 1: x 0 (0.6 kept), y -2; motion 2: x 1 (1.2), y -2; wheel -120; right button
  CHECK(n==5); CHECK(od[0].dwOfs==DIMOFS_Y && (int)od[0].dwData==-2); CHECK(od[1].dwOfs==DIMOFS_X && (int)od[1].dwData==1);
  CHECK(od[3].dwOfs==DIMOFS_Z && (int)od[3].dwData==-120); CHECK(od[4].dwOfs==DIMOFS_BUTTON1 && od[4].dwData==0x80);
  DIPROPSTRING ks{}; ks.diph.dwObj=DIK_RETURN; ks.diph.dwHow=DIPH_BYOFFSET; CHECK(kb->GetProperty(DIPROP_KEYNAME,&ks.diph)==DI_OK);
  printf("keyname(DIK_RETURN)=%ls dik(A)=%d fails=%d\n", ks.wsz, dinput_sdl_scancode_to_dik(SDL_SCANCODE_A), fails);
  kb->Release(); ms->Release(); di->Release(); return fails; }
