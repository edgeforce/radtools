#define WIN32_LEAN_AND_MEAN

#include "rad.h"
#include "string.h"
#include "stdlib.h"
#include "dos.h"

#include "ddraw.h"

#include "bink.h"

static HWND ourwind;
static HCURSOR cur;
static s32 softcur;

static HBINK bink=0;
static s32 surfacetype=0;
static s32 nofocus=0;
static s32 x,y,xadj,yadj;

static LPDIRECTDRAW lpdd=0;
static LPDIRECTDRAWSURFACE lpddsp=0;
static DDSURFACEDESC DDSdesc;


// open DirectDraw
static s32 open_dd()
{
  if (DirectDrawCreate(0,&lpdd,NULL)!=DD_OK)
    return(0);

  if (IDirectDraw_SetCooperativeLevel(lpdd,ourwind,DDSCL_NORMAL)!=DD_OK)
  {
    IDirectDraw_Release(lpdd);
    return(0);
  }

  memset(&DDSdesc,0,sizeof(DDSdesc)); //create primary
  DDSdesc.dwSize=sizeof(DDSdesc);
  DDSdesc.dwFlags=DDSD_CAPS;
  DDSdesc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;

  if (IDirectDraw_CreateSurface(lpdd,&DDSdesc,&lpddsp,NULL)==DD_OK)
  {
    surfacetype=BinkDDSurfaceType(lpddsp);
    if ((surfacetype==-1) || (surfacetype==BINKSURFACE8P))
    {
      MessageBox(0,"Unsupported primary surface format.","Error",MB_OK|MB_ICONSTOP);
      IDirectDrawSurface_Release(lpddsp);
      IDirectDraw_Release(lpdd);
      return(0);
    }

    softcur=BinkIsSoftwareCursor(lpddsp,cur);
  }

  return(1);
}


// close DirectDraw
static void close_dd()
{
  if (lpddsp)
  {
    IDirectDrawSurface_Release(lpddsp);
    lpddsp=0;
  }

  if (lpdd)
  {
    IDirectDraw_Release(lpdd);
    lpdd=0;
  }
}


// draw with blackness
static void DoPaint( HWND win_handle)
{
  PAINTSTRUCT ps;

  HDC dc=BeginPaint(win_handle,&ps);  // clear the repaint flag

  PatBlt(dc,0,0,4096,4096,BLACKNESS);

  EndPaint(win_handle,&ps);
}


// the main window proc
LONG FAR PASCAL WindowProc( HWND win_handle, unsigned msg, WORD wparam, LONG lparam )
{

  switch( msg ) {
    case WM_CHAR:
      DestroyWindow(win_handle);
      break;

    case WM_SETFOCUS:
      nofocus=0;
      BinkPause(bink,0);
      break;

    case WM_KILLFOCUS:
      nofocus=1;
      BinkPause(bink,1);
      break;

    case WM_PAINT:
      DoPaint(win_handle);
      return(0);

    case WM_ERASEBKGND:
      return(1);

    case WM_WINDOWPOSCHANGING:
      // this insures that we are always aligned

      if ((((WINDOWPOS*)lparam)->flags&SWP_NOMOVE)==0)
      {
	// align on a 4 pixel boundary
	((WINDOWPOS*)lparam)->x=((((WINDOWPOS*)lparam)->x-xadj)&~3)+xadj;

	x=((WINDOWPOS*)lparam)->x+xadj;
        y=((WINDOWPOS*)lparam)->y+yadj;
      }
      break;

    case WM_DESTROY:
      PostQuitMessage( 0 );
      return(0);
  }
  return( DefWindowProc( win_handle, msg, wparam, lparam ) );
}


// create a class
static BOOL FirstInstance( HINSTANCE this_inst )
{
  WNDCLASS wc;
  
  cur=LoadCursor(0,IDC_ARROW);

  wc.style = 0;
  wc.lpfnWndProc = (WNDPROC)(LPVOID) WindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = this_inst;
  wc.hIcon = LoadIcon(this_inst,MAKEINTRESOURCE(101));
  wc.hCursor = cur;
  wc.hbrBackground = 0;
  wc.lpszMenuName = 0;
  wc.lpszClassName = "BinkExam";
  return( RegisterClass( &wc ) );
}


// advance to the next Bink Frame
static void NextBinkFrame( HWND win_handle )
{
  s32 count;

  // decompress a frame
  BinkDoFrame(bink);

  // check for a software cursor - hide it, if found
  if (softcur)
    count=BinkCheckCursor(win_handle,0,0,bink->Width,bink->Height);

  // lock the primary surface
  while (IDirectDrawSurface_Lock(lpddsp,0,&DDSdesc,DDLOCK_WAIT,0)==DDERR_SURFACELOST)
    if (IDirectDrawSurface_Restore(lpddsp)!=DD_OK)
      goto cantlock;  // just exit if we can't restore the surface

  // copy the data onto the screen
  BinkCopyToBuffer(bink,DDSdesc.lpSurface,DDSdesc.lPitch,bink->Height,x,y,surfacetype);

  // unlock the surface
  IDirectDrawSurface_Unlock(lpddsp,DDSdesc.lpSurface);

 cantlock:

  // restore the software cursor, if we hid it before
  if (softcur)
    BinkRestoreCursor(count);

  // advance or close the window
  if (bink->FrameNum==bink->Frames) // goto the next if not on the last
    DestroyWindow(win_handle);
  else
    BinkNextFrame(bink);
}


// main entry point
int PASCAL WinMain( HINSTANCE this_inst, HINSTANCE prev_inst, LPSTR cmdline,int cmdshow )
{
  MSG msg;
  RECT r;

  cmdshow=cmdshow;

  if(!prev_inst)
    if (!FirstInstance(this_inst))
      return(1);

  ourwind=CreateWindow( "BinkExam",
                        "Bink Example Player",
			WS_CAPTION|WS_POPUP|WS_CLIPCHILDREN|WS_SYSMENU|WS_MINIMIZEBOX,
                        64,64,32,32,0,0,this_inst,0);
  if( !ourwind )
  {
    MessageBox(0,"Error creating window.","Windows",MB_OK|MB_ICONSTOP);
    return(1);
  }

  // get all of the window/client offsets
  GetWindowRect(ourwind,&r);
  xadj=r.left;
  yadj=r.top;
  r.left=0;
  r.top=0;
  ClientToScreen(ourwind,(POINT*)&r);
  x=r.left;
  y=r.top;
  xadj=r.left-xadj;
  yadj=r.top-yadj;
  GetClientRect(ourwind,&r);

  // open DirectDraw
  if (!open_dd())
  {
    DestroyWindow(ourwind);
    return(1);
  }

  BinkSoundUseDirectSound(0);

  // Open the Bink file
  bink=BinkOpen(cmdline,0);
  if (!bink) {
    MessageBox(0,BinkGetError(),"Bink Error",MB_OK|MB_ICONSTOP);
    DestroyWindow(ourwind);
    return(1);
  }

  SetWindowPos(ourwind,0,0,0,bink->Width+32-r.right,bink->Height+32-r.bottom,SWP_NOMOVE);

  ShowWindow(ourwind,SW_SHOWNORMAL);

  for (;;)
  {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
        break;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      if (!BinkWait(bink))
        NextBinkFrame(ourwind);

      if (nofocus)
        Sleep(1);
    }
  }

  if (bink)
  {
    BinkClose(bink);
    bink=0;
  }

  close_dd();

  return(0);
}

