/*****************************************************************\


	Library of X window functions which call Windows 32
	equivalent functions.

	Some data structures are maintained by this code,
	simulating the operation of an X server and window manager.

	Aug/Sep-92	xyz	$$1 Created.
        Oct-92  abc $$2 Added color stuff.

\******************************************************************/
#ifdef WIN32_XNT

#ifndef __XNT
#define __XNT

#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <winsock.h>
#include "xnt.h"        /* Includes X.h, Xlib.h, Xutil.h mainly. */

#define xtrace(x) /* cjh_printf(x)  */
#define cjh_printf
#define CNUMTORGB(x) RGB(\
	(BYTE) (sysPal->palPalEntry[x].peRed),\
	(BYTE) (sysPal->palPalEntry[x].peGreen),\
	(BYTE) (sysPal->palPalEntry[x].peBlue))
	 
	 /* #define printf(x)   *//* x */ 
	 /* #define SetSystemPaletteUse(x) *//* x */

/* Local Data */

static char *app_name = "Sample";
static Atom wmDeleteWindow;
static XEvent l_event;     /* Local structure filled in by Window function */
static HANDLE hInstance,hPrevInstance;
static NT_window *NT_CWIN=NULL;	/* Handle of the window under the cursor */
static Display *d = NULL;       /* The display structure of this display. */
static NT_window *PIWindow = NULL;
static int PIWidth,PIHeight;
static unsigned char *smallBitBase;
static HDC hdcPal = INVALID_HANDLE;
static HPALETTE hpalX;
static LOGPALETTE *sysPal;
static unsigned int cmapSize;
static unsigned int nextFreeColour;
static long windowBg;
static int sysColorNumArray[21] = {COLOR_ACTIVEBORDER,COLOR_ACTIVECAPTION,COLOR_APPWORKSPACE,COLOR_BACKGROUND,
                                   COLOR_BTNFACE,COLOR_BTNSHADOW,COLOR_BTNTEXT,COLOR_CAPTIONTEXT,COLOR_GRAYTEXT,
                                   COLOR_HIGHLIGHT,COLOR_HIGHLIGHTTEXT,COLOR_INACTIVEBORDER,COLOR_INACTIVECAPTION,
                                   COLOR_INACTIVECAPTIONTEXT,COLOR_MENU,COLOR_MENUTEXT,COLOR_SCROLLBAR,COLOR_BTNSHADOW,
                                   COLOR_WINDOW,COLOR_WINDOWFRAME,COLOR_WINDOWTEXT};
static COLORREF sysColorArray[21];
static SOCKET so_pair[2];

static void freeMemory(p)
void *p;
{
	if (p!=NULL)
	{
		free(p);
	}
}
static void *allocateMemory(s)
int s;
{
	void *p=NULL;
	if (s)
	{
		s +=7;
		s &= ~7;
		p=(void *)malloc(s);
	}
	return p;
}

typedef struct WinEvent_
{
	NT_window *window;
	UINT message;
	UINT wParam;
	LONG lParam;
} WinEvent;

typedef struct WinEventQ_
{
	int num;
	int avail;
	int next;
	int count;
	WinEvent list[100];
} WinEventQ;

static WinEventQ *wineventq;

static void
initQ(WinEventQ **q)
{
	int i;
	*q = (WinEventQ *)allocateMemory(sizeof(WinEventQ));
	(*q)->num=99;
	(*q)->avail=0;
	(*q)->next=0;
	(*q)->count=0;
	for (i=0; i<100; i++) {
		(*q)->list[i].message=0;
		(*q)->list[i].window = NULL;
	}
}

static void
copyWinEvent(WinEvent *to, WinEvent *from)
{
	to->window = from->window;
	to->message = from->message;
	to->wParam = from->wParam;
	to->lParam = from->lParam;
}

#ifdef blah
static int
QEventSo(WinEventQ *q, NT_window *window,UINT message,UINT wParam,LONG lParam)
{
	UINT buff[4];
	buff[0] = (UINT) window;
	buff[1] = message;
	buff[2] = wParam;
	buff[3] = (UINT) lParam;
	write(so_pair[0], buff, sizeof(UINT)*4 );
	q->count ++;
	return 0;
}
#endif

static int
QEvent(WinEventQ *q, NT_window *window,UINT message,UINT wParam,LONG lParam)
{
	int i, j;
	if (q->count>=q->num) {
		for (i=0;i<q->num;i++) cjh_printf("%d, ",q->list[i].message);
		cjh_printf("Queue count %d q->num %d\n", q->count, q->num);
	}
	if (q->count>1 && (message == WM_WINDOWPOSCHANGED ||
					   message == WM_PAINT)) {
		/* only the most recent is interesting, so remove the previous
		   event in the queue */
		i = j = q->avail; i--; if (i<0) i=q->num;
		while (i!=q->next && (q->list[i].message!=message || q->list[i].window!=window)) {
			j=i; i--; if (i<0) i = q->num;
		}
		if (q->list[i].message==message && q->list[i].window==window) {
			while(j!=q->avail) {
				copyWinEvent(&q->list[i],&q->list[j]);
				i=j; j++; if (j>q->num) j=0;
			}
			q->avail = i;
			q->count --;
		}
	}
	q->list[q->avail].window=window;
	q->list[q->avail].message=message;
	q->list[q->avail].wParam=wParam;
	q->list[q->avail].lParam=lParam;
	q->avail++; q->count++;
	if (q->avail>q->num) q->avail=0;
	return 1;
}
#ifdef blah
static int
getQdEventSo(WinEventQ *q, WinEvent *we)
{
	UINT buff[4];
	if (q->count<=0) {
		cjh_printf("Queue is empty\n");
		return 0;
	}
	read(so_pair[1], buff, sizeof(UINT)*4);
	we->window = (NT_window *)buff[0];
	we->message = buff[1];
	we->wParam = buff[2];
	we->lParam = (LONG)buff[3];
	q->count--;
}
#endif

static int
getQdEvent(WinEventQ *q, WinEvent *we)
{
	if (q->count<=0) {
		cjh_printf("Queue is empty\n");
		return 0;
	}
	we->window = q->list[q->next].window;
	we->message = q->list[q->next].message;
	we->wParam = q->list[q->next].wParam;
	we->lParam = q->list[q->next].lParam;
	q->next++; q->count--;
	if (q->next>q->num) q->next=0;	
	return 1;
}
void
GetSysColors(number,nums,vals)
int number;
int *nums;
COLORREF *vals;
{
  int i;
  for (i=0;i<number;i++)
    vals[i] = GetSysColor(nums[i]);
}
/* Routine declarations */

static struct NT_window      *NT_find_window_from_id();
static int                    NT_delete_window();
static struct NT_window      *NT_new_window();
static struct NT_window      *NT_new_window_pair();
static void                   NT_set_GC_pen();
static void                   NT_set_GC_brush();
static HPEN                   NT_get_GC_pen();
static HBRUSH                 NT_get_GC_brush();
static int NT_add_child(NT_window *parent,NT_window *child);
static int NT_show_window_list();
static NT_del_prop_list(struct NT_prop_list *prop_list);
static int NT_del_child(NT_window *parent,NT_window *child);

void NT_set_rop();
double NT_deg64_to_rad(int a);

/*----------------------------------------------------------------*\
| FUNCTIONS TO MAINTAIN AN INTERNAL LIST OF WINDOWS AND THEIR      |
| ATTRIBUTES - AS WOULD BE MAINTAINED IN THE X SERVER.             |
\*----------------------------------------------------------------*/

static struct NT_window *window_list=NULL;

/* Structure to hold pen and brush info in GC ext_data field */

typedef struct NTGC_
{
HPEN	pen;
HBRUSH	brush;
} NTGC;

/* Sizes of window decoration features. */
#define NT_TITLE_BORDER_SIZE 32
#define NT_BORDER_SIZE 8

typedef struct dc_info_
{
  struct dc_info_ *next;
  HWND win_id;
  HDC  dc_id;
  int rop;
} DCInfo;

static DCInfo DCIHead = {NULL,0,0};
static int num_dcs = 0;

void
del_from_dc_info(window,dc)
HWND window;
HDC dc;
{
  DCInfo *ptr = DCIHead.next;
  DCInfo *prev = &DCIHead;
  while (ptr)
  {
    if (ptr->win_id == window && ptr->dc_id == dc)
    {
      prev->next=ptr->next;
      freeMemory(ptr);
      return;
    }
    prev = ptr;
    ptr = ptr->next;
  }
  cjh_printf("Could not find HDC %x for window %x\n");
}

int find_in_dc_info(window,dc)
HWND window;
HDC *dc;
{
  DCInfo *ptr = DCIHead.next;
  while (ptr)
  {
    if (ptr->win_id == window && (*dc==0 || *dc==ptr->dc_id))
    {
      *dc = ptr->dc_id;
      return TRUE;
    }
    ptr = ptr->next;
  }
  return FALSE;
}

void
add_to_dc_info(window,dc)
HWND window;
HDC dc;
{
   DCInfo *newInfo = NULL;
   if(!(newInfo = allocateMemory(sizeof(newInfo))))
   {
     cjh_printf("Failed to malloc DCInfo struct.\n");
     return;
   }
   newInfo->win_id = window;
   newInfo->dc_id = dc;
   newInfo->next = DCIHead.next;
   DCIHead.next = newInfo;
}

HDC cjh_get_dc(window)
HWND window;
{
  HDC foundDC = 0,newDC;
  if (find_in_dc_info(window,&foundDC))
  {
  /*    printf("****************Window %x already has a DC %x allocated.\n",window,foundDC);*/
      newDC = foundDC;
  }
  else
  {
    newDC = GetDC(window);

    if (newDC!=0)
    {
      add_to_dc_info(window,newDC);
    }
    cjh_printf("Got Window %x a dc with value %x                    %d\n",window,newDC,++num_dcs);
  }
  return newDC;
}

HDC cjh_get_dc_ex(window,hrgnClip,flags)
HWND window;
HRGN hrgnClip;
DWORD flags;
{
  HDC foundDC = 0,newDC;
  return cjh_get_dc(window);
  if (find_in_dc_info(window,&foundDC))
  {
      cjh_printf("EX**************Window %x already has a DC %x allocated.\n",window,foundDC);
  }

  newDC = GetDC(window); /*Ex(window,hrgnClip,flags);*/
  if (newDC!=0)
  {
    add_to_dc_info(window,newDC);
  }
  cjh_printf("Got Window %x an ex dc with value %x                    %d\n",window,newDC,++num_dcs);
  return newDC;
}


int cjh_rel_dc(window,dc)
HWND window;
HDC dc;
{
  HDC foundDC;
/***
 ***  MODIFIED TO RETURN TRUE HERE!!!!!!!!!!
 ***/

  return TRUE;

  foundDC = dc;
  if (!find_in_dc_info(window,&foundDC))
  {
      cjh_printf("######################Window %x doesnt have a DC %x\n",window,dc);
      return FALSE;
  }
  cjh_printf("Deleteing Window %x the dc with value %x                      %d\n",window,dc,--num_dcs);
  del_from_dc_info(window,dc);
  if(!find_in_dc_info(window,&foundDC))
    ReleaseDC(window,dc);
  else
    cjh_printf("Whoops Two Entries for %x and %x - Not releasing DC.\n",window,foundDC);
  return TRUE;
}


/****************************************************************\
 FOCUS PUSHDOWN STACK CODE!
\****************************************************************/

typedef struct _pd_focus {
  NT_window *top;
  struct _pd_focus *next;
 } pdFocus;

static pdFocus focusHead = {NULL,NULL};

void
addFocusWindow(newWin)
NT_window *newWin;
{
  pdFocus *newFocus;

  if(!(newFocus = allocateMemory(sizeof(pdFocus))))
  {
    cjh_printf("Argh, Failed to get memory for focus stack - exiting.\n");
    return;
  }
  newFocus->top = newWin;
  newFocus->next = focusHead.next;
  focusHead.next = newFocus;
}

void
raiseFocusWindow(toRaise)
NT_window *toRaise;
{
  pdFocus *finder,*follower;
  finder = focusHead.next;
  follower = &focusHead;
  while(finder)
  {
    if(finder->top == toRaise)
      break;
    follower = finder;
    finder = finder->next;
  }

  if (!finder)
  {
    cjh_printf("Error - raising unknown window.\n");
    return;
  }
  follower->next = finder->next;
  finder->next = focusHead.next;
  focusHead.next = finder;
}

void
delFocusWindow(toDel)
NT_window *toDel;
{
  pdFocus *finder,*follower;
  finder = focusHead.next;
  follower = &focusHead;

  while(finder)
  {
    if(finder->top == toDel)
      break;
    follower = finder;
    finder = finder->next;
  }

  if (!finder)
  {
    cjh_printf("Error - deleting unknown window.\n");
    return;
  }
  follower->next = finder->next;
  freeMemory(finder);
}

void
activateTopWin()
{
  if(focusHead.next)
  {
    SetActiveWindow(focusHead.next->top->w);
    SetFocus(focusHead.next->top->client->w);
  }
}


HWND
getTopWinHandle()
{
  if(focusHead.next)
    return focusHead.next->top->w;
  else
    return 0;
}

/*****************************************************************\

	Function:  NT_OrderFrame

	Comments:  Set the frame to TOPMOST or NOTOPMOST according to
		   the top_flag in the window definition.

\*****************************************************************/

void
NT_OrderFrame(window,raise)
NT_window *window;
int    raise;
{
  if ( raise )
    {
    if ( window->top_flag == TRUE)
      SetWindowPos(window->w, HWND_TOPMOST,
                   0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
    else
      SetWindowPos(window->w, HWND_NOTOPMOST,
                   0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
    }
  else
    SetWindowPos(window->w, HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE );
}

void
NT_OrderFocusList(head, raise)
pdFocus *head;
int     raise;
{

  if ( head->next != NULL )
    NT_OrderFocusList(head->next, raise);
  if ( head->top != NULL )
    NT_OrderFrame(head->top, raise);
}

void
NT_OrderAllFrames(raise)
int raise;
{
  if ( focusHead.next != NULL )
    NT_OrderFocusList(focusHead.next, raise);
}

/*****************************************************************\

	Function: MainWndProc
	Inputs:   Window handle, message, message parameters.

	Comments: This is called when messages are sent to the application
		  but not to the application's message queue.  If an
		  event can be processed, it is done here.  The equivalent
		  XEvent is filled out in l_event, which is picked up
		  in the X event routines.  Some events are not received
		  from Windows, eg Enter/LeaveNotify, so these are made
		  up as required.

	Caution:  The application does not see HWND, but Window.

\*****************************************************************/
/* queued messages
   WM_KEYDOWN
   WM_KEYUP
   WM_CHAR
   WM_MOUSEMOVE
   WM_BUTTONxx
   WM_TIMER
   WM_PAINT
   WM_QUIT
   */
   
LONG APIENTRY MainWndProc(
	HWND hWnd,		  /* window handle		     */
	UINT message,		  /* type of message		     */
	UINT wParam,		  /* additional information	     */
	LONG lParam)		  /* additional information	     */
{
	RECT rect;
	WINDOWPOS *posStruct;
	unsigned long int st=0L;
	NT_window *window;

	/* apparently the WM_CLOSE message doesn't contain a good hWnd */
	if (message == WM_CLOSE) exit(0);
	
	window = NT_find_window_from_id(hWnd);
	if (window == NULL) return  (DefWindowProc(hWnd, message, wParam, lParam));
	
    switch (message) {
		case WM_LBUTTONDOWN:  /* we'll handle these, later */
		case WM_LBUTTONDBLCLK:
		case WM_KILLFOCUS:
		case WM_SETFOCUS:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_QUIT:
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_DESTROYCLIPBOARD:
			/*		case WM_MOVE: */
		case WM_CHAR:
		case USR_StructureNotify:
		case USR_EnterNotify:
		case WM_PAINT:
			QEvent(wineventq,window,message,wParam,lParam);
			break;
		case WM_WINDOWPOSCHANGED:
			posStruct = (WINDOWPOS *)lParam;
			/*			MoveWindow(window->w, posStruct->x, posStruct->y,
						   posStruct->cx, posStruct->cy,FALSE); */
			if (window->client!=NULL && (int)window->client->w!=-1) {
				window=window->client;
				GetClientRect(window->w, &rect);
				MoveWindow(window->w,rect.left,rect.top,
						   rect.left+posStruct->cx-8,
						   rect.top+posStruct->cy-NT_TITLE_BORDER_SIZE,FALSE);
			}
			QEvent(wineventq,window,message,wParam,lParam);
			break;			
		case WM_KEYDOWN:
			switch (wParam)
			{
				case VK_CANCEL:
				case VK_CLEAR:
				case VK_MENU:
				case VK_PAUSE:
				case VK_PRIOR:
				case VK_NEXT:
				case VK_END:
				case VK_HOME:
				case VK_LEFT:
				case VK_UP:
				case VK_RIGHT:
				case VK_DOWN:
				case VK_SELECT:
				case VK_PRINT:
				case VK_EXECUTE:
				case VK_INSERT:
				case VK_DELETE:
				case VK_HELP:
				case VK_NUMLOCK:
				case VK_SCROLL:
					QEvent(wineventq,window,message,wParam,lParam);
					break;
				default:
					return  (DefWindowProc(hWnd, message, wParam, lParam));
					break;
					
			}
			break;
		default:			  /* Passes it on if unproccessed    */
			return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return 0L;
}

int
WinEventToXEvent(
	WinEvent *we,
	XEvent *event)
{
	POINT pt;
	RECT rect;
	PAINTSTRUCT paintStruct;
	unsigned long int st=0L;
	UINT key;
	HWND hWnd;
	UINT wParam;
	LONG lParam;
	NT_window *window;
	
	do {		
		if (event==NULL) break;
		if (we == NULL) break;

		window = we->window;
		wParam = we->wParam;
		lParam = we->lParam;
		
		event->type=-1;
		event->xbutton.subwindow = None;
		hWnd = window->w;
		
		switch (we->message) {
			case WM_SETFOCUS:
				event->type=FocusIn;
				event->xfocus.window=(Window)window;
				break;
			case WM_KILLFOCUS:
				event->type=FocusOut;
				event->xfocus.window=(Window)window;
				break;	
			case WM_PAINT:
				if (window->client!=NULL)
					window=window->client;
				event->type=Expose;
				BeginPaint(hWnd,&paintStruct);
				event->xexpose.x=paintStruct.rcPaint.left;
				event->xexpose.y=paintStruct.rcPaint.top;
				event->xexpose.width=paintStruct.rcPaint.right-paintStruct.rcPaint.left;
				event->xexpose.height=paintStruct.rcPaint.bottom-paintStruct.rcPaint.top;
				event->xexpose.count=0;
				event->xexpose.window=(Window)window;
				/* FillRect(paintStruct.hdc, &paintStruct.rcPaint,
					        GetStockObject(WHITE_BRUSH)); */
				EndPaint(hWnd,&paintStruct);
				break;
			case WM_LBUTTONDOWN:
			case WM_LBUTTONDBLCLK:
				event->type = ButtonPress;
				event->xbutton.x = LOWORD (lParam);
				event->xbutton.y = HIWORD (lParam);
				if ( wParam & MK_SHIFT )
					event->xbutton.button=Button2;
				else
					event->xbutton.button=Button1;
				event->xbutton.window = (Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_LBUTTONUP:
				event->type=ButtonRelease;
				event->xbutton.x=LOWORD(lParam);
				event->xbutton.y=HIWORD (lParam);
				if ( wParam & MK_SHIFT )
					event->xbutton.button=Button2;
				else
					event->xbutton.button=Button1;
				event->xbutton.window=(Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_MBUTTONDOWN:
			case WM_MBUTTONDBLCLK:
				event->type=ButtonPress;
				event->xbutton.x=LOWORD(lParam);
				event->xbutton.y=HIWORD (lParam);
				event->xbutton.button=Button2;
				event->xbutton.window=(Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_MBUTTONUP:
				event->type=ButtonRelease;
				event->xbutton.x=LOWORD(lParam);
				event->xbutton.y=HIWORD (lParam);
				event->xbutton.button=Button2;
				event->xbutton.window=(Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONDBLCLK:
				event->type=ButtonPress;
				event->xbutton.x=LOWORD(lParam);
				event->xbutton.y=HIWORD (lParam);
				event->xbutton.button=Button3;
				event->xbutton.window=(Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_RBUTTONUP:
				event->type=ButtonRelease;
				event->xbutton.x=LOWORD(lParam);
				event->xbutton.y=HIWORD (lParam);
				event->xbutton.button=Button3;
				event->xbutton.window=(Window)window;
				event->xbutton.time=GetTickCount();
				break;
			case WM_MOUSEMOVE:
				if (hWnd!=NT_CWIN)  /* Mouse in different window? */
				{
					if (NT_CWIN==NULL) /* No previous window */
					{
						event->type = EnterNotify;
						event->xcrossing.x = LOWORD(lParam);
						event->xcrossing.y = HIWORD(lParam);
						event->xcrossing.window = (Window)window;
					}
					else
					{
						event->type=LeaveNotify;
						event->xcrossing.x=LOWORD(lParam);
						event->xcrossing.y=HIWORD(lParam);
						event->xcrossing.window = (Window)NT_find_window_from_id(NT_CWIN);
					}
				}
				else
				{
					event->type=MotionNotify;    /* Fill out mouse event */
					event->xmotion.window=(Window)window;
					event->xmotion.x=pt.x=LOWORD(lParam);
					event->xmotion.y=pt.y=HIWORD(lParam);
					ClientToScreen(hWnd,&pt);     /* Translate coordinates */
					event->xmotion.x_root=pt.x;
					event->xmotion.y_root=pt.y;
					if (wParam&MK_CONTROL)
						st|=ControlMask;
					if (wParam&MK_SHIFT)
						st|=ShiftMask;
					if (wParam&MK_LBUTTON)
						st|=Button1Mask;
					if (wParam&MK_MBUTTON)
						st|=Button2Mask;
					if (wParam&MK_RBUTTON)
						st|=Button3Mask;
					event->xmotion.state=st;
				}
				NT_CWIN=hWnd;
				break;
			case WM_CHAR:
				event->type=KeyPress;
				event->xkey.state=0;
				event->xkey.keycode=wParam;
				event->xkey.window=(Window)window;
				break;
			case WM_KEYDOWN:
				event->type=KeyPress;
				event->xkey.state=0;
				switch (wParam)
				{
					case VK_CANCEL:  key=XK_Cancel;      break;
					case VK_CLEAR:   key=XK_Clear;       break;
					case VK_MENU:    key=XK_Alt_L;       break;
					case VK_PAUSE:   key=XK_Pause;       break;
					case VK_PRIOR:   key=XK_Prior;       break;
					case VK_NEXT:    key=XK_Next;        break;
					case VK_END:     key=XK_End;         break;
					case VK_HOME:    key=XK_Home;        break;
					case VK_LEFT:    key=XK_Left;        break;
					case VK_UP:      key=XK_Up;          break;
					case VK_RIGHT:   key=XK_Right;       break;
					case VK_DOWN:    key=XK_Down;        break;
					case VK_SELECT:  key=XK_Select;      break;
					case VK_PRINT:   key=XK_Print;       break;
					case VK_EXECUTE: key=XK_Execute;     break;
					case VK_INSERT:  key=XK_Insert;      break;
					case VK_DELETE:  key=XK_Delete;      break;
					case VK_HELP:    key=XK_Help;        break;
					case VK_NUMLOCK: key=XK_Num_Lock;    break;
					case VK_SCROLL:  key=XK_Scroll_Lock; break;
					default:         key=0;              break;
				}
				if (key == 0) {
					event->type = -1;
				}
				else
				{
					event->xkey.keycode=key;
					event->xkey.window=(Window)window;
				}
				break;
			case WM_DESTROY:
			case WM_QUIT:
			case WM_CLOSE:
				event->type = ClientMessage;
				break;
			case USR_EnterNotify:
				event->type = EnterNotify;
				event->xcrossing.x = LOWORD(lParam);
				event->xcrossing.y = HIWORD(lParam);
				event->xcrossing.window = (Window)window;
				break;

			case WM_WINDOWPOSCHANGED:
				GetClientRect(window->w,&rect);
				event->type = ConfigureNotify;
				event->xconfigure.window = (Window)window;
				event->xconfigure.x = rect.left;
				event->xconfigure.y = rect.top;
				event->xconfigure.width = rect.right-rect.left;
				event->xconfigure.height = rect.bottom-rect.top;
				event->xconfigure.above = Above;
				break;
			case WM_DESTROYCLIPBOARD:
				event->type = SelectionClear;
				event->xselectionclear.time = GetTickCount();
				break;
			case USR_StructureNotify:
				event->type=ConfigureNotify;
				break;
			default:
				break;
		}
	} while(0);
    return (event==NULL?0: (event->type==-1?0:1));
}

#ifdef WINDOWSAPP
/*****************************************************************\

	Function: WinMain
	Inputs:   instance, previous instance, command line arguments,
		  default start up.

	Comments: Called instead of main() as the execution entry point.

\*****************************************************************/

int main(int argc, char *argv[]);

#define MAX_COMMAND_ARGS 20
int  WINAPI 
WinMain(HANDLE hInst,HANDLE hPrevInst,LPSTR lpCmdLine,int nCmdShow)
{
        static char *command_args[MAX_COMMAND_ARGS];
        static int num_command_args;
        static char proEng[] = "proe";
        char *wordPtr,*tempPtr;
        int i,quote;
	hInstance=hInst;
	hPrevInstance=hPrevInst;
        for (i=0;i<MAX_COMMAND_ARGS;i++)
          command_args[i] = NULL;

        wordPtr = lpCmdLine;
        quote = 0;
        num_command_args = 1;
        command_args[0] = proEng;
        while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
           wordPtr++;
        if (*wordPtr == '\"')
        {
          quote = 1;
          wordPtr++;
        }
        if (!*wordPtr)
          main(0,NULL);
        else
        {
          while (*wordPtr && num_command_args < MAX_COMMAND_ARGS)
          {
            tempPtr = wordPtr;
            if (quote)
            {
              while (*tempPtr && *tempPtr != '\"')
                tempPtr++;
              quote = 0;
            }
            else
              while (*tempPtr && *tempPtr != ' ')
                tempPtr++;
            if (*tempPtr)
              *(tempPtr++) = '\0';
            command_args[num_command_args++] = wordPtr;
            wordPtr = tempPtr;
            while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
              wordPtr++;
            if (*wordPtr == '\"')
            {
              quote = 1;
              wordPtr++;
            }
          }
          main(num_command_args,command_args);
        }

return(0);
}
#endif

/*****************************************************************\

	Function: XOpenDisplay
	Inputs:   Display name

	Comments: Fills out a Display structure and a Visual and Screen.
		  Hopefully all the X macros should work with this
		  structure.  Note that the default visual is for a
		  True colour screen (24bits).
\*****************************************************************/
Display *
XOpenDisplay (name)
const char *name;
{
	static char vstring[]="NT Xlibemu",
		*vs,*dn;

	Screen *scrd;
	static Depth dlist[1];
	static Visual vlist[1];
	Colormap cmap;
	RECT rect;
	HDC rootDC = CreateDC("DISPLAY",NULL,NULL,NULL);
	xtrace("XOpenDisplay\n");

	initQ(&wineventq);
	
	dlist[0].depth=8;
	dlist[0].nvisuals=1;
	dlist[0].visuals=vlist;
	vlist[0].ext_data=NULL;
	vlist[0].visualid=0;
	vlist[0].class=PseudoColor;
	vlist[0].bits_per_rgb=8;
	vlist[0].map_entries=256;
	vlist[0].red_mask=255;
	vlist[0].green_mask=255<<8;
	vlist[0].blue_mask=255<<16;
	scrd=(Screen *) allocateMemory (sizeof (Screen));
	scrd->display=d;
	(NT_window*)(scrd->root)= NT_new_window();
	((NT_window*)(scrd->root))->w=GetDesktopWindow();
	((NT_window*)(scrd->root))->parent=0;
	((NT_window*)(scrd->root))->frame=NULL;
	GetWindowRect(GetDesktopWindow(),&rect);
	scrd->width=rect.right-rect.left;
	scrd->height=rect.bottom-rect.top;
	scrd->mwidth=260;
	scrd->mheight=190;
	scrd->ndepths=1;
	scrd->depths=dlist;
	scrd->root_depth=8;
	scrd->root_visual=vlist;
	scrd->default_gc=NULL;
	scrd->cmap=cmap;
	scrd->white_pixel=1;
	scrd->black_pixel=0;


	d=(Display *) allocateMemory (sizeof (Display));
	vs=(char *) allocateMemory (sizeof (char)*strlen (vstring)+1);
	dn=(char *) allocateMemory (sizeof (char)*strlen (name)+1);
	strcpy (vs,vstring);
	strcpy (dn,name);
	d->ext_data=NULL;
	//	d->next=NULL;
	d->fd=0;
	d->lock=0;
	d->proto_major_version=11;
	d->proto_minor_version=4;
	d->vendor=vs;
	d->resource_base=0;
	d->resource_mask=0;
	d->resource_id=0;
	d->resource_shift=0;
	d->resource_alloc=NULL;
	d->byte_order=0;
	d->bitmap_unit=0;
	d->bitmap_pad=0;
	d->bitmap_bit_order=0;
	d->nformats=0;
	d->pixmap_format=NULL;
	d->vnumber=11;
	d->release=4;
	d->head=NULL;
	d->tail=NULL;
	d->qlen=0;
	d->last_request_read=0;
	d->request=0;
	d->last_req=NULL;
	d->buffer=NULL;
	d->bufptr=NULL;
	d->bufmax=NULL;
	d->max_request_size=0;
	d->db=NULL;
	d->synchandler=NULL;
	d->display_name=dn;
	d->default_screen=0;
	d->nscreens=1;
	d->screens=scrd;
	d->motion_buffer=0;
	//	d->current=0;
	d->min_keycode=0;
	d->max_keycode=255;
	d->keysyms=NULL;
	d->modifiermap=NULL;
	d->keysyms_per_keycode=0;
	d->xdefaults=NULL;
	d->scratch_buffer=NULL;
	d->scratch_length=0;
	d->ext_number=0;
	d->ext_procs=NULL;
	/*	d->event_vec[]=;
		d->wire_vec[]=;  */

	/*
	 * Intern some Atoms in our role as a fake MWM
	 */
	XInternAtom(d,_XA_MOTIF_WM_HINTS,FALSE);
	XInternAtom(d,_XA_MWM_HINTS,FALSE);
	wmDeleteWindow = XInternAtom(d,"WM_DELETE_WINDOW", FALSE);

	/*
	 * Initialize the colour map.
	 */
	cmapSize=50;
   
	sysPal = allocateMemory(sizeof(LOGPALETTE) + (cmapSize-1)*sizeof(PALETTEENTRY));
	{
		XColor xc;
		xc.red=xc.blue=xc.green = 0x0000;
		XAllocColor(d,cmap,&xc);	
		xc.red=xc.blue=xc.green = 0xffff;
		XAllocColor(d,cmap,&xc);
	}
	
	hdcPal = rootDC;
	
	/*	socketpair(AF_INET, SOCK_STREAM, IPPROTO_TCP, so_pair); */

	return (d);
}


int
XCloseDisplay(display)
Display *display;
{
  NT_window *wanderer;

  xtrace("XCloseDisplay\n");

  wanderer = window_list;
  while(wanderer)
  {
    DestroyWindow(wanderer->w);
    wanderer = wanderer->next;
  }
/* Do something ? */
/* Must GlobalDelete all atoms/properties leftover */
  return 0;
}

char
*XDisplayString(display)
Display *display;
{
	return (display->display_name);
}


void putImagePart2();

static NT_window *cachedImageWindow = NULL;
int
XFlush(display)
Display *display;
{
	xtrace("XFlush\n");
  putImagePart2();
  return 0;
}


int
XSync(display,discard)
Display *display;
int discard;
{
	/* Do nothing here either */
	return 0;
}

/*****************************************************************\

	Function: XGetVisualInfo
	Inputs:   display, info mask, template, number of matches.
	Returned: List of XVisualInfo structures, one for each matching
		  Visual.

	Comments: Behaves like X routine, but there is only ever one
		  Visual, so the returned list is never longer than one.

\*****************************************************************/
XVisualInfo *
XGetVisualInfo(display,vinm,vint,n)
Display *display;
long vinm;
XVisualInfo *vint;
int *n;
{
	static XVisualInfo xvi;
	int status=1;
	xtrace("XGetVisualInfo\n");

	if ((vinm&VisualIDMask|vinm==VisualAllMask)&&
            vint->visualid!=display->screens->root_visual->visualid)
		status=0;
	if ((vinm&VisualScreenMask|vinm==VisualAllMask)&&
	    vint->screen!=0)
		status=0;
	if ((vinm&VisualDepthMask|vinm==VisualAllMask)&&
	    vint->depth!=24)
		status=0;
	if ((vinm&VisualClassMask|vinm==VisualAllMask)&&
	    vint->class!=display->screens->root_visual->class)
		status=0;
	if ((vinm&VisualRedMaskMask|vinm==VisualAllMask)&&
	    vint->red_mask!=display->screens->root_visual->red_mask)
		status=0;
	if ((vinm&VisualGreenMaskMask|vinm==VisualAllMask)&&
	    vint->green_mask!=display->screens->root_visual->green_mask)
		status=0;
	if ((vinm&VisualBlueMaskMask|vinm==VisualAllMask)&&
	    vint->blue_mask!=display->screens->root_visual->blue_mask)
		status=0;
	if ((vinm&VisualColormapSizeMask|vinm==VisualAllMask)&&
	    vint->colormap_size!=display->screens->root_visual->map_entries)
		status=0;
	if ((vinm&VisualBitsPerRGBMask|vinm==VisualAllMask)&&
	    vint->bits_per_rgb!=display->screens->root_visual->bits_per_rgb)
		status=0;
	if (status==1)
	{
		xvi.visualid=display->screens->root_visual->visualid;
		xvi.screen=0;
		xvi.depth=display->screens->root_visual->bits_per_rgb;
		xvi.class=display->screens->root_visual->class;
		xvi.red_mask=display->screens->root_visual->red_mask;
		xvi.green_mask=display->screens->root_visual->green_mask;
		xvi.blue_mask=display->screens->root_visual->blue_mask;
		xvi.colormap_size=display->screens->root_visual->map_entries;
		xvi.bits_per_rgb=display->screens->root_visual->bits_per_rgb;
		xvi.visual=display->screens->root_visual;
		*n=1;
		return (&xvi);
	}
	*n=0;
	return (&xvi);
}

Status XMatchVisualInfo(
    Display*		display,
    int			screen,
    int			depth,
    int			class,
    XVisualInfo*	vinfo_return)
{
	int status=0;
	xtrace("XMatchVisualInfo\n");
	return status;
}

/*****************************************************************\

	Function: XClearWindow
	Inputs:   display, window

	Comments: As mentioned, the Window structure is not the one windows
		  recognises.  The background colour for the window is
		  stored in this structure.

		  The sequence of GetDC, CreateBrush/Pen, SelectObject,
		  <draw stuff>, DeleteObject, ReleaseDC occurs in all the
		  drawing functions.

\*****************************************************************/
int
XClearWindow(display, w)
Display *display;
Window w;
{
	RECT  rRect;
	HBRUSH hbrush;
	HDC hDC;
	HANDLE oldObj;
	int oldROP;
	NT_window *window = (NT_window *)w;
	
	xtrace("XClearWindow\n");
	rRect.left= rRect.right=rRect.bottom=rRect.top =0;
	
	hDC = cjh_get_dc(window->w);
	oldROP = SetROP2(hDC,R2_COPYPEN);
	hbrush = CreateSolidBrush(CNUMTORGB(window->bg));
	oldObj = SelectObject(hDC, hbrush);
	GetClientRect(window->w, &rRect);
	FillRect(hDC, &rRect, hbrush);
	SelectObject(hDC, oldObj);
	DeleteObject(hbrush);
	SetROP2(hDC,oldROP);
	cjh_rel_dc(window->w,hDC);
	return 0;
}

int
XSetWindowBackground(display, window, pixmap)
	 Display *display;
	 NT_window *window;
	 Pixmap pixmap;
{
	xtrace("XSetWindowBackground\n");
	return 0;
}

	

/*****************************************************************\

	Function: XCreateSimpleWindow
	Inputs:   display, parent window, geometry, border width,
		  border colour, background colour.
	Returned: Window ID

	Comments: The first time a window is made by the application, it
		  has to be registered.
		  To simulate the action of a window manager, the toplevel
		  client window is reparented and a frame window is created.
		  A StructureNotify event is sent to the new client.
		  Note that child windows are created in the manner of the
		  default X behaviour, ie. each is clipped individually.


        NOTE:     This routine has now changed. As part of our role as
                  Window manager, we now defer creation of the windows until
                  they are mapped. The fact that a window has been created
                  and not mapped is flagged to other routines by setting the
                  w element of the structure to -1.
                  WE STILL CREATE THE Window STRUCTURES.
                  (SEE XMapWindow)

\*****************************************************************/

Window
XCreateSimpleWindow(display, parent,x, y, w, h, brd, brd_col, bg)
Display *display;
NT_window * parent;
int     x, y;
unsigned int brd,w,h;
unsigned long bg, brd_col;
{
	WNDCLASS  wc;
	NT_window  *canvas, *outer;
	HANDLE    curInstance = hInstance;
	HBRUSH    hbrush;
	HANDLE    prevInstance = hPrevInstance;
	static int first = 1;
	xtrace("XCreateSimpleWindow\n");
	windowBg = bg;
	if (!prevInstance  && first)
	{
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; /* CS_OWNDC */
		wc.lpfnWndProc = (WNDPROC)MainWndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = curInstance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);

		wc.hbrBackground = hbrush = CreateSolidBrush (CNUMTORGB(bg));
		wc.lpszMenuName =  NULL;
		wc.lpszClassName = app_name;
		RegisterClass(&wc);
		/*		if (!RegisterClass(&wc))
				return NULL;     */
		first = 0;
	}

	if ( parent->w == GetDesktopWindow() )
	{
		outer = NT_new_window_pair(&canvas);
		outer->client = canvas;
		outer->bg = bg;
		outer->parent = parent;
		outer->frame  = NULL;
		NT_add_child(outer,canvas);
		NT_add_child(parent,outer);
		canvas->frame = outer;
		outer->x = x;
		outer->y = y;
		outer->wdth = w;
		outer->hght = h;
		canvas->x = -1;
		canvas->y = -1;
	}
	else
	{
		canvas = NT_new_window();
		outer = parent;
		canvas->frame = NULL;
		canvas->x = x;
		canvas->y = y;
		canvas->wdth = w;
		canvas->hght = h;
		NT_add_child(parent,canvas);
	}
	canvas->bg=bg;
	canvas->parent=outer;
	canvas->client=NULL;
	canvas->title_text = NULL;
	canvas->props = NULL;
	if (outer!=parent)
		outer->client=canvas;

	return ((Window)canvas);
}


/*****************************************************************\

	Function: XCreateWindow
	Inputs:   display, parent window, geometry, border width, depth,
		  class, visual, attributes mask, attributes structure
	Returned: Window ID

	Comments: Simply calls XCreateSimpleWindow.  Some of the arguments
		  are ignored :-).

\*****************************************************************/

Window
XCreateWindow(display,parent,x,y,width,height,bw,depth,class,visual,
			valuemask,attr)
Display *display;
Window  parent;
int x,y;
unsigned int width,height,bw;
int depth;
unsigned int class;
Visual *visual;
unsigned long valuemask;
XSetWindowAttributes *attr;
{
	xtrace("XCreateWindow\n");
	return (XCreateSimpleWindow(display,parent,x,y,width,height,bw,
		attr->border_pixel,attr->background_pixel));
}


/*****************************************************************\

	Function: XDestroyWindow
	Inputs:   Display, window to be destroyed.

	Comments: Removes a window from the server.  Some fiddling is needed
		  to remove frames that we have made as well as client
		  windows.

\*****************************************************************/
int
XDestroyWindow(display,w)
Display *display;
NT_window *w;
{
	xtrace("XDestroyWindow\n");
        if (w->hDC != INVALID_HANDLE)
          cjh_rel_dc(w->w,w->hDC);
	/*DestroyWindow(w->w);*/
        if ( w->frame != NULL )
	  {
          if (w->frame->hDC != INVALID_HANDLE)
            cjh_rel_dc(w->frame->w,w->frame->hDC);
          delFocusWindow(w->frame);
          activateTopWin();
	  /*DestroyWindow(w->frame->w);*/
	  NT_delete_window(w->frame);
	  }
	NT_delete_window(w);   /* Remove window from data structure */
	return 0;
}


/*****************************************************************\

	Function: XGetGeometry
	Inputs:   display, window
	Returned: root window, screen depth, geometry, border width

	Comments: fetches information from the windows kernel and our
		  display structure.

\*****************************************************************/

Status
XGetGeometry(display,w,root,x,y,width,height,bw,depth)
Display *display;
Drawable w;
NT_window **root;
int *x,*y;
unsigned int *width,*height;
unsigned int *bw,*depth;
{
	RECT r;
	xtrace("XGetGeometry\n");

	*root=(NT_window*)display->screens[0].root;
	*depth=24;

	GetWindowRect(w->w,&r);
	*x=r.left;
	*y=r.top;
	*width=r.right-r.left;
	*height=r.bottom-r.top;
	GetClientRect(w->w,&r);
	*bw=(*width-(r.right-r.left))/2;
	return 0;
}


/*****************************************************************\

	Function: XGetWindowAttributes
	Inputs:   display, window, attributes
	Returned: 1 = ok.

	Comments: Fills out attributes structure.

\*****************************************************************/

Status
XGetWindowAttributes(display,w,wattr)
Display *display;
Window w;
XWindowAttributes *wattr;
{
	xtrace("XGetWindowAttributes\n");
	XGetGeometry(display,w,&wattr->root,&wattr->x,&wattr->y,&wattr->width,
		&wattr->height,&wattr->border_width,&wattr->depth);
	wattr->class=InputOutput;
	wattr->bit_gravity=StaticGravity;
	wattr->win_gravity=CenterGravity;
	wattr->backing_store=NotUseful;
	wattr->backing_planes=0;
	wattr->backing_pixel=0;
	wattr->save_under=False;
	wattr->colormap=None;
	wattr->map_installed=True;
	wattr->map_state=IsViewable;
/*	wattr->all_event_masks=
	wattr->your_event_mask=
	wattr->do_not_propagate_mask=    */
	wattr->override_redirect=False;
	wattr->screen=display->screens;
	return (1);
}



int
XSelectInput(display, window, mask)
Display *display;
Window  window;
long    mask;
{
	xtrace("XSelectInput\n");
/***** Depends on event structure -- not fixed yet *****/
	return 0;
}

DWORD mwmHintsToFrameStyle(hints)
MwmHints *hints;
{

  DWORD frame_style = WS_CLIPCHILDREN;
  
  if (hints == NULL) return frame_style;
  
  if (hints->decorations & MWM_DECOR_BORDER)
  {
    frame_style |=  WS_BORDER;
  }

  if (hints->decorations & MWM_DECOR_RESIZEH)
  {
    frame_style |= WS_THICKFRAME;
  }
  if (hints->decorations & MWM_DECOR_TITLE)
  {
    frame_style |= WS_CAPTION;
  }
  else
  {
    frame_style |= WS_POPUP;
  }
  if (hints->decorations & MWM_DECOR_MENU)
  {
    frame_style |= WS_SYSMENU;
  }
  if (hints->decorations & MWM_DECOR_MINIMIZE)
  {
    frame_style |= WS_MINIMIZEBOX;
  }
  if (hints->decorations & MWM_DECOR_MAXIMIZE)
  {
    frame_style |= WS_MAXIMIZEBOX;
  }
  return frame_style;
}

void NT_dispError(char *msg)
{
	LPVOID lpMsgBuf=NULL;
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,
				  GetLastError(),
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR) &lpMsgBuf,
				  0,
				  NULL);
	MessageBox( NULL, lpMsgBuf, msg, MB_OK|MB_ICONINFORMATION );
	LocalFree( lpMsgBuf );
}

/*****************************************************************\

	Function: XMapWindow
	Inputs:   display, window to be mapped

	Comments: If the specified window is not already mapped, this
		  routine calls the Windows function which displays it.
		  Again, frames have to be mapped as well as clients.

\*****************************************************************/
int
XMapWindow(display, window)
Display *display;
NT_window *window;
{
	RECT rect;
	HANDLE curInstance = hInstance;
	unsigned char *hints;
	Atom property;
	Atom ret_type;
	int ret_format;
	DWORD frame_style;
	long ret_nitems;
	long ret_after;
	HDC hDC;
	xtrace("XMapWindow\n");

	property = XInternAtom(d,_XA_MWM_HINTS,TRUE);

	if (window->frame != NULL )
	{
		if (XGetWindowProperty(d,window,property,0L,0L,
							   FALSE,property,&ret_type,&ret_format,
							   &ret_nitems,&ret_after,&hints) == 1)
		{
            frame_style = mwmHintsToFrameStyle(hints);
		}
		else
        {
            frame_style = WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN;
		}

		if (window->free_flag == NT_NOT_REPROPED)
		{
            cjh_printf("Redoing the properties of window %x with HWND %x\n",window,window->frame->w);
            window->frame->free_flag = NT_USED;
            SetWindowLong(window->frame->w,GWL_STYLE,frame_style);

            if((frame_style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW)
				window->frame->hght+=NT_TITLE_BORDER_SIZE;
            else if ((frame_style & WS_CAPTION) == WS_CAPTION)
				window->frame->hght+=NT_TITLE_BORDER_SIZE;
            else
				window->frame->hght += NT_BORDER_SIZE;
            window->frame->wdth+=8;

			if ( window->frame->wdth > 250 )
				window->frame->top_flag = FALSE;
			else
				window->frame->top_flag = TRUE;

            SetWindowPos(window->frame->w,HWND_TOP,window->frame->x,
                         window->frame->y,window->frame->wdth,
                         window->frame->hght,SWP_NOACTIVATE);
            addFocusWindow(window->frame);
		}




		if ( window->frame->w == INVALID_HANDLE )
		{
			if((frame_style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW)
                window->frame->hght+=NT_TITLE_BORDER_SIZE;
			else if ((frame_style & WS_CAPTION) == WS_CAPTION)
                window->frame->hght+=NT_TITLE_BORDER_SIZE;
			else
                window->frame->hght += NT_BORDER_SIZE;

			window->free_flag = NT_USED;
			window->frame->wdth+=8;

			if ( window->frame->wdth > 250 )
				window->frame->top_flag = FALSE;
			else
				window->frame->top_flag = TRUE;

			window->frame->hDC = INVALID_HANDLE;
			window->frame->w = CreateWindow(app_name,"",frame_style,
											window->frame->x,window->frame->y,
											window->frame->wdth,window->frame->hght,
											NULL,NULL,curInstance,NULL);
			if (window->frame->w==NULL) NT_dispError("create window frame");
			
			/*window->frame->hDC = cjh_get_dc(window->frame->w);*/
			if (window->title_text != NULL)
                (void) SetWindowText(window->frame->w,window->title_text);
			addFocusWindow(window->frame);
			cjh_printf("Created Window Frame %x with handle %x\n",window->frame,window->frame->w);
		}

		if ( IsWindowVisible(window->frame->w)==0)
		{

			ShowWindow(window->frame->w, SW_SHOW);
			UpdateWindow(window->frame->w);
		}
		NT_OrderFrame(window->frame, TRUE);
	}

	if (window->x == -1 && window->y == -1)
	{
		GetClientRect(window->frame->w,&rect);
		window->x = rect.left;
		window->y = rect.top;
		window->wdth = rect.right-rect.left;
		window->hght = rect.bottom - rect.top;
	}

	if (window->free_flag == NT_NOT_REPROPED)
	{
		window->free_flag = NT_USED;
		SetWindowPos(window->w,HWND_TOP,window->x,
					 window->y,window->wdth,
					 window->hght,SWP_NOACTIVATE);
	}

	if (window->w == INVALID_HANDLE)
	{
		window->hDC = INVALID_HANDLE;
		
		if (window->parent->w == INVALID_HANDLE)
		{
			XMapWindow(display, window->parent);
		}
		
		window->w = CreateWindow(app_name,"",
								 WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
								 window->x,window->y,window->wdth,window->hght,
								 window->parent->w,NULL,curInstance,NULL);
		if (window->w==NULL) NT_dispError("create window1");
		cjh_printf("Created Window %x with handle %x %d %d\n",window,window->w,window->wdth,window->hght);
		hDC = cjh_get_dc(window->w);
		PostMessage(window->w,USR_StructureNotify,0,0);
	}
	if (IsWindowVisible(window->w)==0)
	{
		ShowWindow(window->w, SW_SHOW);
		UpdateWindow(window->w);
		/*		XClearWindow(display,window); */
		PostMessage(window->w,USR_StructureNotify,0,0);
	}
	GetWindowRect(window->w,&rect);
	return 0;
}


/*****************************************************************\

	Function: XCreateGC
	Inputs:   display, window, mask of setup values, setup values.
	Returned: GC pointer.

	Comments: Fills out a GC structure with the X defaults unless
		  the caller specifies otherwise.

\*****************************************************************/

GC
XCreateGC(display, window, mask, gc_values)
Display *display;
Window window;
unsigned long mask;
XGCValues *gc_values;
{
	GC	local_gc;
	int	size;
	char	*ptr;
	xtrace("XCreateGC\n");

	size = sizeof(GC);

	ptr = (char *)allocateMemory((size_t)1000);
	local_gc = (GC)ptr;
	local_gc->ext_data = NULL;
	local_gc->gid=(GContext) window;
	local_gc->rects=FALSE;
	local_gc->dashes=FALSE;

	if (mask&GCArcMode)
		local_gc->values.arc_mode=gc_values->arc_mode;
	else
		local_gc->values.arc_mode=ArcPieSlice;
	if (mask&GCBackground)
		local_gc->values.background=gc_values->background;
	else
		local_gc->values.background=display->screens->white_pixel;
	if (mask&GCCapStyle)
		local_gc->values.cap_style=gc_values->cap_style;
	else
		local_gc->values.cap_style=CapButt;
	if (mask&GCClipMask)
		local_gc->values.clip_mask=gc_values->clip_mask;
	else
		local_gc->values.clip_mask=None;
	if (mask&GCClipXOrigin)
		local_gc->values.clip_x_origin=gc_values->clip_x_origin;
	else
		local_gc->values.clip_x_origin=0;
	if (mask&GCClipYOrigin)
		local_gc->values.clip_y_origin=gc_values->clip_y_origin;
	else
		local_gc->values.clip_y_origin=0;
	if (mask&GCDashList)
		local_gc->values.dashes=gc_values->dashes;
	else
		local_gc->values.dashes=4;
	if (mask&GCDashOffset)
		local_gc->values.dash_offset=gc_values->dash_offset;
	else
		local_gc->values.dash_offset=0;
	if (mask&GCFillRule)
		local_gc->values.fill_rule=gc_values->fill_rule;
	else
		local_gc->values.fill_rule=EvenOddRule;
	if (mask&GCFillStyle)
		local_gc->values.fill_style=gc_values->fill_style;
	else
		local_gc->values.fill_style=FillSolid;
	if (mask&GCFont)
		local_gc->values.font=gc_values->font;
	else
		local_gc->values.font= 999;/*"fixed";*/
	if (mask&GCForeground)
		local_gc->values.foreground=gc_values->foreground;
	else
		local_gc->values.foreground=display->screens->black_pixel;
	if (mask&GCFunction)
		local_gc->values.function=gc_values->function;
	else
		local_gc->values.function=GXcopy;
	if (mask&GCGraphicsExposures)
		local_gc->values.graphics_exposures=gc_values->graphics_exposures;
	else
		local_gc->values.graphics_exposures=True;
	if (mask&GCJoinStyle)
		local_gc->values.join_style=gc_values->join_style;
	else
		local_gc->values.join_style=JoinMiter;
	if (mask&GCLineStyle)
		local_gc->values.line_style=gc_values->line_style;
	else
		local_gc->values.line_style=LineSolid;
	if (mask&GCLineWidth)
		local_gc->values.line_width=gc_values->line_width;
	else
		local_gc->values.line_width=0;
	if (mask&GCPlaneMask)
		local_gc->values.plane_mask=gc_values->plane_mask;
	else
		local_gc->values.plane_mask=255;
	if (mask&GCStipple)
		local_gc->values.stipple=gc_values->stipple;
	/*	else
		local_gc->values.stipple=XCreatePixmap(display,window,8,8,8);  */
	if (mask&GCSubwindowMode)
		local_gc->values.subwindow_mode=gc_values->subwindow_mode;
	else
		local_gc->values.subwindow_mode=ClipByChildren;
	if (mask&GCTile)
		local_gc->values.tile=gc_values->tile;
	/*	else
		local_gc->values.tile=XCreatePixmap(display,window,8,8,8);  */
	if (mask&GCTileStipXOrigin)
		local_gc->values.ts_x_origin=gc_values->ts_x_origin;
	else
		local_gc->values.ts_x_origin=0;
	if (mask&GCTileStipYOrigin)
		local_gc->values.ts_y_origin=gc_values->ts_y_origin;
	else
		local_gc->values.ts_y_origin=0;

	local_gc->dirty = 0xFFFF;

	return (local_gc);
}


/*****************************************************************\

	Function: XSetForeground
	Inputs:   display, gc, colour.

	Comments: Colour is an RGB triple (24bits).
\*****************************************************************/
int
XSetForeground(display, gc, color)
Display *display;
GC gc;
unsigned long    color;
{
	xtrace("XSetForegrond\n");
	gc->values.foreground=color;
	gc->dirty |= GCForeground;
	return 0;
}


/*****************************************************************\

	Function: XDrawString
	Inputs:   display, window, gc, position, string, length of string.

	Comments: Writes text to the screen in the manner of X windows.
		  Note that the y origin is on the text baseline, ie.
		  the lowest part of a letter o rests on the baseline and
		  descenders fall below it.
		  The text is transparent.

\*****************************************************************/

int
XDrawString(Display *display, Drawable window,
    GC gc, int x, int y, const char* str, int len)
{
	HDC          hDC;
	TEXTMETRIC   tmet;
	xtrace("XDrawString\n");

	hDC = cjh_get_dc(window->w);
	SetBkMode(hDC,TRANSPARENT);
	SetTextColor(hDC,CNUMTORGB(gc->values.foreground));
	GetTextMetrics(hDC,&tmet);
	TextOut(hDC,x,y-tmet.tmAscent,str,len);
	cjh_rel_dc(window->w,hDC);
	return 0;
}



int
XDrawString16(Display *display, Drawable window,
    GC gc, int x, int y,
    const XChar2b* str,
    int len)
{
	xtrace("XDrawString16\n");
	return 0;
}


/*****************************************************************\

	Function: XFillRectangle
	Inputs:   display, window, gc, geometry.

	Comments: fills rectangles in uniform colours.  No tiles/Pixmaps
		  are implemented yet.

\*****************************************************************/

int
XFillRectangle (display,window,gc,x,y,w,h)
Display *display;
NT_window *window;
GC gc;
int x,y;
unsigned int w,h;
{
	RECT rct;
	HBRUSH hbrush;
	HDC hDC;
	HANDLE oldObj;
	int ret;
	xtrace("XFillRectangle\n");

	hDC = cjh_get_dc(window->w);
	NT_set_rop(hDC,gc);
	hbrush = NT_get_GC_brush(hDC,gc);
	oldObj = SelectObject(hDC, hbrush);
	rct.left=(LONG) x;
	rct.right=(LONG) (x+w);
	rct.top=(LONG) y;
	rct.bottom=(LONG) (y+h);
	ret=FillRect(hDC, &rct, hbrush);
	cjh_rel_dc(window->w,hDC);
	SelectObject(hDC, oldObj);
	return (ret);
}


/*****************************************************************\

	Function: XClearArea
	Inputs:   display, geometry, exposure events allowed.

	Comments: Straightforward.

\*****************************************************************/
int
XClearArea(display,w,x,y,width,height,exposures)
Display *display;
NT_window *w;
int x,y;
unsigned int width,height;
Bool exposures;
{
	RECT rct;
	HBRUSH hbrush;
	HDC hDC;
	HANDLE oldObj;
	int oldROP;
	xtrace("XClearArea\n");

	hDC = cjh_get_dc(w->w);
	oldROP = SetROP2(hDC,R2_COPYPEN);
	hbrush=CreateSolidBrush(CNUMTORGB(w->bg));
	oldObj = SelectObject(hDC,hbrush);
	GetClientRect(w->w,&rct);

	if (width!=0)
	{
		rct.left=(LONG)x;
		rct.right=(LONG)(x+width+1);
	}

	if (height !=0)
	{
		rct.top=(LONG)y;
		rct.bottom=(LONG)(y+height + 1);
	}

	FillRect(hDC,&rct,hbrush);
	SelectObject(hDC, oldObj);
	DeleteObject(hbrush);
	SetROP2(hDC,oldROP);
	cjh_rel_dc(w->w,hDC);
	return 0;
}


Region
XCreateRegion()
{
	HRGN hrgn;
	xtrace("XCreateRegion\n");

	hrgn=CreateRectRgn(0,0,1,1);
	return (hrgn);
}


/* Untested.  The Region stuff needs thinking about. */

int
XClipBox(hrgn,rect)
Region hrgn;
XRectangle *rect;
{
	RECT rct;
	xtrace("XClipBox\n");

	GetRgnBox(hrgn,&rct);
	rect->x=(short)rct.left;
	rect->y=(short)rct.top;
	rect->width=(unsigned short)(rct.right-rct.left);
	rect->height=(unsigned short)(rct.bottom-rct.top);
	return TRUE;/*(rect);*/
}


int
XSetRegion(display,gc,hrgn)
Display *display;
GC gc;
Region hrgn;
{
	/* What to do here ? */
	xtrace("XSetRegion\n");
	return 0;
}


int
XDestroyRegion(hrgn)
Region hrgn;
{
	xtrace("XDestroyRegion\n");
	DeleteObject(hrgn);
	return 0;
}


int
XUnionRectWithRegion(rect,hrgnsrc,hrgndest)
XRectangle *rect;
Region hrgnsrc,hrgndest;
{
	HRGN temp;
	xtrace("XUnionRectWithRegion\n");
	temp=CreateRectRgn(rect->x,rect->y,rect->x+rect->width,
				rect->y+rect->height);
	CombineRgn(hrgndest,hrgnsrc,temp,RGN_OR);
	return 0;
}


/*****************************************************************\

	Function: XDrawArc
	Inputs:   display, window, gc, bounding box geometry, arc angles.

	Comments: Works fine.

\*****************************************************************/

int
XDrawArc(display,w,gc,x,y,width,height,a1,a2)
Display *display;
NT_window *w;
GC gc;
int x,y;
unsigned int width,height;
int a1,a2;
{
	HDC hDC;
	HPEN hpen;
	int tmp;
	double NT_deg64_to_rad();
	HANDLE oldObj;
	xtrace("XDrawArc\n");
	if (a2>=0)
		a2+=a1;
	else
	{
		tmp=a1;
		a1-=a2;
		a2=tmp;
	}

	hDC = cjh_get_dc(w->w);
	hpen = NT_get_GC_pen(hDC,gc);
	oldObj = SelectObject(hDC,hpen);
	Arc(hDC,x,y,x+width-1,y+height-1,
		(int) (x+width/2+width*cos(NT_deg64_to_rad(a1))),
		(int) (y+height/2-height*sin(NT_deg64_to_rad(a1))),
		(int) (x+width/2+width*cos(NT_deg64_to_rad(a2))),
		(int) (y+height/2-height*sin(NT_deg64_to_rad(a2))));
	SelectObject(hDC, oldObj);
	cjh_rel_dc(w->w,hDC);
	return 0;
}


/*****************************************************************\

	Function: XFillArc
	Inputs:   display, window, gc, geometry as above.

	Comments: Not tested at all, but should work.

\*****************************************************************/

int
XFillArc(display,w,gc,x,y,width,height,a1,a2)
Display *display;
NT_window *w;
GC gc;
int x,y;
unsigned int width,height;
int a1,a2;
{
	HDC hDC;
	HBRUSH hbrush;
	int tmp;
        HANDLE oldObj;
		xtrace("XFillArc\n");
	if (a2>=0)
		a2+=a1;
	else
	{
		tmp=a1;
		a1-=a2;
		a2=tmp;
	}
        hDC = cjh_get_dc(w->w);
	hbrush = NT_get_GC_brush(hDC,gc);
	oldObj = SelectObject(hDC,hbrush);
	if (gc->values.arc_mode==ArcChord)
	{
		Chord(hDC,x,y,x+width,y+height,
			(int) (x+width/2+width*cos(NT_deg64_to_rad(a1))),
			(int) (y+height/2+height*sin(NT_deg64_to_rad(a1))),
			(int) (x+width/2+width*cos(NT_deg64_to_rad(a2))),
			(int) (y+height/2+height*sin(NT_deg64_to_rad(a2))));
	}
	else
	{
		Pie(hDC,x,y,x+width,y+height,
			(int) (x+width/2+width*cos(NT_deg64_to_rad(a1))),
			(int) (y+height/2+height*sin(NT_deg64_to_rad(a1))),
			(int) (x+width/2+width*cos(NT_deg64_to_rad(a2))),
			(int) (y+height/2+height*sin(NT_deg64_to_rad(a2))));
	}
    SelectObject(hDC, oldObj);
    cjh_rel_dc(w->w,hDC);
	return 0;
}


/*****************************************************************\

	Function: XFillPolygon
	Inputs:   display, window, gc, points list, number of points,
		  shape hint, relative drawing mode.

	Comments: Works for convex polygons.  Untested on otherwise.
	          Optimisation hints are unused, as is the mode.

\*****************************************************************/

int
XFillPolygon(display,w,gc,points,nps,shape,mode)
Display *display;
NT_window *w;
GC gc;
XPoint *points;
int nps,shape,mode;
{
	HBRUSH hbrush;
	int n;
	POINT ntps[1000];
	HDC hDC;
	HANDLE oldObj;
	xtrace("XFillPolygon\n");
 	/*ntps=allocateMemory(sizeof(POINT)*nps);*/
	hDC = cjh_get_dc(w->w);
	hbrush = NT_get_GC_brush(hDC,gc);
	oldObj = SelectObject(hDC,hbrush);
	for (n=0;n<nps;++n)
	{
		(ntps+n)->x=(LONG)points->x;
		(ntps+n)->y=(LONG)(points++)->y;
	}
	Polygon(hDC,ntps,nps);
	SelectObject(hDC, oldObj);
	cjh_rel_dc(w->w,hDC);
	/*free(ntps);*/
	return 0;
}


/*****************************************************************\

	Function: XDrawLine
	Inputs:   display, window, geometry.

	Comments: Seems to work ok.

\*****************************************************************/

int
XDrawLine(display,w,gc,x1,y1,x2,y2)
Display *display;
NT_window *w;
GC gc;
int x1,y1,x2,y2;
{
	HDC hDC;
	HPEN hpen;
	RECT da;
	HANDLE oldObj;
	xtrace("XDrawLine\n");
	GetClientRect(w->w,&da);

	x1 = ( x1 < 0         )   ? 0          : x1;
	x1 = ( x1 > da.right  )   ? da.right   : x1;
	y1 = ( y1 < 0         )   ? 0          : y1;
	y1 = ( y1 > da.bottom )   ? da.bottom  : y1;
	x2 = ( x2 < 0         )   ? 0          : x2;
	x2 = ( x2 > da.right  )   ? da.right   : x2;
	y2 = ( y2 < 0         )   ? 0          : y2;
	y2 = ( y2 > da.bottom )   ? da.bottom  : y2;

	hDC = cjh_get_dc(w->w);
	hpen = NT_get_GC_pen(hDC,gc);
	oldObj = SelectObject(hDC,hpen);
	MoveToEx(hDC,x1,y1,NULL);
	LineTo(hDC,x2,y2);
	SelectObject(hDC, oldObj);
	cjh_rel_dc(w->w,hDC);
	return 0;
}


/*****************************************************************\

	Function: XDrawLines
	Inputs:   display, window, gc, points list, number of points, mode.

	Comments: Untested.

\*****************************************************************/

int
XDrawLines(display,w,gc,points,nps,mode)
Display *display;
NT_window *w;
GC gc;
XPoint *points;
int nps,mode;
{
	HPEN hpen;
	int n;
	POINT pts[1000];
	HDC hDC;
	HANDLE oldObj;
	xtrace("XDrawLines\n");
	/*pts=(POINT *) allocateMemory(sizeof(POINT)*nps);*/
	pts->x=(LONG)points->x;
	pts->y=(LONG)points->y;

	for(n=1;n<nps;++n)
		if (mode==CoordModeOrigin)
		{
			(pts+n)->x=(LONG)(points+n)->x;
			(pts+n)->y=(LONG)(points+n)->y;
		}
		else
		{
			(pts+n)->x=(LONG)(points+n)->x+(pts+n-1)->x;
			(pts+n)->y=(LONG)(points+n)->y+(pts+n-1)->y;
		}

	hDC = cjh_get_dc(w->w);
	hpen = NT_get_GC_pen(hDC,gc);
	oldObj = SelectObject(hDC,hpen);
	Polyline(hDC,pts,nps);
	SelectObject(hDC, oldObj);
	cjh_rel_dc(w->w,hDC);
	/*free(pts);*/
	return 0;
}


/*****************************************************************\

	Function: XDrawPoints
	Inputs:   display, window, gc, points list, number of points, mode.

	Comments: Untested.

\*****************************************************************/

int
XDrawPoints(display,w,gc,points,nps,mode)
Display *display;
NT_window *w;
GC gc;
XPoint *points;
int nps,mode;
{
	HDC hDC;
	int n;
	xtrace("XDrawPoints\n");
	hDC = cjh_get_dc(w->w);
	SetPixelV(hDC,points->x,points->y,CNUMTORGB(gc->values.foreground));
	for (n=1;n<nps;++n)
	{
		if (mode==CoordModeOrigin)
			SetPixelV(hDC,(points+n)->x,(points+n)->y,
					  CNUMTORGB(gc->values.foreground));
		else
			SetPixelV(hDC,(points+n-1)->x+(points+n)->x,
					  (points+n-1)->y+(points+n)->y,
					  CNUMTORGB(gc->values.foreground));
	}
	cjh_rel_dc(w->w,hDC);
	return 0;
}
int
XDrawPoint(display,w,gc,x,y)
Display *display;
NT_window *w;
GC gc;
int x,y;
{
	HDC hDC;
	xtrace("XDrawPoint\n");
	hDC = cjh_get_dc(w->w);
	SetPixelV(hDC,x,y,CNUMTORGB(gc->values.foreground));
	cjh_rel_dc(w->w,hDC);
	return 0;
}


/*****************************************************************\

	Function: XDrawRectangle
	Inputs:   display, window, gc, geometry

	Comments: Seems to work.

\*****************************************************************/

int
XDrawRectangle(display,w,gc,x,y,width,height)
Display *display;
NT_window *w;
GC gc;
int x,y;
unsigned int width,height;
{
	HDC hDC;
	RECT rect;
	HBRUSH hbrush;
	HPEN hpen;
	HANDLE oldbrush, oldpen;
	xtrace("XDrawRectangle\n");
	hDC = cjh_get_dc(w->w);
	hbrush = NT_get_GC_brush(hDC,gc);
	rect.left=(LONG)x;
	rect.right=(LONG)(x+width);
	rect.top=(LONG)y;
	rect.bottom=(LONG)(y+height);
	oldbrush = SelectObject(hDC,GetStockObject(NULL_BRUSH));
	hpen = NT_get_GC_pen(hDC,gc);
	oldpen = SelectObject(hDC,hpen);

	Rectangle(hDC,(int)rect.left,(int)rect.top,(int)rect.right,(int)rect.bottom);
	/*
	  FrameRect(hDC,&rect,hbrush);
	  */
	SelectObject(hDC, oldbrush);
	SelectObject(hDC, oldpen);
	cjh_rel_dc(w->w,hDC);
	return 0;
}


/*****************************************************************\

	Function: XDrawSegments
	Inputs: display, window, gc, segment list, number of segments.

	Comments: Untested.

\*****************************************************************/

int
XDrawSegments(display,w,gc,segs,nsegs)
Display *display;
NT_window *w;
GC gc;
XSegment *segs;
int nsegs;
{
	HDC hDC;
	HPEN hpen;
	int n;
        HANDLE oldObj;
		xtrace("XDrawSegments\n");
        hDC = cjh_get_dc(w->w);
	hpen = NT_get_GC_pen(hDC,gc);
	oldObj = SelectObject(hDC,hpen);
        SetBkMode(hDC,TRANSPARENT);
	for (n=0;n<nsegs;n++)
	{
          MoveToEx(hDC,(segs+n)->x1,(segs+n)->y1,NULL);
          LineTo(hDC,(segs+n)->x2,(segs+n)->y2);
	}
    SelectObject(hDC, oldObj);
    cjh_rel_dc(w->w,hDC);
	return 0;
}


/*	Not needed for TrueColour */

Status
XAllocColor(display,cmap,xc)
Display *display;
Colormap cmap;
XColor *xc;
{
	xtrace("XAllocColor\n");
	cjh_printf("Calling alloc color %x %x %x %d.\n",xc->red,xc->green,xc->blue,nextFreeColour);
	/* RGB((BYTE) (xc->red>>8),(BYTE) (xc->green>>8),(BYTE) (xc->blue>>8)); */
	sysPal->palPalEntry[nextFreeColour].peRed = (BYTE) (xc->red>>8);
	sysPal->palPalEntry[nextFreeColour].peGreen = (BYTE) (xc->green>>8);
	sysPal->palPalEntry[nextFreeColour].peBlue = (BYTE) (xc->blue>>8);
	xc->pixel = nextFreeColour++;
	return 1;
}

int NT_XDestroyImage(ximage)
XImage *ximage;
{
  freeMemory(ximage->data);
  freeMemory(ximage);
  return 1;
}


XImage
*XGetImage(display,w,x,y,width,height,plm,format)
Display *display;
Window w;
int x,y;
unsigned int width,height;
unsigned long plm;
int format;
{
	XImage *ret_image;
/*	RECT clientRect; */
/*	HDC  hdcCompatible, hdcWindow; */
/*	unsigned char *bigBitData; */
	unsigned char *smallBitData,*smallBitBase;
/*	HBITMAP hbmBitmap; */
	xtrace("XGetImage\n");
	/*
	  hdcWindow = cjh_get_dc(w->w);
	  GetClientRect(w->w,&clientRect);


	  hdcCompatible = CreateCompatibleDC(hdcWindow);
	  hbmBitmap = CreateCompatibleBitmap(hdcWindow,width,height);

	  /* Get the window's bitmap into our new "Compatible" bitmap
		 if (!SelectObject(hdcCompatible,hbmBitmap))
		 {
		 printf("Could not allocate X image structure.\n");
		 return NULL;
		 }
		 */
	if(!(ret_image = allocateMemory(sizeof(XImage))))
	{
		cjh_printf("Could not allocate X image structure.\n");
		return NULL;
	}

	ret_image->width = width;
	ret_image->height = height;
	ret_image->xoffset = 0;
	ret_image->format = ZPixmap;

	if (!(ret_image->data = allocateMemory(width*height*2)))
	{
		cjh_printf("Failed to allocate data area for XImage.\n");
		return NULL;
	}
	if (!(smallBitData = smallBitBase = allocateMemory(width*height*3)))
	{
		cjh_printf("Failed to allocate data area for 24 Bit image.\n");
		return NULL;
	}


	ret_image->byte_order = LSBFirst;
	ret_image->bitmap_unit = 8;
	ret_image->bitmap_bit_order = MSBFirst;
	ret_image->bitmap_pad = 0;
	ret_image->depth = 8;
	ret_image->bytes_per_line = width;
	ret_image->bits_per_pixel = 8;
	ret_image->f.destroy_image = NT_XDestroyImage;

	/* Get the BITMAP info into the Image .
	   GetDIBits(hdcCompatible,hbmBitmap,0,height,smallBitData,&bmInfo,DIB_RGB_COLORS);

	   /* Copy the 24 Bit Pixmap to a 32-Bit one.
		  bigBitData = ret_image->data;
		  for (i = 0 ; i< width*height;i++)
		  {
          *bigBitData++ = *smallBitData++;
          *bigBitData++ = *smallBitData++;
          *bigBitData++ = *smallBitData++;
          *bigBitData++ = 0;
		  }


		  /* Free the Device contexts, and the Bitmap
			 freeMemory(smallBitBase);
			 DeleteDC(hdcCompatible);
			 DeleteObject(hbmBitmap);
			 cachedImageWindow = w;*/
	return ret_image;
}



XPutImage2(display,w,gc,image,sx,sy,dx,dy,width,height)
Display *display;
NT_window *w;
XImage *image;
GC gc;
int sx,sy,dx,dy;
unsigned int width,height;
{
	RECT clientRect;
	HDC  hdcPICompat, hdcWindow;
	HBITMAP hbmPICompat;
	BITMAPINFO *bmInfo;
	unsigned int i;
/*	unsigned char *smallBitData,*smallBitBase,*bigBitData; */
	WORD *indexWords;
	xtrace("XPutImage2\n");
	GetClientRect(w->w,&clientRect);
	/*printf("PutImage %d %d - %d %d | %d %d\n",sx,sy,dx,dy,width,height);*/

	PIWindow = w;

	hdcWindow = cjh_get_dc(w->w);
	hdcPICompat = CreateCompatibleDC(hdcWindow);
	hbmPICompat = CreateCompatibleBitmap(hdcWindow,width,height);
	bmInfo = allocateMemory(sizeof(BITMAPINFO)+(cmapSize-1)*sizeof(WORD)-sizeof(RGBQUAD));

	/*  printf("*Put Bitmap is %x\n",hbmBitmap);*/

	/* Get the window's bitmap into our new "Compatible" bitmap */
	if (!SelectObject(hdcPICompat,hbmPICompat))
	{
		cjh_printf("Failed to select comp bitmap into comp DC.\n");
		if (hdcPICompat)
            DeleteDC(hdcPICompat);
		if (hbmPICompat)
            DeleteObject(hbmPICompat);
		return 0;
	}

	/*if (!(smallBitData = smallBitBase = allocateMemory(width*height*2)))
	  {
	  printf("Failed to allocate data area for 24 Bit image.\n");
	  return NULL;
	  }
	  */

	/* Copy the 32 Bit Pixmap to a 24-Bit one.
	   bigBitData = image->data;
	   for (i = 0 ; i< width*height;i++)
	   {
	   *smallBitData++ = *bigBitData++;
	   }
	   */

	/* Setup the BITMAPINFOHEADER, for the equivilent of an X-Windows ZPixmap. */
	bmInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo->bmiHeader.biWidth = width;
	bmInfo->bmiHeader.biHeight = height;
	bmInfo->bmiHeader.biPlanes = 1;
	bmInfo->bmiHeader.biBitCount = 8;
	bmInfo->bmiHeader.biCompression = BI_RGB;
	bmInfo->bmiHeader.biSizeImage = width *height; /* Specify a 24 bit bitmap. */
	bmInfo->bmiHeader.biClrImportant = 0; /* All colours important. */
	bmInfo->bmiHeader.biClrUsed = 0;
	indexWords = (WORD *)bmInfo->bmiColors;
	for (i=0;i<cmapSize;i++)
        indexWords[i] = i;

	/* Put the XImage data into our compatible Bitmap. */
	SetDIBits(hdcPICompat,hbmPICompat,0,height,image->data/*smallBitBase*/,bmInfo,DIB_PAL_COLORS);

	/* Copy into the window. */
	(void)BitBlt(hdcWindow,dx,dy,width,height,hdcPICompat,sx,sy,SRCCOPY);

	/* Free the Device contexts, and the Bitmap */

	DeleteDC(hdcPICompat);
	DeleteObject(hbmPICompat);
	cjh_rel_dc(w->w,hdcWindow);
	/*       freeMemory(smallBitBase);*/
	freeMemory(bmInfo);
	PIWindow = NULL;
	/*for (i=0;i<width;i++)
	  printf("%x%x%x ",image->data[3*i],image->data[3*i+1],image->data[3*i+2]);
	  printf("\n");*/
}

int
XPutImage(display,w,gc,image,sx,sy,dx,dy,width,height)
Display *display;
NT_window *w;
XImage *image;
GC gc;
int sx,sy,dx,dy;
unsigned int width,height;
{
	RECT clientRect;
	unsigned int i,j;
	int toNextLine;
	unsigned char *smallBitData;
	unsigned char *bigBitData;
	xtrace("XPutImage\n");
	cjh_printf("XPutImage %d %d -> %d %d by %d and %d into %d %d\n",sx,sy,dx,dy,width,height,PIWidth,PIHeight);
	if (width==height && width==1)
	{
		return 0;
	}
	if (w != PIWindow)
	{
		putImagePart2();
		PIWindow = w;
		GetClientRect(PIWindow->w,&clientRect);
		PIWidth = clientRect.right - clientRect.left ;
		PIHeight = clientRect.bottom - clientRect.top;
		PIWidth += ((PIWidth%4) == 0)?0:(4-(PIWidth%4));
		if (!(smallBitBase = allocateMemory(PIWidth*PIHeight *2)))
		{
            cjh_printf("Failed to allocate data area for Bit image.\n");
            return 0;
		}
	}


	bigBitData = image->data;
	smallBitData = smallBitBase+(PIWidth*(PIHeight - dy -1) + dx);
	toNextLine = 2*(PIWidth);
	for (i = 0 ; i< height;i++)
	{
		for (j = 0;j<width;j++)
		{
            *smallBitData++ = *(bigBitData++);
		}
		smallBitData-=toNextLine;
	}

}


void putImagePart2()
{
	HDC hdcPIWindow;
	HDC hdcPICompat;
/*	HWND oldFocusWin; */
	HBITMAP hbmPICompat;
	BITMAPINFO *bmInfo;
/*	BITMAPINFOHEADER bmInfoHdr; */
	unsigned int i;
	WORD *indexWords;

	if (!smallBitBase)
		return;

	cjh_printf("Differences are %d %d\n",PIWindow->wdth-PIWidth,PIWindow->hght-PIHeight);
	bmInfo = allocateMemory(sizeof(BITMAPINFO)+(cmapSize-1)*sizeof(WORD)-sizeof(RGBQUAD));
	hdcPIWindow = cjh_get_dc(PIWindow->w);
	hdcPICompat = CreateCompatibleDC(hdcPIWindow);
	hbmPICompat = CreateCompatibleBitmap(hdcPIWindow,PIWidth,PIHeight);

	/*  Select the newly created Bitmap into the Compatible DC. */
	if (!SelectObject(hdcPICompat,hbmPICompat))
	{
		cjh_printf("Failed to select comp bitmap into comp DC.\n");
		return;
	}



	/* Setup the BITMAPINFOHEADER, for the equivilent of an X-Windows ZPixmap. */
	/*  GetDIBits(hdcPICompat,hbmPICompat,0,PIHeight,NULL,bmInfo,DIB_PAL_COLORS);
		printf("Dimensions are %x %x instead of %x %x\n",(int)bmInfo->bmiHeader.biWidth,
		(int)bmInfo->bmiHeader.biHeight,
		PIWidth,PIHeight);
		*/

	bmInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmInfo->bmiHeader.biWidth = PIWidth;
	bmInfo->bmiHeader.biHeight = PIHeight;
	bmInfo->bmiHeader.biPlanes = 1;
	bmInfo->bmiHeader.biBitCount = 8;
	bmInfo->bmiHeader.biCompression = BI_RGB;
	bmInfo->bmiHeader.biSizeImage = (PIWidth)* (PIHeight);
	bmInfo->bmiHeader.biClrImportant = 0; /* All colours important. */
	bmInfo->bmiHeader.biClrUsed = 0;

	indexWords = (WORD *)bmInfo->bmiColors;
	for (i=0;i<cmapSize;i++)
		indexWords[i] = i;



	/* Put the XImage data into our compatible Bitmap. */
	SetDIBits(hdcPICompat,hbmPICompat,0,PIHeight,smallBitBase,bmInfo,DIB_PAL_COLORS);


	/* Copy into the window. */
    (void)BitBlt(hdcPIWindow,0,0,PIWidth,PIHeight,hdcPICompat,0,0,SRCCOPY);

	/* Free the Device contexts, and the Bitmap */
	DeleteDC(hdcPICompat);
	DeleteObject(hbmPICompat);
	cjh_rel_dc(PIWindow->w,hdcPIWindow);
	freeMemory(smallBitBase);
	freeMemory(bmInfo);
	smallBitBase = NULL;
	PIWindow = NULL;
}


/*****************************************************************\

	Function: XSetFillStyle
	Inputs:   display, gc, fill style.

	Comments: ZZzzz...

\*****************************************************************/

int
XSetFillStyle(display,gc,fs)
Display *display;
GC gc;
int fs;
{
	xtrace("XSetFillStyle\n");
	gc->values.fill_style=fs;
	gc->dirty |= GCFillStyle;
	return 0;
}


int
XSetDashes(Display *display,
    GC gc, int dash_offset,
    const char * dash_list,
    int n)
{
	xtrace("XSetDashes\n");
	return 0;
}


Pixmap
XCreateBitmapFromData(Display *display,
    Drawable w, const char *data,
    unsigned int width, unsigned int height)
{
	xtrace("XCreateBitmapFromData\n");
	return 0;
}


int
XChangeWindowAttributes(display,w,vmask,attr)
Display *display;
Window w;
unsigned long vmask;
XSetWindowAttributes *attr;
{
	xtrace("XChangeWindowAttributes\n");
	return 0;
}


/*****************************************************************\

	Function: XLowerWindow
	Inputs:   display, window to be lowered.

	Comments: Make sure if the window has a frame that that gets lowered
		  too.

\*****************************************************************/

int
XLowerWindow(display,w)
Display *display;
NT_window *w;
{
	xtrace("XLowerWindow\n");
	SetWindowPos((HWND)w->w,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	if (w->frame!=NULL)
		SetWindowPos(w->frame->w,HWND_BOTTOM,0,0,0,0,
					SWP_NOMOVE|SWP_NOSIZE);
	return 0;
}


/*****************************************************************\

	Function: XMapRaised
	Inputs:   display, window.

	Comments: Frames get raised first.

\*****************************************************************/

int
XMapRaised(display,w)
Display *display;
NT_window *w;
{
	xtrace("XMapRaised\n");
	XMapWindow(display,w);
	if (w->frame!=NULL)
		SetWindowPos(w->frame->w,HWND_TOP,0,0,0,0,
					SWP_NOMOVE|SWP_NOSIZE);
	SetWindowPos(w->w,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	return 0;
}


/*****************************************************************\

	Function: XMapSubwindows
	Inputs:   display, window.

	Comments: Unfortunately, the optimisations normally made by
		  the X server are not made here.

\*****************************************************************/

int
XMapSubwindows(display,w)
Display *display;
NT_window *w;
{
        struct NT_child *tmp;

	xtrace("XMapSubWindows\n");
	tmp=w->child;
	while (tmp!=NULL)
	{
		XMapWindow(display,tmp->w);
		tmp=tmp->next;
        }
	return 0;
}


/*****************************************************************\

	Function: XQueryTree
	Inputs:   display, window.
	Returned: root window, parent window, list of children, status.

	Comments: Not fully implemented.  The child field is wrong.

\*****************************************************************/

Status
XQueryTree(display,w,root,parent,ch,n)
Display *display;
NT_window *w;
NT_window** root;
NT_window** parent;
NT_window*** ch;
unsigned int *n;
{
	Status status=1;

	xtrace("XQueryTree\n");
	*parent=w->parent;
	if (*parent==NULL)
		status=0;
	*root=(NT_window*)display->screens[0].root;
	*ch=NULL;
	*n=0;
	return (status);
}


/*****************************************************************\

	Function: XRaiseWindow
	Inputs:   display, window.

	Comments: Recursive raising of window and its children.

\*****************************************************************/

int
XRaiseWindow(display,w)
Display *display;
NT_window *w;
{
	struct NT_child *tmp;
	xtrace("XRaiseWindows\n");

	if (w->frame!=NULL)
		BringWindowToTop(w->frame->w);
	BringWindowToTop(w->w);
	tmp=w->child;
	while (tmp!=NULL)
	{
		XRaiseWindow(display,tmp->w);
		tmp=tmp->next;
	}
	return 0;
}


/*****************************************************************\

	Function: XRootWindow
	Inputs:   display, screen number
	Returned: root window ID.

	Comments: Info taken from display structure.

\*****************************************************************/

Window
XRootWindow(display,scr)
Display *display;
int scr;
{
	xtrace("XRootWindow\n");
	return(display->screens[0].root);
}


/*****************************************************************\

	Function: XRootWindowOfScreen
	Inputs:   screen pointer
	Returned: root window ID.

	Comments: ZZzzz...

\*****************************************************************/

Window
XRootWindowOfScreen(scr)
Screen *scr;
{
	xtrace("XRootWindowOfScreen\n");
	return(scr->root);
}


/*****************************************************************\

	Function: NT_get_children
	Inputs:   Window
	Returned: number of windows, window list.

	Comments: In preference to the windows Enum procedures, we examine
		  our own data structure to find the children of a given
		  window.

\*****************************************************************/

NT_window**
NT_get_children(win,n)
NT_window *win;
int *n;
{
	int numcs=0;
        struct NT_child *tmp=win->child;
        NT_window **dest;

	while (tmp!=NULL)
	{
		numcs++;
		tmp=tmp->next;
	}
	*n=numcs;
	if (numcs==0)
		return (NULL);
	else
	{
		dest=(NT_window **) allocateMemory (sizeof(NT_window*)*numcs);
                tmp=win->child;
                while (tmp!=NULL)
		{
			dest[--numcs]=tmp->w;
			tmp=tmp->next;
		}
        }
        return (dest);
}


/*****************************************************************\

	Function: XTranslateCoordinates
	Inputs:   display, source window, destination window, source x, y.
	Returned: destination x, y, child window if any.

	Comments: Seems to work properly.

\*****************************************************************/

Bool
XTranslateCoordinates(display,sw,dw,sx,sy,dx,dy,ch)
Display *display;
NT_window *sw,*dw;
int sx,sy,*dx,*dy;
NT_window **ch;
{
	RECT srct,drct,crct;
	POINT target;
	NT_window **wp,*child=NULL;
	int nwin,x;
	xtrace("XTranslateCoordinates\n");
	if (sw->w == INVALID_HANDLE)
	{
		srct.left = sw->x;
		srct.right =  sw->x + sw->wdth;
		srct.top = sw->y;
		srct.bottom = sw->y + sw->hght;
	}
	else
		GetWindowRect(sw->w,&srct);

	if (dw->w == INVALID_HANDLE)
	{
		drct.left = dw->x;
		drct.right =  dw->x + dw->wdth;
		drct.top = dw->y;
		drct.bottom = dw->y + dw->hght;
	}
	else
		GetWindowRect(dw->w,&drct),

			*dx=sx+srct.left-drct.left;
	*dy=sy+srct.top-drct.top;
	target.x=sx+srct.left;
	target.y=sy+srct.top;
	wp=NT_get_children(dw,&nwin);
	for (x=0;x<nwin;++x)
	{
		if (wp[x]->w == INVALID_HANDLE)
		{
            crct.left = wp[x]->x;
            crct.right =  wp[x]->x + wp[x]->wdth;
            crct.top = wp[x]->y;
            crct.bottom = wp[x]->y + wp[x]->hght;
		}
		else
			GetWindowRect(wp[x]->w,&crct);
		if (target.x>=crct.left && target.x<crct.right &&
			target.y>=crct.left && target.y<crct.right &&
			PtVisible(wp[x]->w,target.x,target.y))
			child=wp[x];
	}
	freeMemory(wp);
	if (child==NULL)
		*ch=None;
	else
		*ch=child;
	return (True);
}


/*****************************************************************\

	Function: XUnmapWindow
	Inputs:   display, window.

	Comments: Removes a window and its frame, if it has one, from the
		  screen.

\*****************************************************************/

int
XUnmapWindow(display,w)
Display *display;
NT_window *w;
{
	xtrace("XUnmapWindow\n");
	ShowWindow(w->w,SW_HIDE);
	if ( w->frame != NULL )
		ShowWindow(w->frame->w,SW_HIDE);
	return 0;
}


/*****************************************************************\

	Function: XCopyGC
	Inputs:   display, source gc, values mask, destination gc.

	Comments: Copies masked elements from source to destination.

\*****************************************************************/

int
XCopyGC(display,sgc,vmask,dgc)
Display *display;
GC sgc,dgc;
unsigned long vmask;
{
	xtrace("XCopyGC\n");
	if (vmask&GCFunction)
		dgc->values.function=sgc->values.function;
	if (vmask&GCPlaneMask)
		dgc->values.plane_mask=sgc->values.plane_mask;
	if (vmask&GCForeground)
		dgc->values.foreground=sgc->values.foreground;
	if (vmask&GCBackground)
		dgc->values.background=sgc->values.background;
	if (vmask&GCLineWidth)
		dgc->values.line_width=sgc->values.line_width;
	if (vmask&GCLineStyle)
		dgc->values.line_style=sgc->values.line_style;
	if (vmask&GCCapStyle)
		dgc->values.cap_style=sgc->values.cap_style;
	if (vmask&GCJoinStyle)
		dgc->values.join_style=sgc->values.join_style;
	if (vmask&GCFillStyle)
		dgc->values.fill_style=sgc->values.fill_style;
	if (vmask&GCFillRule)
		dgc->values.fill_rule=sgc->values.fill_rule;
	if (vmask&GCTile)
		dgc->values.tile=sgc->values.tile;
	if (vmask&GCStipple)
		dgc->values.stipple=sgc->values.stipple;
	if (vmask&GCTileStipXOrigin)
		dgc->values.ts_x_origin=sgc->values.ts_x_origin;
	if (vmask&GCTileStipYOrigin)
		dgc->values.ts_y_origin=sgc->values.ts_y_origin;
	if (vmask&GCFont)
		dgc->values.font=sgc->values.font;
	if (vmask&GCSubwindowMode)
		dgc->values.subwindow_mode=sgc->values.subwindow_mode;
	if (vmask&GCGraphicsExposures)
		dgc->values.graphics_exposures=sgc->values.graphics_exposures;
	if (vmask&GCClipXOrigin)
		dgc->values.clip_x_origin=sgc->values.clip_x_origin;
	if (vmask&GCClipYOrigin)
		dgc->values.clip_y_origin=sgc->values.clip_y_origin;
	if (vmask&GCClipMask)
		dgc->values.clip_mask=sgc->values.clip_mask;
	if (vmask&GCDashList)
		dgc->values.dashes=sgc->values.dashes;
	if (vmask&GCArcMode)
		dgc->values.arc_mode=sgc->values.arc_mode;

	dgc->dirty = 0xFFFF;
	return 0;
}


int
XSetClipMask(display,gc,cmask)
Display *display;
GC gc;
Pixmap cmask;
{
	xtrace("XSetClipMask\n");
	return 0;
}


int
XSetClipRectangles(display,gc,clx,cly,rs,n,order)
Display *display;
GC gc;
int clx,cly;
XRectangle *rs;
int n,order;
{
	xtrace("XSetClipRectangles\n");
	return 0;
}


/*****************************************************************\

	Function: XSetFunction
	Inputs:   display, gc, graphics operation.

	Comments: ZZzzz...

\*****************************************************************/

int
XSetFunction(display,gc,fn)
Display *display;
GC gc;
int fn;
{
	xtrace("XSetFunction\n");
	gc->values.function=fn;
	gc->dirty |= GCFunction;
	return 0;
}


/*****************************************************************\

	Function: XSetLineAttributes
	Inputs:   display, gc, line width, line style, cap style, join style.

	Comments: These all have windows equivalents.

\*****************************************************************/

int
XSetLineAttributes(display,gc,lw,ls,cs,js)
Display *display;
GC gc;
unsigned int lw;
int ls,cs,js;
{
	xtrace("XSetLineAttributes\n");
	gc->values.line_width=lw;
	gc->values.line_style=ls;
	gc->values.cap_style=cs;
	gc->values.join_style=js;
	gc->dirty |= GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle;
	return 0;
}


/*****************************************************************\

	Function: XSetPlaneMask
	Inputs:   display, gc, plane mask.

	Comments: What's a plane mask?

\*****************************************************************/

int
XSetPlaneMask(display,gc,pmask)
Display *display;
GC gc;
unsigned long pmask;
{
	xtrace("XSetPlaneMask\n");
	gc->values.plane_mask=pmask;
	return 0;
}


int
XSetTile(display,gc,tile)
Display *display;
GC gc;
Pixmap tile;
{
	xtrace("XSetTile\n");
	return 0;
}


Status
XAllocColorCells(display,cmap,cont,pmasks,np,pixels,nc)
Display *display;
Colormap cmap;
Bool cont;
unsigned long *pmasks;
unsigned int np;
unsigned long *pixels;
unsigned int nc;
{
	unsigned int i;
	cjh_printf("Alloate Color Cells with %d colors, %d masks.\n",nc,np);
	xtrace("XAllocColorCells\n");
	if (nextFreeColour + nc < cmapSize)
	{
		for (i = 0;i<nc;i++)
			pixels[i] = nextFreeColour++;
		if(np==1)
		{
			nextFreeColour+=nc;
			*pmasks = nc;
		}
		return 1;
	}
	return 0;

}


/*****************************************************************\

	Function: XAllocColorPlanes
	Inputs:   display, colour map, contiguous flag, pixel value list,
		  number of colours, number of reds, greens, blues.
	Returned: red mask, green mask, blue mask, status.

	Comments: Works for True Colour only.

\*****************************************************************/

Status
XAllocColorPlanes(display,cmap,cont,pixels,nc,nr,ng,nb,rmask,gmask,bmask)
Display *display;
Colormap cmap;
Bool cont;
unsigned long *pixels;
int nc;
int nr,ng,nb;
unsigned long *rmask,*gmask,*bmask;
{
	xtrace("XAllocColorPlanes\n");
	*rmask=255;
	*gmask=255<<8;
	*bmask=255<<16;
	return (1);
}


Status
XAllocNamedColor(Display *display,
    Colormap cmap, const char *cname,
    XColor *cell, XColor *rgb)
{
	xtrace("XAllocNamedColor\n");
	return 0;
}


Colormap
XCreateColormap(display,w,visual,alloc)
Display *display;
Window w;
Visual *visual;
int alloc;
{
	xtrace("XCreateColormap\n");
	return 0;
}


Status
XGetStandardColormap(display,w,cmapinf,prop)
Display *display;
Window w;
XStandardColormap *cmapinf;
Atom prop;
{
	xtrace("XGetStandardColormap\n");
	return 0;
}



int
XQueryColor(display,cmap,cell)
Display *display;
Colormap cmap;
XColor *cell;
{
	xtrace("XQueryColor\n");
  return XQueryColors(display,cmap,cell,1);
}


int
XQueryColors(display,cmap,cells,nc)
Display *display;
Colormap cmap;
XColor *cells;
int nc;
{
	int i;

	xtrace("XQueryColors\n");
	for (i=0;i<nc;i++)
	{
		cells[i].red=sysPal->palPalEntry[cells[i].pixel].peRed;
		cells[i].green=sysPal->palPalEntry[cells[i].pixel].peGreen;
		cells[i].blue=sysPal->palPalEntry[cells[i].pixel].peBlue;
	}
	return 0;
}


int
XStoreColor(display,cmap,cell)
Display *display;
Colormap cmap;
XColor *cell;
{
	xtrace("XStoreColor\n");
    sysPal->palPalEntry[cell->pixel].peRed = (unsigned char)cell->red;
    sysPal->palPalEntry[cell->pixel].peBlue = (unsigned char)cell->blue;
    sysPal->palPalEntry[cell->pixel].peGreen = (unsigned char)cell->green;
	return 0;
}



int
XStoreColors(display,cmap,cells,nc)
Display *display;
Colormap cmap;
XColor *cells;
int nc;
{
	int i;
	xtrace("XStoreColors\n");
	for (i=0 ; i<nc ; i++)
	{
		sysPal->palPalEntry[cells[i].pixel].peRed = (unsigned char)cells[i].red;
		sysPal->palPalEntry[cells[i].pixel].peBlue = (unsigned char)cells[i].blue;
		sysPal->palPalEntry[cells[i].pixel].peGreen = (unsigned char)cells[i].green;
	}
	return 0;
}

char **
XGetFontPath(display,nps)
Display *display;
int *nps;
{
	xtrace("XGetFontPath\n");
	return (NULL);
}


Bool
XGetFontProperty(fstruct,atom,val)
XFontStruct *fstruct;
Atom atom;
unsigned long *val;
{
	xtrace("XGetFontProperty\n");
	return (0);
}


Font
NT_loadfont(name)
char *name;
{
	LOGFONT lf;
	HFONT hfont;
	memset(&lf,0,sizeof(lf));
	lf.lfHeight = -13;
	strcpy(lf.lfFaceName,"Lucida Console");
	hfont = CreateFontIndirect(&lf);
	return (Font)hfont;
}

XFontStruct *
XLoadQueryFont(Display *display, const char *name)
{
	XFontStruct *fs;
	TEXTMETRIC tm;
	HDC hdc;
	HWND root;
	HFONT old;
	int i;
	
	xtrace("XLoadQueryFont\n");
	fs = allocateMemory(sizeof(XFontStruct));
	fs->fid = NT_loadfont(name);

	root=GetDesktopWindow();
	hdc=cjh_get_dc(root);
	old = SelectObject(hdc, (HFONT)fs->fid);
	GetTextMetrics(hdc, &tm);
	SelectObject(hdc,old);
	cjh_rel_dc(root,hdc);
	fs->min_bounds.width = (short)tm.tmMaxCharWidth;
	fs->max_bounds.width = (short)tm.tmMaxCharWidth;
	fs->n_properties = 0;
	fs->properties = NULL;
	fs->min_char_or_byte2 =0;
	fs->max_char_or_byte2 =255;
	fs->per_char = (XCharStruct*)allocateMemory(sizeof(XCharStruct)*256);
	for (i=0; i<256; i++) {
		fs->per_char[i].width = (short)tm.tmMaxCharWidth;
		fs->per_char[i].rbearing = (short)tm.tmMaxCharWidth;
	}
	fs->ascent = tm.tmAscent;
	fs->descent= tm.tmDescent;
	return(fs);
}

XFontStruct *
XQueryFont(display, font_id)
Display *display;
XID     font_id;
{
	static XFontStruct fs;

	xtrace("XQueryFont\n");
	return(&fs);
}

KeySym
XKeycodeToKeysym(display, keycode, index)
Display *display;
unsigned int keycode;
int     index;
{
	xtrace("XKeycodeToKeysym\n");
	return(NoSymbol);
}
KeySym
XStringToKeysym(const char *str)
{
	xtrace("XStringToKeysym\n");
	return(NoSymbol);
}

XModifierKeymap *
XGetModifierMapping(display)
Display *display;
{
	static XModifierKeymap map;

	xtrace("XGetModifierMapping\n");
	map.max_keypermod = 0;

	return(&map);

}
int
XSetFont(display,gc,font)
Display *display;
GC gc;
Font font;
{
	xtrace("XSetFont\n");
	return 0;
}


int
XSetFontPath(display,dirs,nd)
Display *display;
char **dirs;
int nd;
{
	xtrace("XSetFontPath\n");
	return 0;
}


/*****************************************************************\

	Function: XTextExtents
	Inputs:   font structure, string, string length.
	Returned: writing direction, max ascent, max descent, font overall
		  characteristics.

	Comments: The design of windows fonts is similar to X, as far as
		  ascent and descent are concerned.  However, they are
		  positioned differently on the screen (see XDrawText).

\*****************************************************************/

static HDC desktopHDC;
static int firstTE = TRUE;
int
XTextExtents(fstruct,str,nc,dir,ascent,descent,overall)
XFontStruct *fstruct;
const char *str;
int nc;
int *dir,*ascent,*descent;
XCharStruct *overall;
{
	TEXTMETRIC tmet;
	HDC hdc;
	SIZE tsize;
	HWND desktop;
	HFONT old;
	
	xtrace("XTextExtents\n");
	if (firstTE)
	{
		firstTE = FALSE;
		desktop=GetDesktopWindow();
		desktopHDC=cjh_get_dc(desktop);
	}
	hdc = desktopHDC;
	old = SelectObject(hdc, (HFONT)fstruct->fid);
	GetTextMetrics(hdc,&tmet);
	GetTextExtentPoint(hdc,str,nc,&tsize);
	*dir=FontLeftToRight;
	*ascent=tmet.tmAscent + 1;
	*descent=tmet.tmDescent;
	overall->ascent=(short)(tmet.tmAscent + 1);
	overall->descent=(short)tmet.tmDescent;
	overall->width=(short)tsize.cx;
	overall->lbearing=0;
	overall->rbearing=0;
	/* cjh_rel_dc(desktop,hdc);*/
	SelectObject(hdc,old);
	return 0;
}


int
XTextExtents16(fstruct,str,nc,dir,ascent,descent,overall)
XFontStruct *fstruct;
const XChar2b *str;
int nc;
int *dir,*ascent,*descent;
XCharStruct *overall;
{
	xtrace("XTextExtents16\n");
	return 0;
}


/*****************************************************************\


	Function: XTextWidth
	Inputs:   font structure, string, length of string.
	Returned: String width in pixels.

	Comments:

\*****************************************************************/

int
XTextWidth(fstruct,str,co)
XFontStruct *fstruct;
const char *str;
int co;
{
	HDC hdc;
	SIZE tsize;
	HWND root;
	HFONT old;
	xtrace("XTextWidth\n");

	if(firstTE)
	{
		firstTE = FALSE;
		root=GetDesktopWindow();
		hdc=cjh_get_dc(root);
	}
	old = SelectObject(hdc, (HFONT)fstruct->fid);
	GetTextExtentPoint(hdc,str,co,&tsize);
	SelectObject(hdc,old);
	/*cjh_rel_dc(root,hdc);*/
	return (tsize.cx);
}


int
XTextWidth16(fstruct,str,co)
XFontStruct *fstruct;
const XChar2b *str;
int co;
{
	xtrace("XTextWidth16\n");
	return 0;
}


int
XGetErrorDatabaseText(display,name,msg,defstr,buf,len)
Display *display;
const char *name,*msg;
const char *defstr;
char *buf;
int len;
{
	static char def_err[]="Errors not implemented";
	int n;

	xtrace("XGetErrorDatabaseText\n");
	while (n<len && def_err[n] != 0)
		*(buf+n)=def_err[n++];
	*(buf+n)=0;
	return 0;
}


int
XGetErrorText(display,code,buf,len)
Display *display;
int code;
char *buf;
int len;
{
	xtrace("XGetErrorText\n");
	return 0;
}


XErrorHandler
XSetErrorHandler(handler)
XErrorHandler handler;
{
	xtrace("XSetErrorHandler\n");
	return 0;
}


/*****************************************************************\


	Function: XDefaultScreen
	Inputs:   display
	Returned: default screen number

	Comments:

\*****************************************************************/

int
XDefaultScreen(display)
Display *display;
{
	xtrace("XDefaultScreen\n");
	return (display->default_screen);
}


/*****************************************************************\


	Function: XScreenOfDisplay
	Inputs:   display,screen number
	Returned: screen list.

	Comments:

\*****************************************************************/

Screen *
XScreenOfDisplay(display,scr)
Display *display;
int scr;
{
	xtrace("XScreenOfDisplay\n");
	return (display->screens);
}


Cursor
XCreateFontCursor(display,shape)
Display *display;
unsigned int shape;
{
	xtrace("XCreateFontCursor\n");
	return 0;
}


int
XRecolorCursor(display,cursor,fg,bg)
Display *display;
Cursor cursor;
XColor *fg,*bg;
{
	xtrace("XRecolorCursor\n");
	return 0;
}


/*****************************************************************\


	Function: XWarpPointer
	Inputs:   display, source window, destination window, source window
		  geometry, destination x, y.

	Comments: Not knowingly tested.

\*****************************************************************/

int
XWarpPointer(display,sw,dw,sx,sy,swidth,sheight,dx,dy)
Display *display;
NT_window *sw,*dw;
int sx,sy;
unsigned int swidth,sheight;
int dx,dy;
{
	POINT cpt,tmp;
	RECT srct;

	xtrace("XWarpPointer\n");
	GetCursorPos(&cpt);
	if (sw==None)
	{
		if (dw==None)
			SetCursorPos(dx,dy);
		else
		{
			tmp.x=dx;
			tmp.y=dy;
			ClientToScreen(dw->w,&tmp);
			SetCursorPos(tmp.x,tmp.y);
		}
	}
	else
	{
		GetWindowRect(sw->w,&srct);
		tmp.x=sx;
		tmp.y=sy;
		ClientToScreen(sw->w,&tmp);
		if (swidth==0)
			swidth=srct.right-sx;
		if (sheight==0)
			sheight=srct.bottom-sy;
		if (cpt.x>=tmp.x && cpt.x<tmp.x+(int)swidth &&
		    cpt.y>=tmp.y && cpt.y<tmp.y+(int)sheight &&
		    PtVisible(sw->w,cpt.x,cpt.y))
		{
			if (dw==None)
				SetCursorPos(cpt.x+dx,cpt.y+dy);
			else
			{
				tmp.x=dx;
				tmp.y=dy;
				ClientToScreen(dw->w,&tmp);
				SetCursorPos(tmp.x,tmp.y);
			}
		}
	}
	return 0;
}


/*****************************************************************\


	Function: XBell
	Inputs:   display, percent loudness.

	Comments: Don't throw away your CD just yet.

\*****************************************************************/

int
XBell(display,pc)
Display *display;
int pc;
{
	xtrace("XBell\n");
	Beep(20,40);
	return 0;
}


/*****************************************************************\


	Function: XGetInputFocus
	Inputs:   display, focus window, revert to window.

	Comments: We don't have the data in place for the revert to field
		  to work.

\*****************************************************************/

int
XGetInputFocus(display,focus,revto)
Display *display;
NT_window **focus;
int *revto;
{
	xtrace("XGetInputFocus\n");
	*focus=GetFocus();  /* Note: returns NULL if the focus window */
	revto=RevertToNone; /*       belongs to another thread.       */
	return 0;
}


/*****************************************************************\


	Function: XSetInputFocus
	Inputs:   display, focus window, revert to window, time.

	Comments: Set focus to requested client window.

\*****************************************************************/

int
XSetInputFocus(display,focus,revto,time)
Display *display;
NT_window *focus;
int revto;
Time time;
{
	xtrace("XSetInputFocus\n");
	SetFocus(focus->w);
	return 0;
}


int
XLookupString(event,buf,nbytes,keysym,status)
XKeyEvent *event;
char *buf;
int nbytes;
KeySym *keysym;
XComposeStatus *status;
{
	xtrace("XLookupString\n");
	*buf=event->keycode;
	*keysym=event->keycode;
	return (1);
}


int
XRefreshKeyboardMapping(event)
XMappingEvent *event;
{
	xtrace("XRefreshKeyboardMapping\n");
	return 0;
}


int
XSetClassHint(display,w,chints)
Display *display;
Window w;
XClassHint *chints;
{
	xtrace("XSetClassHint\n");
	return 0;
}


/*****************************************************************\


	Function: XSetNormalHints
	Inputs:   display, window, size hints.

	Comments: Assuming the role of the window manager, we alter the
		  window geometry as requested.

\*****************************************************************/

int
XSetNormalHints(display,w,hints)
Display *display;
NT_window *w;
XSizeHints *hints;
{
	UINT ff;

	xtrace("XSetNormalHints\n");
	if (w->frame==NULL)
	{
		if (!hints->flags&PPosition)
			ff=SWP_NOMOVE;
		else
			ff=0;
		if (!hints->flags&PSize)
			ff=ff|SWP_NOSIZE;
		if (w->w == INVALID_HANDLE)
		{

			/* Not yet mapped, but already being moved. set the new coordinates*/
            if(w->x == -1 && w->y == -1)
            {
				w->frame->x = hints->x;
				w->frame->y = hints->y;
				w->frame->wdth = hints->width + 8;
				w->frame->hght = hints->height + 32;
            }
            else
            {
				w->x = hints->x;
				w->y = hints->y;
				w->wdth = hints->width;
				w->hght = hints->height;
            }
		}
		else
			SetWindowPos(w->w,HWND_TOPMOST,hints->x,hints->y,
						 hints->width,hints->height,ff|SWP_SHOWWINDOW);
	}
	return 0;
}


int
XSetWMHints(display,w,wmhints)
Display *display;
Window w;
XWMHints *wmhints;
{
	xtrace("XSetWMHints\n");
	return 0;
}


Status
XSetWMProtocols(display,w,prots,co)
Display *display;
Window w;
Atom *prots;
int co;
{
	xtrace("XSetWMProtocols\n");
	return 0;
}


/*****************************************************************\


	Function: XStoreName
	Inputs:   display, window, window name.

	Comments: Only set names to the frame windows, otherwise captions
		  appear in the client areas.

\*****************************************************************/

int
XStoreName(display,w,wname)
Display *display;
NT_window *w;
const char *wname;
{
	int status = 0;

	xtrace("XStoreName\n");
	if ( w->frame != NULL )
	{
		w->title_text = strdup(wname);
		if (w->frame->w !=INVALID_HANDLE)
			status = SetWindowText (w->frame->w , wname);
	}

	return(status);
}

Status
XFetchName(
    Display *display,
    NT_window *w,
    char **window_name_return)
{
	int status = 1;
	xtrace("XFetchName\n");
	if ( w->frame != NULL && w->title_text)
	{
		*window_name_return = strdup(w->title_text);
		status =0;
	}
	return(status);
}



/*****************************************************************\


	Function: XDoesBackingStore
	Inputs:   screen

	Comments: No backing store at the moment.  Windows doesn't seem
		  to support it, unless we do it ourselves with compatible
		  bitmaps.

\*****************************************************************/

int
XDoesBackingStore(scr)
Screen *scr;
{
	xtrace("XDoesBackingStore\n");
	return(0);
}


XExtCodes *
XInitExtension(display,name)
Display *display;
const char *name;
{
	xtrace("XInitExtension\n");
	return 0;
}


/*****************************************************************\


	Function: XFree
	Inputs:   data to be freed.

	Comments: This might need changing sometime.  No crashes so far.

\*****************************************************************/

int
XFree(data)
char *data;
{
	xtrace("XFree\n");
	freeMemory(data);
	return 0;
}

/*****************************************************************\


	Function: XServerVendor
	Inputs:   display.
	Returned: string of vendor's name.

	Comments: Copied from the display structure.

\*****************************************************************/

char *
XServerVendor(display)
Display *display;
{
	xtrace("XServerVendor\n");
	return (display->vendor);
}


int
XSetIconName(display,w,iname)
Display *display;
Window w;
const char *iname;
{
	xtrace("XSetIconName\n");
	return 0;
}
int
XGetIconName(display,w,iname)
Display *display;
Window w;
char **iname;
{
	xtrace("XGetIconName\n");
	*iname = NULL;
	return 0;
}


int
XSetSelectionOwner(display, sel, owner, time)
Display* display;
Atom sel;
Window owner;
Time time;
{
    HWND hwnd = owner ? ((NT_window*)owner)->w : NULL;
	if (GetClipboardOwner() != hwnd) {
	    OpenClipboard(hwnd);
	    EmptyClipboard();
	    SetClipboardData(CF_TEXT, NULL);
	    CloseClipboard();
	}
	return 0;
}

Window
XGetSelectionOwner(display,selection)
Display* display;
Atom selection;
{
	HWND hwnd = NULL;
	xtrace("XGetSelectionOwner\n");
	hwnd = GetClipboardOwner();
	return (Window)NT_find_window_from_id(hwnd);
}

int
XConvertSelection(display,sel,target,prop,req,time)
Display *display;
Atom sel,target,prop;
Window req;
Time time;
{
	xtrace("XConvertSelection\n");
	return 0;
}


/*****************************************************************\


	Function: XCheckWindowEvent
	Inputs:   display, window, event mask.
	Returned: pointer to filled in event structure, status.

	Comments: This is fudged at the moment to work with the toolkit.
		  The event system needs rewriting to account properly for
		  event masks.

\*****************************************************************/

Bool
XCheckTypedEvent(display,ev,rep)
Display *display;
int ev;
XEvent *rep;
{
	xtrace("XCheckTypedEvent\n");
	return (False);
}

Bool
XCheckWindowEvent(display,w,emask,ev)
Display *display;
NT_window *w;
long emask;
XEvent *ev;
{
	MSG msg;
	Bool status = 0;

	xtrace("XCheckWindowEvent\n");
	if (emask&SubstructureNotifyMask)
		if (PeekMessage(&msg,w->w,USR_StructureNotify,
						USR_StructureNotify,PM_REMOVE)||
		    PeekMessage(&msg,w->w,WM_PAINT,WM_PAINT,PM_REMOVE))
		{
			cjh_printf("removed message\n");
			ev->type=ConfigureNotify;
			status = 1;
		}
	return(status);
}

/*
  XPending checks for x events pending.
  We don't know if we have x events until we process
  the win events.
  */
int
XPending (display)
Display *display;
{
	MSG msg;
	/*	xtrace("XPending\n"); */
	while(wineventq->count<=0 && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return wineventq->count;
}

int
XPutBackEvent(display,event)
Display *display;
XEvent *event;
{
	xtrace("XPutBackEvent\n");
	return 0;
}


Status
XSendEvent(display,w,prop,emask,event)
Display *display;
Window w;
Bool prop;
long emask;
XEvent *event;
{
	xtrace("XSendEvent\n");
	return 0;
}

/* I'm tempted to flush the windows queue
**   before checking, but I think that would
**   break the assumtion that most of the WM_PAINT
**   messges are only going to be dispatched when
**   the app is directly calling us.
*/

Bool
XCheckTypedWindowEvent(
	Display* display,
	Window w,
	int event_type,
	XEvent* event_return)
{
	int i,j;
	xtrace("XCheckTypedWindowEvent\n");
	if (w==0) return 0;
	i = wineventq->next;
	while(i != wineventq->avail)
	{
		if (wineventq->list[i].window==(NT_window*)w)
		{
			WinEventToXEvent(&wineventq->list[i],event_return);
			if (event_return->type == event_type)
			{
				break;
			}
		}
		i++;
		if (i>wineventq->num) i=0;
	}
	if (i != wineventq->avail)
	{
		while(i != wineventq->next)
		{
			j =i-1;
			if (j<0) j= wineventq->num;
			copyWinEvent(&wineventq->list[i],&wineventq->list[j]);
			i = j;
		}
		wineventq->next++;
		wineventq->count--;
		cjh_printf("removed an event\n");
		return 1;
	}
	return 0;
}

/*****************************************************************\


	Function: XWindowEvent
	Inputs:   display, window, event mask.
	Returned: pointer to filled in event structure.

	Comments: This is fudged at the moment to work with the toolkit.
		  The event system needs rewriting to account properly for
		  event masks.

\*****************************************************************/

int
XWindowEvent(display,w,emask,rep)
Display *display;
NT_window *w;
long emask;
XEvent *rep;
{
	MSG msg;

	xtrace("XWindowEvent\n");
	if (emask&ExposureMask)
	{
		GetMessage(&msg,w->w,USR_StructureNotify,USR_StructureNotify);
		rep->type=ConfigureNotify;
	}
	return 0;
}



/*****************************************************************\

	Function: XNextEvent
	Inputs:   display, event structure pointer.

	Comments: Windows routines receive messages (events) in two ways:
		  firstly by GetMessage, which takes messages from the
		  calling thread's message queue, and secondly by the
		  window function being called with events as arguments.
		  To simulate the X system, we get messages from the queue
		  and pass them to the window function anyway, which
		  processes them and fills out the local XEvent structure.
		  DispatchMessage calls the window procedure and waits until
		  it returns. Translate message turns WM_KEYUP/DOWN messages
		  into WM_CHAR.

\*****************************************************************/

int
XNextEvent(display, event)
Display *display;
XEvent  *event;
{
	MSG msg;
	WinEvent we;
	
	xtrace("XNextEvent\n");

	/* if there isn't already an event in the pipe, this will block */
	while(wineventq->count <= 0 && GetMessage(&msg, NULL, 0, 0))
	{
		
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (wineventq->count>0)
	{	
		getQdEvent(wineventq,&we);
		WinEventToXEvent(&we,event);
	}
	else
	{
		/* hmm, GetMessage failed, maybe we're supposed to quit */
		event->type=ClientMessage;
		event->xclient.format = 32;
		event->xclient.data.l[0] = wmDeleteWindow;
		return 1;
	}
	return 1;
	
}


/*----------------------------------------------------------------*\
| FUNCTIONS TO MAINTAIN AN INTERNAL LIST OF WINDOWS AND THEIR      |
| ATTRIBUTES - AS WOULD BE MAINTAINED IN THE X SERVER.             |
\*----------------------------------------------------------------*/




/*---------------------------------------------------*\
| Function: NT_new_window_pair                        |
| Purpose:  Add a new window id to the Window table   |
| Return:   Pointer to the new Window structure.      |
\*---------------------------------------------------*/
static struct NT_window *
NT_new_window_pair(clientWin)
struct NT_window **clientWin;
{
	struct NT_window *new;
	struct NT_window *finder;
	xtrace("NT_new_window_pair\n");
	NT_show_window_list();

	finder = window_list;
	while(finder)
	{
		if (finder->free_flag==NT_FREE_WIN && finder->client!= NULL)
			break;
		finder = finder->next;
	}
	if (!finder)
	{
		new = (struct NT_window *) allocateMemory (sizeof(struct NT_window));
		new->next = window_list;
		new->child=NULL;
		new->props=NULL;
		new->free_flag = NT_NEWLY_CREATED;
		new->w = INVALID_HANDLE;
		window_list = new;

		(*clientWin) = (struct NT_window *) allocateMemory (sizeof(struct NT_window));
		(*clientWin)->next = window_list;
		(*clientWin)->child=NULL;
		(*clientWin)->props=NULL;
		(*clientWin)->free_flag = NT_NEWLY_CREATED;
		(*clientWin)->top_flag = FALSE;
		(*clientWin)->w = INVALID_HANDLE;
		window_list = (*clientWin);
		cjh_printf("NEW window pair %x %x\n",window_list->next,window_list);
		return(window_list->next);
	}
	else
	{
		cjh_printf("REUSING window pair %x %x\n",finder,finder->client);

		*clientWin = finder->client;
		(*clientWin)->child=NULL;
		(*clientWin)->client = NULL;
		(*clientWin)->parent = NULL;
		(*clientWin)->frame = NULL;
		NT_del_prop_list((*clientWin)->props);
		(*clientWin)->props=NULL;
		(*clientWin)->free_flag = NT_NOT_REPROPED;
		(*clientWin)->top_flag = FALSE;

		finder->child=NULL;
		finder->client = NULL;
		finder->parent = NULL;
		finder->frame = NULL;
		NT_del_prop_list(finder->props);
		finder->props=NULL;
		finder->free_flag = NT_NOT_REPROPED;
		(*clientWin)->top_flag = FALSE;
		return finder;
	}
}

/*---------------------------------------------------*\
| Function: NT_new_window                             |
| Purpose:  Add a new window id to the Window table   |
| Return:   Pointer to the new Window structure.      |
\*---------------------------------------------------*/
static struct NT_window *
NT_new_window()
{
	struct NT_window *new;
	struct NT_window *finder;
	xtrace("NT_new_window\n");
	NT_show_window_list();

	finder = window_list;
	while(finder)
	{
		if (finder->free_flag==NT_FREE_WIN && finder->client==NULL
			&& finder->frame == NULL)
			break;
		finder = finder->next;
	}
	if (!finder)
	{
		new = (struct NT_window *) allocateMemory (sizeof(struct NT_window));
		new->next = window_list;
		new->child=NULL;
		new->props=NULL;
		new->free_flag = NT_NEWLY_CREATED;
		new->w = INVALID_HANDLE;
		window_list = new;
		cjh_printf("NEW window %x\n",window_list);
		return(window_list);
	}
	else
	{
		cjh_printf("REUSING window %x\n",finder);
		finder->child=NULL;
		finder->client = NULL;
		finder->parent = NULL;
		finder->frame = NULL;
		NT_del_prop_list(finder->props);
		finder->props=NULL;
		finder->free_flag = NT_NOT_REPROPED;
		return finder;
	}
}

/*---------------------------------------------------*\
| Function: NT_delete_window                          |
| Purpose:  Remove a window from the window list      |
| Input:    w - pointer to window data                |
| Return:   TRUE if deleted                           |
\*---------------------------------------------------*/
static int
NT_delete_window(struct NT_window *w)
{
/*	struct NT_window *current,*last; */
	int status = FALSE;
	xtrace("NT_delete_window\n");
	NT_show_window_list();

	if(w->parent != NULL)
		NT_del_child(w->parent,w);
	/*if (window_list==w)
	  {
	  window_list=window_list->next;
	  freeMemory(w);
	  status=TRUE;
	  }
	  else
	  {
	  last=window_list;
	  current=window_list->next;
	  while (current!=w && current!=NULL)
	  {
	  last=current;
	  current=current->next;
	  }
	  if (current!=NULL)
	  {
	  last->next=current->next;
	  freeMemory(current);
	  status=TRUE;
	  }
	  }
	  */
	cjh_printf("Marking Window %x as FREE\n",w);
	w->free_flag = NT_FREE_WIN;
	ShowWindow(w->w,SW_HIDE);

	return TRUE;
	/*return(status);*/
}

/*------------------------------------------------*\
| Function: NT_find_window_from_id                 |
| Purpose:  Find the window in the window list     |
|           from the HWND id of the window.        |
| Input:    w - Window id (Windows HWND)           |
| Return:   pointer to NT_window structure for w.  |
\*------------------------------------------------*/
static struct NT_window *
NT_find_window_from_id(HWND w)
{
	struct NT_window *current = window_list;
	/* xtrace("NT_find_window_from_id\n"); */
	
	while ( current != NULL &&
			current->w != w )
		current = current->next;
	if(current)
		/*		&& current->free_flag==NT_USED)*/
		return(current);
	current=window_list;
	/*	while(current) {
		cjh_printf("win %x no equal %x\n",current->w, w);
		current = current->next;
	} */
	
	return NULL;
}

int
NT_show_window_list()
{
	struct NT_window *current=window_list;
	struct NT_child *tmp;

	cjh_printf("----------- Window list --------------\n");
	while (current != NULL)
    {
		cjh_printf("%s Window - %d %d\n",current->free_flag?"":"Free", current, current->w);
		tmp=current->child;
		while (tmp!=NULL)
		{
			cjh_printf("Child          - %d %d\n", tmp, tmp->w);
			tmp=tmp->next;
		}
		current = current->next;
    }
	return 0;
}


/*****************************************************************\

	Function: NT_add_child
	Inputs:   parent and child window IDs.
	Returned: 1

	Comments: When a child window is created (eg. client canvas) we
		  update our internal list of windows and their children.
		  New children are added to the front of the list.

\*****************************************************************/

int
NT_add_child(parent,child)
NT_window *parent,*child;
{
	struct NT_child *new;

	new=(struct NT_child *) allocateMemory (sizeof(struct NT_child));
	new->w=child;
	new->next = parent->child;
	parent->child=new;
	return(1);
}


/*****************************************************************\

	Function: NT_del_child
	Inputs:   parent and child window IDs.
	Returned: TRUE if window is removed, FALSE otherwise.

	Comments: Finds child window if it exits, and it so removes it from
		  the window list.

\*****************************************************************/

int
NT_del_child(parent,child)
struct NT_window *parent;
struct NT_window *child;
{
	struct NT_child *current,*last;
	int status=FALSE;

	if (parent->child->w==child)
	{
		current = parent->child;
		parent->child=parent->child->next;
		freeMemory(current);
		status=TRUE;
	}
	else
	{
		last=parent->child;
		current=parent->child->next;
		while (current->w!=child && current!=NULL)
		{
			last=current;
			current=current->next;
		}
		if (current!=NULL)
		{
			last->next=current->next;
			freeMemory(current);
			status=TRUE;
		}
	}
	return(status);
}


/*****************************************************************\

	Function: NT_set_rop
	Inputs:   window device context, X graphics context

	Comments: Sets the graphics drawing operation.

\*****************************************************************/

void
NT_set_rop(hdc,gc)
HDC hdc;
GC gc;
{
	switch (gc->values.function)
	{
		case GXclear:
			SetROP2(hdc,R2_BLACK);
			break;

		case GXand:
			SetROP2(hdc,R2_MASKPEN);
			break;

		case GXandReverse:
			SetROP2(hdc,R2_MASKPENNOT);
			break;

		case GXcopy:
			SetROP2(hdc,R2_COPYPEN);
			break;

		case GXnoop:
			SetROP2(hdc,R2_NOP);
			break;

		case GXxor:
			SetROP2(hdc,R2_XORPEN);/*XORPEN);*/
			cjh_printf("Setting to Xor\n");
			break;

		case GXor:
			SetROP2(hdc,R2_MERGEPEN);
			break;

		case GXnor:
			SetROP2(hdc,R2_NOTMERGEPEN);
			break;

		case GXequiv:
			SetROP2(hdc,R2_NOTXORPEN);
			break;

		case GXinvert:
			SetROP2(hdc,R2_NOT);
			break;

		case GXorReverse:
			SetROP2(hdc,R2_MERGEPENNOT);
			break;

		case GXcopyInverted:
			SetROP2(hdc,R2_NOTCOPYPEN);
			break;

		case GXorInverted:
			SetROP2(hdc,R2_MERGENOTPEN);
			break;

		case GXnand:
			SetROP2(hdc,R2_NOTMASKPEN);
			break;

		case GXset:
			SetROP2(hdc,R2_WHITE);
			break;
	}
}

/*****************************************************************\

	Function: NT_check_update_GC
	Inputs:   gc - Graphics Context

	Comments: Check what has changed in the GC and modify the
		  pen and brush accordingly.

\*****************************************************************/
static int
NT_check_update_GC(gc)
GC gc;
{
	DWORD style=PS_GEOMETRIC;
	LOGBRUSH lbr;
	int	 width;
	NTGC	*lntgc;

	if ( gc->ext_data == NULL )
	{
		gc->ext_data = (XExtData *)allocateMemory(sizeof(XExtData));
		lntgc = (NTGC *)allocateMemory(sizeof(NTGC));
		gc->ext_data->private_data = (char *)lntgc;
		lntgc->pen = INVALID_HANDLE;
		lntgc->brush = INVALID_HANDLE;
	}

	if ( TRUE ||(gc->dirty & GCForeground) ||
		 (gc->dirty & GCLineStyle)  ||
	     (gc->dirty & GCCapStyle)   ||
	     (gc->dirty & GCJoinStyle)  ||
	     (gc->dirty & GCLineWidth) )
	{
		lbr.lbStyle=BS_SOLID;
		lbr.lbColor=CNUMTORGB(gc->values.foreground);
		lbr.lbHatch=0;

		if (gc->values.line_style==LineDoubleDash)
			style|=PS_DASHDOT;
		else if (gc->values.line_style==LineOnOffDash)
			style|=PS_DASH;
		else
			style|=PS_SOLID;

		if (gc->values.cap_style==CapProjecting)
			style|=PS_ENDCAP_SQUARE;
		else if (gc->values.cap_style==CapRound)
			style|=PS_ENDCAP_ROUND;
		else
			style|=PS_ENDCAP_FLAT;

		if (gc->values.join_style==JoinRound)
			style|=PS_JOIN_ROUND;
		else if (gc->values.join_style==JoinMiter)
			style|=PS_JOIN_MITER;
		else
			style|=PS_JOIN_BEVEL;
		width=gc->values.line_width;
		if (width==0)
			width=1;

		lntgc = (NTGC *)gc->ext_data->private_data;
		if ( lntgc->pen != INVALID_HANDLE )
			DeleteObject(lntgc->pen);

		lntgc->pen = ExtCreatePen(style,width,&lbr,0,NULL);
	}

	if ( TRUE || (gc->dirty & GCForeground) )
	{
		lntgc = (NTGC *)gc->ext_data->private_data;
		if ( lntgc->brush != INVALID_HANDLE )
			DeleteObject(lntgc->brush);
		lntgc->brush = CreateSolidBrush(CNUMTORGB(gc->values.foreground));
	}

	gc->dirty = 0;

	return(1);
}


/*****************************************************************\

	Function: NT_get_GC_pen
	Inputs:   device context, graphics context

	Comments: Sets the device context and pen according to the
		  graphics context.

\*****************************************************************/
static HPEN
NT_get_GC_pen(hdc,gc)
HDC hdc;
GC gc;
{
	NTGC *lntgc;

	NT_check_update_GC(gc);
	NT_set_rop(hdc,gc);

	lntgc = (NTGC *)gc->ext_data->private_data;

	return(lntgc->pen);
}


/*****************************************************************\

	Function: NT_get_GC_brush
	Inputs:   device context, graphics context
	Returns:  handle for brush.

	Comments: Same as above for painting operations.

\*****************************************************************/
static HBRUSH
NT_get_GC_brush(hdc,gc)
HDC hdc;
GC gc;
{
	NTGC *lntgc;

        NT_check_update_GC(gc);

	if (gc->values.fill_rule==EvenOddRule)
		SetPolyFillMode(hdc,ALTERNATE);
	else
		SetPolyFillMode(hdc,WINDING);

	NT_set_rop(hdc,gc);

	lntgc = (NTGC *)gc->ext_data->private_data;

	return(lntgc->brush);
}


/*****************************************************************\

	Function: NT_deg64_to_rad
	Inputs:   angle (in 64ths of a degree)

	Comments: Converts int angle to double in radians.

\*****************************************************************/

double
NT_deg64_to_rad(a)
int a;
{
	return ((double)a/64.0*0.017453);
}

/******************************************************************/
/*                                                                */
/*               Atoms and properties code.                       */
/*                                                                */
/******************************************************************/

static char **nt_atoms;
static int num_nt_atoms = 0;
static int max_num_nt_atoms = 0;
#define ATOMS_BLOCK_SIZE 40

/******************************************************************\

         Function:  XInternAtom
         Inputs:    Display, property name, creation flag.

         Comments:  Could be made much more efficient.

\******************************************************************/

Atom
XInternAtom(display, property_name, only_if_exists)
Display *display;
const char *property_name;
Bool only_if_exists;
{
	int i;
	char **new_nt_atoms;

	xtrace("XInternAtom\n");
	if (strcmp(property_name,"VT_SELECTION")==0)
	{
		return XA_LAST_PREDEFINED + 667;
	}
	
	for (i=0;i< num_nt_atoms ;i++)
		if (strcmp(nt_atoms[i],property_name) == 0)
			return XA_LAST_PREDEFINED + i;
	
	if (only_if_exists)
		return None;

	if (num_nt_atoms >= max_num_nt_atoms)
	{
		new_nt_atoms = (char **)realloc(nt_atoms,(max_num_nt_atoms + ATOMS_BLOCK_SIZE)*sizeof(char *));
		if (!new_nt_atoms)
			return None;
		nt_atoms = new_nt_atoms;
		max_num_nt_atoms+= ATOMS_BLOCK_SIZE;
		nt_atoms[num_nt_atoms] = allocateMemory(strlen(property_name)+1);
		if (!nt_atoms[num_nt_atoms])
			return None;
		strcpy(nt_atoms[num_nt_atoms],property_name);
		return (XA_LAST_PREDEFINED +  num_nt_atoms++);
	}
}

/******************************************************************\

         Function:  XGetAtomName
         Inputs:    Display,Atom

         Comments:  None.

\******************************************************************/
char *
XGetAtomName(display,atom)
Display *display;
Atom atom;
{
	char *ret_string;
	xtrace("XGetAtomName\n");
	if (atom > num_nt_atoms + XA_LAST_PREDEFINED)
		return NULL;

	if (! (ret_string = allocateMemory(strlen(nt_atoms[atom - XA_LAST_PREDEFINED])+1)))
		return FALSE;

	strcpy(ret_string,nt_atoms[atom]);

	return ret_string;
}

/******************************************************************\

         Function:  XChangeProperty
         Inputs:    Display,Window,Property,type,format,mode,data,
                    nelements.

         Comments:  None.

\******************************************************************/
void
XChangeProperty(
	Display *display,
	Window window,
	Atom property,
	Atom type,
	int format,
	int mode,
	const unsigned char *data,
	int nelements)
{
	struct NT_prop_list *wanderer, *new;
	xtrace("XChangeProperty\n");
	/* here's a hack to fake setting the selection */
	if (property == XA_CUT_BUFFER0)
	{
		HGLOBAL handle=NULL;
		char *buffer=NULL;
		
		handle = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, nelements+1);
		if (!handle) return;
		OpenClipboard(window->w);
		buffer = GlobalLock(handle);
		memcpy(buffer, data, nelements);
		GlobalUnlock(handle);
		SetClipboardData(CF_TEXT, handle);
		CloseClipboard();
		return;
	}
	wanderer = window->props;
	while (wanderer)
	{
		if (wanderer->atom == property)
			break;
		wanderer = wanderer->next;
	}

	if (!wanderer)
	{
		if(!(new = allocateMemory(sizeof(struct NT_prop_list))))
			return;
		if(!(new->data = allocateMemory(nelements*format/8)))
			return;

		new->atom = property;
		new->format = format;
		new->num_items = nelements;
		new->type = type;
		memcpy(new->data,data,nelements*format/8);
		new->next = window->props;
		window->props = new;
	}
	else
	{
		switch(mode)
		{
			case PropModeReplace:
			case PropModePrepend:
			case PropModeAppend:
				freeMemory (wanderer->data);
				if (!(wanderer->data = allocateMemory(nelements*format/8)))
					return;
				memcpy(wanderer->data,data,nelements*format/8);
				wanderer->format = format;
				wanderer->type = type;
				wanderer->num_items = nelements;
				break;
		}
	}
}

NT_del_prop_list(prop_list)
struct NT_prop_list *prop_list;
{
	struct NT_prop_list *next_prop_list;
	xtrace("NT_del_prop_list\n");
	while(prop_list)
	{
		next_prop_list = prop_list->next;
		freeMemory (prop_list->data);
		freeMemory (prop_list);
		prop_list = next_prop_list;
	}
	return 0;
}

int
XGetWindowProperty(display,window,property,long_offset,long_length,delete,
                   req_type,actual_type_return,actual_format_return,
                   nitems_return,bytes_after_return,prop_return)
Display *display;
NT_window *window;
Atom property;
long long_offset;
long long_length;
Bool delete;
Atom req_type;
Atom *actual_type_return;
int *actual_format_return;
unsigned long *nitems_return;
unsigned long *bytes_after_return;
unsigned char **prop_return;
{
	struct NT_prop_list *wanderer;
	char *data, *destPtr;
	HGLOBAL handle;
	
	xtrace("XGetWindowProperty\n");
	
	/* here's a hack to fake getting the selection */
	if (property == XA_CUT_BUFFER0)
	{
		if (IsClipboardFormatAvailable(CF_TEXT) &&
			OpenClipboard(NULL)) {
			handle = GetClipboardData(CF_TEXT);
			if (handle != NULL) {
				data = GlobalLock(handle);
				*nitems_return  = strlen(data);
				*prop_return = allocateMemory(*nitems_return+1);
				destPtr = *prop_return;
				while (*data != '\0') {
					if (*data != '\r') {
						*destPtr = *data;
						destPtr++;
					}
					data++;
				}
				*destPtr = '\0';
				GlobalUnlock(handle);
				*actual_type_return = XA_STRING;
				*bytes_after_return = 0;
			} 
			CloseClipboard();
			return 1;
		}
	}

	wanderer = window->props;
	while(wanderer)
	{
		if (wanderer->atom == property)
			break;
		wanderer = wanderer->next;
	}
	*prop_return = NULL;
	*nitems_return = 0;
	if (!wanderer)
		return 0;

	*actual_format_return = wanderer->format;
	*actual_type_return = wanderer->type;
	if (wanderer->type == req_type)
	{
		if((*prop_return = allocateMemory(wanderer->format*wanderer->num_items/8)))
		{
			memcpy(*prop_return,wanderer->data,wanderer->format*wanderer->num_items/8);
			*nitems_return = wanderer->num_items;
			return 0;
		}
	}
	return 0;
}



char **
XListExtensions(display,ret_num)
Display *display;
int *ret_num;
{
	*ret_num = 0;
	xtrace("XListExtensions\n");
	return NULL;
}

XFreeExtensionList(list)
char **list;
{
	xtrace("XFreeExtensionList\n");
	return 0;
}

#if 0
static FILE *pro_out = NULL;

#include <varargs.h> 

int
cjh_printf(fmt,va_alist)
char *fmt;
va_dcl
{
	int ret = 0;
	va_list ap;
	va_start(ap);
	pro_out = fopen("std.out",pro_out?"a":"w");
	ret = vfprintf(pro_out,fmt,ap);
	fclose(pro_out);
	va_end(ap);
	return ret;
}
#endif

XChangeGC(
	Display* display,
	GC gc,
	unsigned long mask,
	XGCValues* gc_values)
{
	xtrace("XChangeGC\n");
	if (mask&GCArcMode)
		gc->values.arc_mode=gc_values->arc_mode;
	if (mask&GCBackground)
		gc->values.background=gc_values->background;
	if (mask&GCCapStyle)
		gc->values.cap_style=gc_values->cap_style;
	if (mask&GCClipMask)
		gc->values.clip_mask=gc_values->clip_mask;
	if (mask&GCClipXOrigin)
		gc->values.clip_x_origin=gc_values->clip_x_origin;
	if (mask&GCClipYOrigin)
		gc->values.clip_y_origin=gc_values->clip_y_origin;
	if (mask&GCDashList)
		gc->values.dashes=gc_values->dashes;
	if (mask&GCDashOffset)
		gc->values.dash_offset=gc_values->dash_offset;
	if (mask&GCFillRule)
		gc->values.fill_rule=gc_values->fill_rule;
	if (mask&GCFillStyle)
		gc->values.fill_style=gc_values->fill_style;
	if (mask&GCFont)
		gc->values.font=gc_values->font;
	if (mask&GCForeground)
		gc->values.foreground=gc_values->foreground;
	if (mask&GCFunction)
		gc->values.function=gc_values->function;
	if (mask&GCGraphicsExposures)
		gc->values.graphics_exposures=gc_values->graphics_exposures;
	if (mask&GCJoinStyle)
		gc->values.join_style=gc_values->join_style;
	if (mask&GCLineStyle)
		gc->values.line_style=gc_values->line_style;
	if (mask&GCLineWidth)
		gc->values.line_width=gc_values->line_width;
	if (mask&GCPlaneMask)
		gc->values.plane_mask=gc_values->plane_mask;
	if (mask&GCStipple)
		gc->values.stipple=gc_values->stipple;
	if (mask&GCSubwindowMode)
		gc->values.subwindow_mode=gc_values->subwindow_mode;
	if (mask&GCTile)
		gc->values.tile=gc_values->tile;
	if (mask&GCTileStipXOrigin)
		gc->values.ts_x_origin=gc_values->ts_x_origin;
	if (mask&GCTileStipYOrigin)
		gc->values.ts_y_origin=gc_values->ts_y_origin;
	gc->dirty |= mask;
	return 0;
}


int
XConnectionNumber(Display* display)
{
	xtrace("XConnectionNumber\n");
	return 0;
/*
	write (so_pair[0],"foobar",6);
	
	return so_pair[1];*/
}

XDrawImageString(
	Display* display,
	Drawable d,
	GC gc,
	int x,
	int y,
	const char* string,
	int length)
{
	HDC hDC = NULL;
	TEXTMETRIC   tmet;
	HFONT old;
	hDC = cjh_get_dc(((NT_window*)d)->w);
	SetBkMode(hDC,OPAQUE);
	SetBkColor(hDC, CNUMTORGB(gc->values.background));	
	SetTextColor(hDC,CNUMTORGB(gc->values.foreground));
	old=SelectObject(hDC,(HFONT)gc->values.font);
    GetTextMetrics(hDC,&tmet);
	xtrace("XDrawImageString\n");
	TextOut( hDC, x, y-tmet.tmAscent, string, length );
	SelectObject(hDC,old);
	return 0;
}

XFreeFont(Display* display,XFontStruct* font_struct)
{
	xtrace("XFreeFont\n");
	return 0;
}

char *
XSetLocaleModifiers(const char* modifier_list)
{
	xtrace("XSetLocaleModifiers\n");
	return NULL;
}
XIM
XOpenIM(
	Display* dpy,
	struct _XrmHashBucketRec* rdb,
	char* res_name,
	char* res_class)
{
	xtrace("XOpenIM\n");
	return 0;
}
char *
XGetIMValues(XIM im , ...)
{
	xtrace("XGetIMValues\n");
	return NULL;
}
XIC
XCreateIC(XIM im , ...)
{
	xtrace("XCreateIC\n");
	return 0;
}
Status
XCloseIM(XIM im)
{
	xtrace("XCloseIM\n");
	return 0;
}
Bool
XFilterEvent(XEvent* event,Window window)
{
	xtrace("XFilterEvent\n");
	return 0;
}

/*
char *
XrmQuarkToString(XrmQuark quark)
{
	xtrace("XrmQuarkToString\n");
	return NULL;
}*/

int
XmbLookupString(
	XIC ic,
	XKeyPressedEvent* event,
	char* buffer_return,
	int bytes_buffer,
	KeySym* keysym_return,
	Status* status_return)
{
	xtrace("XmbLookupString\n");
	return 0;
}
void
XSetICFocus(XIC ic)
{
	xtrace("XSetICFocus\n");
}
void
XUnsetICFocus(XIC ic)
{
	xtrace("XUnsetICFocus\n");
}

Bool
XQueryPointer(
	Display* display,
	Window w,
	Window* root_return,
	Window* child_return,
	int* root_x_return,
	int* root_y_return,
	int* win_x_return,
	int* win_y_return,
	unsigned int* mask_return)
{
    POINT point;
	RECT rect;
    int state = 0;
	xtrace("XQueryPointer\n");
    GetCursorPos(&point);
    *root_x_return = point.x;
    *root_y_return = point.y;
	GetWindowRect(((NT_window*)w)->w,&rect);
	*win_x_return= point.x - rect.left;
	*win_y_return= point.y - rect.top;
    if (GetKeyState(VK_SHIFT)   & 0x8000) state |= ShiftMask;
    if (GetKeyState(VK_CONTROL) & 0x8000) state |= ControlMask;
    if (GetKeyState(VK_MENU)    & 0x8000) state |= Mod2Mask;
    if (GetKeyState(VK_CAPITAL) & 0x0001) state |= LockMask;
    if (GetKeyState(VK_NUMLOCK) & 0x0001) state |= Mod1Mask;
    if (GetKeyState(VK_SCROLL)  & 0x0001) state |= Mod3Mask;
    if (GetKeyState(VK_LBUTTON) & 0x8000) state |= Button1Mask;
    if (GetKeyState(VK_MBUTTON) & 0x8000) state |= Button2Mask;
    if (GetKeyState(VK_RBUTTON) & 0x8000) state |= Button3Mask;
    *mask_return = state;
    return True;
}
/* lifted from emacs */
/*
 *    XParseGeometry parses strings of the form
 *   "=<width>x<height>{+-}<xoffset>{+-}<yoffset>", where
 *   width, height, xoffset, and yoffset are unsigned integers.
 *   Example:  "=80x24+300-49"
 *   The equal sign is optional.
 *   It returns a bitmask that indicates which of the four values
 *   were actually found in the string.  For each value found,
 *   the corresponding argument is updated;  for each value
 *   not found, the corresponding argument is left unchanged. 
 */

static int
read_integer (string, NextString)
     register char *string;
     char **NextString;
{
	register int Result = 0;
	int Sign = 1;
  
	if (*string == '+')
		string++;
	else if (*string == '-')
    {
		string++;
		Sign = -1;
    }
	for (; (*string >= '0') && (*string <= '9'); string++)
    {
		Result = (Result * 10) + (*string - '0');
    }
	*NextString = string;
	if (Sign >= 0)
		return (Result);
	else
		return (-Result);
}

/* lifted from emacs */
int
XParseGeometry(
	const char* string,
	int* x,
	int* y,
	unsigned int* width,
	unsigned int* height)
{
	int mask = NoValue;
	register char *strind;
	unsigned int tempWidth, tempHeight;
	int tempX, tempY;
	char *nextCharacter;
  
	if ((string == NULL) || (*string == '\0')) return (mask);
	if (*string == '=')
		string++;  /* ignore possible '=' at beg of geometry spec */
  
	strind = (char *)string;
	if (*strind != '+' && *strind != '-' && *strind != 'x') 
    {
		tempWidth = read_integer (strind, &nextCharacter);
		if (strind == nextCharacter) 
			return (0);
		strind = nextCharacter;
		mask |= WidthValue;
    }
  
	if (*strind == 'x' || *strind == 'X') 
    {	
		strind++;
		tempHeight = read_integer (strind, &nextCharacter);
		if (strind == nextCharacter)
			return (0);
		strind = nextCharacter;
		mask |= HeightValue;
    }
  
	if ((*strind == '+') || (*strind == '-')) 
    {
		if (*strind == '-') 
		{
			strind++;
			tempX = -read_integer (strind, &nextCharacter);
			if (strind == nextCharacter)
				return (0);
			strind = nextCharacter;
			mask |= XNegative;

		}
		else
		{	
			strind++;
			tempX = read_integer (strind, &nextCharacter);
			if (strind == nextCharacter)
				return (0);
			strind = nextCharacter;
		}
		mask |= XValue;
		if ((*strind == '+') || (*strind == '-')) 
		{
			if (*strind == '-') 
			{
				strind++;
				tempY = -read_integer (strind, &nextCharacter);
				if (strind == nextCharacter)
					return (0);
				strind = nextCharacter;
				mask |= YNegative;

			}
			else
			{
				strind++;
				tempY = read_integer (strind, &nextCharacter);
				if (strind == nextCharacter)
					return (0);
				strind = nextCharacter;
			}
			mask |= YValue;
		}
    }
  
	/* If strind isn't at the end of the string the it's an invalid
	   geometry specification. */
  
	if (*strind != '\0') return (0);
  
	if (mask & XValue)
		*x = tempX;
	if (mask & YValue)
		*y = tempY;
	if (mask & WidthValue)
		*width = tempWidth;
	if (mask & HeightValue)
		*height = tempHeight;
	return (mask);
}

XResizeWindow(
	Display* display,
	NT_window *w,
	unsigned int width,
	unsigned int height)
{
	xtrace("XResizeWindow\n");
	w->wdth = width;
	w->hght = height;
	if (w->w) {
		MoveWindow(w->w, w->x,w->y,w->wdth,w->hght,FALSE);
	}
	return 0;
}

void
XSetWMNormalHints(Display* display,Window w,XSizeHints* hints)
{
	xtrace("XSetWMNormalHints\n");
}

void
XSetWMProperties(
	Display* display,
	Window w,
	XTextProperty* window_name,
	XTextProperty* icon_name,
	char** argv,
	int argc,
	XSizeHints* normal_hints,
	XWMHints* wm_hints,
	XClassHint* class_hints)
{
	xtrace("XSetWMProperties\n");
}
XDefineCursor(Display* display,Window w,Cursor cursor)
{
	xtrace("XDefineCursor\n");
	return 0;
}

void 
XMoveResizeWindow(
	Display* display,
	NT_window *w,
	int x,
	int y,
	unsigned int width,
	unsigned int height)
{
	xtrace("XMoveResizeWindow\n");
	w->x = x;
	w->y = y;
	w->wdth = width;
	w->hght = height;
	if (w->w) {
		MoveWindow(w->w, x,y,width,height,FALSE);
	}
	
}
XMoveWindow(
	Display* display,
	NT_window *w,
	int x,
	int y)
{
	xtrace("XMoveWindow\n");
	w->x = x;
	w->y = y;
	if (w->w) {
		MoveWindow(w->w, x,y,w->wdth,w->hght,FALSE);
	}
	return 0;
}

/* 
 * xcolors.c --
 *
 *	This file contains the routines used to map from X color
 *	names to RGB and pixel values.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) xcolors.c 1.3 96/12/17 13:07:02
 */

/*
 * Define an array that defines the mapping from color names to RGB values.
 * Note that this array must be kept sorted alphabetically so that the
 * binary search used in XParseColor will succeed.
 */
typedef struct {
    char *name;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} XColorEntry;

static XColorEntry xColors[] = {
    "alice blue", 240, 248, 255,
    "AliceBlue", 240, 248, 255,
    "antique white", 250, 235, 215,
    "AntiqueWhite", 250, 235, 215,
    "AntiqueWhite1", 255, 239, 219,
    "AntiqueWhite2", 238, 223, 204,
    "AntiqueWhite3", 205, 192, 176,
    "AntiqueWhite4", 139, 131, 120,
    "aquamarine", 127, 255, 212,
    "aquamarine1", 127, 255, 212,
    "aquamarine2", 118, 238, 198,
    "aquamarine3", 102, 205, 170,
    "aquamarine4", 69, 139, 116,
    "azure", 240, 255, 255,
    "azure1", 240, 255, 255,
    "azure2", 224, 238, 238,
    "azure3", 193, 205, 205,
    "azure4", 131, 139, 139,
    "beige", 245, 245, 220,
    "bisque", 255, 228, 196,
    "bisque1", 255, 228, 196,
    "bisque2", 238, 213, 183,
    "bisque3", 205, 183, 158,
    "bisque4", 139, 125, 107,
    "black", 0, 0, 0,
    "blanched almond", 255, 235, 205,
    "BlanchedAlmond", 255, 235, 205,
    "blue", 0, 0, 255,
    "blue violet", 138, 43, 226,
    "blue1", 0, 0, 255,
    "blue2", 0, 0, 238,
    "blue3", 0, 0, 205,
    "blue4", 0, 0, 139,
    "BlueViolet", 138, 43, 226,
    "brown", 165, 42, 42,
    "brown1", 255, 64, 64,
    "brown2", 238, 59, 59,
    "brown3", 205, 51, 51,
    "brown4", 139, 35, 35,
    "burlywood", 222, 184, 135,
    "burlywood1", 255, 211, 155,
    "burlywood2", 238, 197, 145,
    "burlywood3", 205, 170, 125,
    "burlywood4", 139, 115, 85,
    "cadet blue", 95, 158, 160,
    "CadetBlue", 95, 158, 160,
    "CadetBlue1", 152, 245, 255,
    "CadetBlue2", 142, 229, 238,
    "CadetBlue3", 122, 197, 205,
    "CadetBlue4", 83, 134, 139,
    "chartreuse", 127, 255, 0,
    "chartreuse1", 127, 255, 0,
    "chartreuse2", 118, 238, 0,
    "chartreuse3", 102, 205, 0,
    "chartreuse4", 69, 139, 0,
    "chocolate", 210, 105, 30,
    "chocolate1", 255, 127, 36,
    "chocolate2", 238, 118, 33,
    "chocolate3", 205, 102, 29,
    "chocolate4", 139, 69, 19,
    "coral", 255, 127, 80,
    "coral1", 255, 114, 86,
    "coral2", 238, 106, 80,
    "coral3", 205, 91, 69,
    "coral4", 139, 62, 47,
    "cornflower blue", 100, 149, 237,
    "CornflowerBlue", 100, 149, 237,
    "cornsilk", 255, 248, 220,
    "cornsilk1", 255, 248, 220,
    "cornsilk2", 238, 232, 205,
    "cornsilk3", 205, 200, 177,
    "cornsilk4", 139, 136, 120,
    "cyan", 0, 255, 255,
    "cyan1", 0, 255, 255,
    "cyan2", 0, 238, 238,
    "cyan3", 0, 205, 205,
    "cyan4", 0, 139, 139,
    "dark goldenrod", 184, 134, 11,
    "dark green", 0, 100, 0,
    "dark khaki", 189, 183, 107,
    "dark olive green", 85, 107, 47,
    "dark orange", 255, 140, 0,
    "dark orchid", 153, 50, 204,
    "dark salmon", 233, 150, 122,
    "dark sea green", 143, 188, 143,
    "dark slate blue", 72, 61, 139,
    "dark slate gray", 47, 79, 79,
    "dark slate grey", 47, 79, 79,
    "dark turquoise", 0, 206, 209,
    "dark violet", 148, 0, 211,
    "DarkGoldenrod", 184, 134, 11,
    "DarkGoldenrod1", 255, 185, 15,
    "DarkGoldenrod2", 238, 173, 14,
    "DarkGoldenrod3", 205, 149, 12,
    "DarkGoldenrod4", 139, 101, 8,
    "DarkGreen", 0, 100, 0,
    "DarkKhaki", 189, 183, 107,
    "DarkOliveGreen", 85, 107, 47,
    "DarkOliveGreen1", 202, 255, 112,
    "DarkOliveGreen2", 188, 238, 104,
    "DarkOliveGreen3", 162, 205, 90,
    "DarkOliveGreen4", 110, 139, 61,
    "DarkOrange", 255, 140, 0,
    "DarkOrange1", 255, 127, 0,
    "DarkOrange2", 238, 118, 0,
    "DarkOrange3", 205, 102, 0,
    "DarkOrange4", 139, 69, 0,
    "DarkOrchid", 153, 50, 204,
    "DarkOrchid1", 191, 62, 255,
    "DarkOrchid2", 178, 58, 238,
    "DarkOrchid3", 154, 50, 205,
    "DarkOrchid4", 104, 34, 139,
    "DarkSalmon", 233, 150, 122,
    "DarkSeaGreen", 143, 188, 143,
    "DarkSeaGreen1", 193, 255, 193,
    "DarkSeaGreen2", 180, 238, 180,
    "DarkSeaGreen3", 155, 205, 155,
    "DarkSeaGreen4", 105, 139, 105,
    "DarkSlateBlue", 72, 61, 139,
    "DarkSlateGray", 47, 79, 79,
    "DarkSlateGray1", 151, 255, 255,
    "DarkSlateGray2", 141, 238, 238,
    "DarkSlateGray3", 121, 205, 205,
    "DarkSlateGray4", 82, 139, 139,
    "DarkSlateGrey", 47, 79, 79,
    "DarkTurquoise", 0, 206, 209,
    "DarkViolet", 148, 0, 211,
    "deep pink", 255, 20, 147,
    "deep sky blue", 0, 191, 255,
    "DeepPink", 255, 20, 147,
    "DeepPink1", 255, 20, 147,
    "DeepPink2", 238, 18, 137,
    "DeepPink3", 205, 16, 118,
    "DeepPink4", 139, 10, 80,
    "DeepSkyBlue", 0, 191, 255,
    "DeepSkyBlue1", 0, 191, 255,
    "DeepSkyBlue2", 0, 178, 238,
    "DeepSkyBlue3", 0, 154, 205,
    "DeepSkyBlue4", 0, 104, 139,
    "dim gray", 105, 105, 105,
    "dim grey", 105, 105, 105,
    "DimGray", 105, 105, 105,
    "DimGrey", 105, 105, 105,
    "dodger blue", 30, 144, 255,
    "DodgerBlue", 30, 144, 255,
    "DodgerBlue1", 30, 144, 255,
    "DodgerBlue2", 28, 134, 238,
    "DodgerBlue3", 24, 116, 205,
    "DodgerBlue4", 16, 78, 139,
    "firebrick", 178, 34, 34,
    "firebrick1", 255, 48, 48,
    "firebrick2", 238, 44, 44,
    "firebrick3", 205, 38, 38,
    "firebrick4", 139, 26, 26,
    "floral white", 255, 250, 240,
    "FloralWhite", 255, 250, 240,
    "forest green", 34, 139, 34,
    "ForestGreen", 34, 139, 34,
    "gainsboro", 220, 220, 220,
    "ghost white", 248, 248, 255,
    "GhostWhite", 248, 248, 255,
    "gold", 255, 215, 0,
    "gold1", 255, 215, 0,
    "gold2", 238, 201, 0,
    "gold3", 205, 173, 0,
    "gold4", 139, 117, 0,
    "goldenrod", 218, 165, 32,
    "goldenrod1", 255, 193, 37,
    "goldenrod2", 238, 180, 34,
    "goldenrod3", 205, 155, 29,
    "goldenrod4", 139, 105, 20,
    "gray", 190, 190, 190,
    "gray0", 0, 0, 0,
    "gray1", 3, 3, 3,
    "gray10", 26, 26, 26,
    "gray100", 255, 255, 255,
    "gray11", 28, 28, 28,
    "gray12", 31, 31, 31,
    "gray13", 33, 33, 33,
    "gray14", 36, 36, 36,
    "gray15", 38, 38, 38,
    "gray16", 41, 41, 41,
    "gray17", 43, 43, 43,
    "gray18", 46, 46, 46,
    "gray19", 48, 48, 48,
    "gray2", 5, 5, 5,
    "gray20", 51, 51, 51,
    "gray21", 54, 54, 54,
    "gray22", 56, 56, 56,
    "gray23", 59, 59, 59,
    "gray24", 61, 61, 61,
    "gray25", 64, 64, 64,
    "gray26", 66, 66, 66,
    "gray27", 69, 69, 69,
    "gray28", 71, 71, 71,
    "gray29", 74, 74, 74,
    "gray3", 8, 8, 8,
    "gray30", 77, 77, 77,
    "gray31", 79, 79, 79,
    "gray32", 82, 82, 82,
    "gray33", 84, 84, 84,
    "gray34", 87, 87, 87,
    "gray35", 89, 89, 89,
    "gray36", 92, 92, 92,
    "gray37", 94, 94, 94,
    "gray38", 97, 97, 97,
    "gray39", 99, 99, 99,
    "gray4", 10, 10, 10,
    "gray40", 102, 102, 102,
    "gray41", 105, 105, 105,
    "gray42", 107, 107, 107,
    "gray43", 110, 110, 110,
    "gray44", 112, 112, 112,
    "gray45", 115, 115, 115,
    "gray46", 117, 117, 117,
    "gray47", 120, 120, 120,
    "gray48", 122, 122, 122,
    "gray49", 125, 125, 125,
    "gray5", 13, 13, 13,
    "gray50", 127, 127, 127,
    "gray51", 130, 130, 130,
    "gray52", 133, 133, 133,
    "gray53", 135, 135, 135,
    "gray54", 138, 138, 138,
    "gray55", 140, 140, 140,
    "gray56", 143, 143, 143,
    "gray57", 145, 145, 145,
    "gray58", 148, 148, 148,
    "gray59", 150, 150, 150,
    "gray6", 15, 15, 15,
    "gray60", 153, 153, 153,
    "gray61", 156, 156, 156,
    "gray62", 158, 158, 158,
    "gray63", 161, 161, 161,
    "gray64", 163, 163, 163,
    "gray65", 166, 166, 166,
    "gray66", 168, 168, 168,
    "gray67", 171, 171, 171,
    "gray68", 173, 173, 173,
    "gray69", 176, 176, 176,
    "gray7", 18, 18, 18,
    "gray70", 179, 179, 179,
    "gray71", 181, 181, 181,
    "gray72", 184, 184, 184,
    "gray73", 186, 186, 186,
    "gray74", 189, 189, 189,
    "gray75", 191, 191, 191,
    "gray76", 194, 194, 194,
    "gray77", 196, 196, 196,
    "gray78", 199, 199, 199,
    "gray79", 201, 201, 201,
    "gray8", 20, 20, 20,
    "gray80", 204, 204, 204,
    "gray81", 207, 207, 207,
    "gray82", 209, 209, 209,
    "gray83", 212, 212, 212,
    "gray84", 214, 214, 214,
    "gray85", 217, 217, 217,
    "gray86", 219, 219, 219,
    "gray87", 222, 222, 222,
    "gray88", 224, 224, 224,
    "gray89", 227, 227, 227,
    "gray9", 23, 23, 23,
    "gray90", 229, 229, 229,
    "gray91", 232, 232, 232,
    "gray92", 235, 235, 235,
    "gray93", 237, 237, 237,
    "gray94", 240, 240, 240,
    "gray95", 242, 242, 242,
    "gray96", 245, 245, 245,
    "gray97", 247, 247, 247,
    "gray98", 250, 250, 250,
    "gray99", 252, 252, 252,
    "green", 0, 255, 0,
    "green yellow", 173, 255, 47,
    "green1", 0, 255, 0,
    "green2", 0, 238, 0,
    "green3", 0, 205, 0,
    "green4", 0, 139, 0,
    "GreenYellow", 173, 255, 47,
    "grey", 190, 190, 190,
    "grey0", 0, 0, 0,
    "grey1", 3, 3, 3,
    "grey10", 26, 26, 26,
    "grey100", 255, 255, 255,
    "grey11", 28, 28, 28,
    "grey12", 31, 31, 31,
    "grey13", 33, 33, 33,
    "grey14", 36, 36, 36,
    "grey15", 38, 38, 38,
    "grey16", 41, 41, 41,
    "grey17", 43, 43, 43,
    "grey18", 46, 46, 46,
    "grey19", 48, 48, 48,
    "grey2", 5, 5, 5,
    "grey20", 51, 51, 51,
    "grey21", 54, 54, 54,
    "grey22", 56, 56, 56,
    "grey23", 59, 59, 59,
    "grey24", 61, 61, 61,
    "grey25", 64, 64, 64,
    "grey26", 66, 66, 66,
    "grey27", 69, 69, 69,
    "grey28", 71, 71, 71,
    "grey29", 74, 74, 74,
    "grey3", 8, 8, 8,
    "grey30", 77, 77, 77,
    "grey31", 79, 79, 79,
    "grey32", 82, 82, 82,
    "grey33", 84, 84, 84,
    "grey34", 87, 87, 87,
    "grey35", 89, 89, 89,
    "grey36", 92, 92, 92,
    "grey37", 94, 94, 94,
    "grey38", 97, 97, 97,
    "grey39", 99, 99, 99,
    "grey4", 10, 10, 10,
    "grey40", 102, 102, 102,
    "grey41", 105, 105, 105,
    "grey42", 107, 107, 107,
    "grey43", 110, 110, 110,
    "grey44", 112, 112, 112,
    "grey45", 115, 115, 115,
    "grey46", 117, 117, 117,
    "grey47", 120, 120, 120,
    "grey48", 122, 122, 122,
    "grey49", 125, 125, 125,
    "grey5", 13, 13, 13,
    "grey50", 127, 127, 127,
    "grey51", 130, 130, 130,
    "grey52", 133, 133, 133,
    "grey53", 135, 135, 135,
    "grey54", 138, 138, 138,
    "grey55", 140, 140, 140,
    "grey56", 143, 143, 143,
    "grey57", 145, 145, 145,
    "grey58", 148, 148, 148,
    "grey59", 150, 150, 150,
    "grey6", 15, 15, 15,
    "grey60", 153, 153, 153,
    "grey61", 156, 156, 156,
    "grey62", 158, 158, 158,
    "grey63", 161, 161, 161,
    "grey64", 163, 163, 163,
    "grey65", 166, 166, 166,
    "grey66", 168, 168, 168,
    "grey67", 171, 171, 171,
    "grey68", 173, 173, 173,
    "grey69", 176, 176, 176,
    "grey7", 18, 18, 18,
    "grey70", 179, 179, 179,
    "grey71", 181, 181, 181,
    "grey72", 184, 184, 184,
    "grey73", 186, 186, 186,
    "grey74", 189, 189, 189,
    "grey75", 191, 191, 191,
    "grey76", 194, 194, 194,
    "grey77", 196, 196, 196,
    "grey78", 199, 199, 199,
    "grey79", 201, 201, 201,
    "grey8", 20, 20, 20,
    "grey80", 204, 204, 204,
    "grey81", 207, 207, 207,
    "grey82", 209, 209, 209,
    "grey83", 212, 212, 212,
    "grey84", 214, 214, 214,
    "grey85", 217, 217, 217,
    "grey86", 219, 219, 219,
    "grey87", 222, 222, 222,
    "grey88", 224, 224, 224,
    "grey89", 227, 227, 227,
    "grey9", 23, 23, 23,
    "grey90", 229, 229, 229,
    "grey91", 232, 232, 232,
    "grey92", 235, 235, 235,
    "grey93", 237, 237, 237,
    "grey94", 240, 240, 240,
    "grey95", 242, 242, 242,
    "grey96", 245, 245, 245,
    "grey97", 247, 247, 247,
    "grey98", 250, 250, 250,
    "grey99", 252, 252, 252,
    "honeydew", 240, 255, 240,
    "honeydew1", 240, 255, 240,
    "honeydew2", 224, 238, 224,
    "honeydew3", 193, 205, 193,
    "honeydew4", 131, 139, 131,
    "hot pink", 255, 105, 180,
    "HotPink", 255, 105, 180,
    "HotPink1", 255, 110, 180,
    "HotPink2", 238, 106, 167,
    "HotPink3", 205, 96, 144,
    "HotPink4", 139, 58, 98,
    "indian red", 205, 92, 92,
    "IndianRed", 205, 92, 92,
    "IndianRed1", 255, 106, 106,
    "IndianRed2", 238, 99, 99,
    "IndianRed3", 205, 85, 85,
    "IndianRed4", 139, 58, 58,
    "ivory", 255, 255, 240,
    "ivory1", 255, 255, 240,
    "ivory2", 238, 238, 224,
    "ivory3", 205, 205, 193,
    "ivory4", 139, 139, 131,
    "khaki", 240, 230, 140,
    "khaki1", 255, 246, 143,
    "khaki2", 238, 230, 133,
    "khaki3", 205, 198, 115,
    "khaki4", 139, 134, 78,
    "lavender", 230, 230, 250,
    "lavender blush", 255, 240, 245,
    "LavenderBlush", 255, 240, 245,
    "LavenderBlush1", 255, 240, 245,
    "LavenderBlush2", 238, 224, 229,
    "LavenderBlush3", 205, 193, 197,
    "LavenderBlush4", 139, 131, 134,
    "lawn green", 124, 252, 0,
    "LawnGreen", 124, 252, 0,
    "lemon chiffon", 255, 250, 205,
    "LemonChiffon", 255, 250, 205,
    "LemonChiffon1", 255, 250, 205,
    "LemonChiffon2", 238, 233, 191,
    "LemonChiffon3", 205, 201, 165,
    "LemonChiffon4", 139, 137, 112,
    "light blue", 173, 216, 230,
    "light coral", 240, 128, 128,
    "light cyan", 224, 255, 255,
    "light goldenrod", 238, 221, 130,
    "light goldenrod yellow", 250, 250, 210,
    "light gray", 211, 211, 211,
    "light grey", 211, 211, 211,
    "light pink", 255, 182, 193,
    "light salmon", 255, 160, 122,
    "light sea green", 32, 178, 170,
    "light sky blue", 135, 206, 250,
    "light slate blue", 132, 112, 255,
    "light slate gray", 119, 136, 153,
    "light slate grey", 119, 136, 153,
    "light steel blue", 176, 196, 222,
    "light yellow", 255, 255, 224,
    "LightBlue", 173, 216, 230,
    "LightBlue1", 191, 239, 255,
    "LightBlue2", 178, 223, 238,
    "LightBlue3", 154, 192, 205,
    "LightBlue4", 104, 131, 139,
    "LightCoral", 240, 128, 128,
    "LightCyan", 224, 255, 255,
    "LightCyan1", 224, 255, 255,
    "LightCyan2", 209, 238, 238,
    "LightCyan3", 180, 205, 205,
    "LightCyan4", 122, 139, 139,
    "LightGoldenrod", 238, 221, 130,
    "LightGoldenrod1", 255, 236, 139,
    "LightGoldenrod2", 238, 220, 130,
    "LightGoldenrod3", 205, 190, 112,
    "LightGoldenrod4", 139, 129, 76,
    "LightGoldenrodYellow", 250, 250, 210,
    "LightGray", 211, 211, 211,
    "LightGrey", 211, 211, 211,
    "LightPink", 255, 182, 193,
    "LightPink1", 255, 174, 185,
    "LightPink2", 238, 162, 173,
    "LightPink3", 205, 140, 149,
    "LightPink4", 139, 95, 101,
    "LightSalmon", 255, 160, 122,
    "LightSalmon1", 255, 160, 122,
    "LightSalmon2", 238, 149, 114,
    "LightSalmon3", 205, 129, 98,
    "LightSalmon4", 139, 87, 66,
    "LightSeaGreen", 32, 178, 170,
    "LightSkyBlue", 135, 206, 250,
    "LightSkyBlue1", 176, 226, 255,
    "LightSkyBlue2", 164, 211, 238,
    "LightSkyBlue3", 141, 182, 205,
    "LightSkyBlue4", 96, 123, 139,
    "LightSlateBlue", 132, 112, 255,
    "LightSlateGray", 119, 136, 153,
    "LightSlateGrey", 119, 136, 153,
    "LightSteelBlue", 176, 196, 222,
    "LightSteelBlue1", 202, 225, 255,
    "LightSteelBlue2", 188, 210, 238,
    "LightSteelBlue3", 162, 181, 205,
    "LightSteelBlue4", 110, 123, 139,
    "LightYellow", 255, 255, 224,
    "LightYellow1", 255, 255, 224,
    "LightYellow2", 238, 238, 209,
    "LightYellow3", 205, 205, 180,
    "LightYellow4", 139, 139, 122,
    "lime green", 50, 205, 50,
    "LimeGreen", 50, 205, 50,
    "linen", 250, 240, 230,
    "magenta", 255, 0, 255,
    "magenta1", 255, 0, 255,
    "magenta2", 238, 0, 238,
    "magenta3", 205, 0, 205,
    "magenta4", 139, 0, 139,
    "maroon", 176, 48, 96,
    "maroon1", 255, 52, 179,
    "maroon2", 238, 48, 167,
    "maroon3", 205, 41, 144,
    "maroon4", 139, 28, 98,
    "medium aquamarine", 102, 205, 170,
    "medium blue", 0, 0, 205,
    "medium orchid", 186, 85, 211,
    "medium purple", 147, 112, 219,
    "medium sea green", 60, 179, 113,
    "medium slate blue", 123, 104, 238,
    "medium spring green", 0, 250, 154,
    "medium turquoise", 72, 209, 204,
    "medium violet red", 199, 21, 133,
    "MediumAquamarine", 102, 205, 170,
    "MediumBlue", 0, 0, 205,
    "MediumOrchid", 186, 85, 211,
    "MediumOrchid1", 224, 102, 255,
    "MediumOrchid2", 209, 95, 238,
    "MediumOrchid3", 180, 82, 205,
    "MediumOrchid4", 122, 55, 139,
    "MediumPurple", 147, 112, 219,
    "MediumPurple1", 171, 130, 255,
    "MediumPurple2", 159, 121, 238,
    "MediumPurple3", 137, 104, 205,
    "MediumPurple4", 93, 71, 139,
    "MediumSeaGreen", 60, 179, 113,
    "MediumSlateBlue", 123, 104, 238,
    "MediumSpringGreen", 0, 250, 154,
    "MediumTurquoise", 72, 209, 204,
    "MediumVioletRed", 199, 21, 133,
    "midnight blue", 25, 25, 112,
    "MidnightBlue", 25, 25, 112,
    "mint cream", 245, 255, 250,
    "MintCream", 245, 255, 250,
    "misty rose", 255, 228, 225,
    "MistyRose", 255, 228, 225,
    "MistyRose1", 255, 228, 225,
    "MistyRose2", 238, 213, 210,
    "MistyRose3", 205, 183, 181,
    "MistyRose4", 139, 125, 123,
    "moccasin", 255, 228, 181,
    "navajo white", 255, 222, 173,
    "NavajoWhite", 255, 222, 173,
    "NavajoWhite1", 255, 222, 173,
    "NavajoWhite2", 238, 207, 161,
    "NavajoWhite3", 205, 179, 139,
    "NavajoWhite4", 139, 121, 94,
    "navy", 0, 0, 128,
    "navy blue", 0, 0, 128,
    "NavyBlue", 0, 0, 128,
    "old lace", 253, 245, 230,
    "OldLace", 253, 245, 230,
    "olive drab", 107, 142, 35,
    "OliveDrab", 107, 142, 35,
    "OliveDrab1", 192, 255, 62,
    "OliveDrab2", 179, 238, 58,
    "OliveDrab3", 154, 205, 50,
    "OliveDrab4", 105, 139, 34,
    "orange", 255, 165, 0,
    "orange red", 255, 69, 0,
    "orange1", 255, 165, 0,
    "orange2", 238, 154, 0,
    "orange3", 205, 133, 0,
    "orange4", 139, 90, 0,
    "OrangeRed", 255, 69, 0,
    "OrangeRed1", 255, 69, 0,
    "OrangeRed2", 238, 64, 0,
    "OrangeRed3", 205, 55, 0,
    "OrangeRed4", 139, 37, 0,
    "orchid", 218, 112, 214,
    "orchid1", 255, 131, 250,
    "orchid2", 238, 122, 233,
    "orchid3", 205, 105, 201,
    "orchid4", 139, 71, 137,
    "pale goldenrod", 238, 232, 170,
    "pale green", 152, 251, 152,
    "pale turquoise", 175, 238, 238,
    "pale violet red", 219, 112, 147,
    "PaleGoldenrod", 238, 232, 170,
    "PaleGreen", 152, 251, 152,
    "PaleGreen1", 154, 255, 154,
    "PaleGreen2", 144, 238, 144,
    "PaleGreen3", 124, 205, 124,
    "PaleGreen4", 84, 139, 84,
    "PaleTurquoise", 175, 238, 238,
    "PaleTurquoise1", 187, 255, 255,
    "PaleTurquoise2", 174, 238, 238,
    "PaleTurquoise3", 150, 205, 205,
    "PaleTurquoise4", 102, 139, 139,
    "PaleVioletRed", 219, 112, 147,
    "PaleVioletRed1", 255, 130, 171,
    "PaleVioletRed2", 238, 121, 159,
    "PaleVioletRed3", 205, 104, 137,
    "PaleVioletRed4", 139, 71, 93,
    "papaya whip", 255, 239, 213,
    "PapayaWhip", 255, 239, 213,
    "peach puff", 255, 218, 185,
    "PeachPuff", 255, 218, 185,
    "PeachPuff1", 255, 218, 185,
    "PeachPuff2", 238, 203, 173,
    "PeachPuff3", 205, 175, 149,
    "PeachPuff4", 139, 119, 101,
    "peru", 205, 133, 63,
    "pink", 255, 192, 203,
    "pink1", 255, 181, 197,
    "pink2", 238, 169, 184,
    "pink3", 205, 145, 158,
    "pink4", 139, 99, 108,
    "plum", 221, 160, 221,
    "plum1", 255, 187, 255,
    "plum2", 238, 174, 238,
    "plum3", 205, 150, 205,
    "plum4", 139, 102, 139,
    "powder blue", 176, 224, 230,
    "PowderBlue", 176, 224, 230,
    "purple", 160, 32, 240,
    "purple1", 155, 48, 255,
    "purple2", 145, 44, 238,
    "purple3", 125, 38, 205,
    "purple4", 85, 26, 139,
    "red", 255, 0, 0,
    "red1", 255, 0, 0,
    "red2", 238, 0, 0,
    "red3", 205, 0, 0,
    "red4", 139, 0, 0,
    "rosy brown", 188, 143, 143,
    "RosyBrown", 188, 143, 143,
    "RosyBrown1", 255, 193, 193,
    "RosyBrown2", 238, 180, 180,
    "RosyBrown3", 205, 155, 155,
    "RosyBrown4", 139, 105, 105,
    "royal blue", 65, 105, 225,
    "RoyalBlue", 65, 105, 225,
    "RoyalBlue1", 72, 118, 255,
    "RoyalBlue2", 67, 110, 238,
    "RoyalBlue3", 58, 95, 205,
    "RoyalBlue4", 39, 64, 139,
    "saddle brown", 139, 69, 19,
    "SaddleBrown", 139, 69, 19,
    "salmon", 250, 128, 114,
    "salmon1", 255, 140, 105,
    "salmon2", 238, 130, 98,
    "salmon3", 205, 112, 84,
    "salmon4", 139, 76, 57,
    "sandy brown", 244, 164, 96,
    "SandyBrown", 244, 164, 96,
    "sea green", 46, 139, 87,
    "SeaGreen", 46, 139, 87,
    "SeaGreen1", 84, 255, 159,
    "SeaGreen2", 78, 238, 148,
    "SeaGreen3", 67, 205, 128,
    "SeaGreen4", 46, 139, 87,
    "seashell", 255, 245, 238,
    "seashell1", 255, 245, 238,
    "seashell2", 238, 229, 222,
    "seashell3", 205, 197, 191,
    "seashell4", 139, 134, 130,
    "sienna", 160, 82, 45,
    "sienna1", 255, 130, 71,
    "sienna2", 238, 121, 66,
    "sienna3", 205, 104, 57,
    "sienna4", 139, 71, 38,
    "sky blue", 135, 206, 235,
    "SkyBlue", 135, 206, 235,
    "SkyBlue1", 135, 206, 255,
    "SkyBlue2", 126, 192, 238,
    "SkyBlue3", 108, 166, 205,
    "SkyBlue4", 74, 112, 139,
    "slate blue", 106, 90, 205,
    "slate gray", 112, 128, 144,
    "slate grey", 112, 128, 144,
    "SlateBlue", 106, 90, 205,
    "SlateBlue1", 131, 111, 255,
    "SlateBlue2", 122, 103, 238,
    "SlateBlue3", 105, 89, 205,
    "SlateBlue4", 71, 60, 139,
    "SlateGray", 112, 128, 144,
    "SlateGray1", 198, 226, 255,
    "SlateGray2", 185, 211, 238,
    "SlateGray3", 159, 182, 205,
    "SlateGray4", 108, 123, 139,
    "SlateGrey", 112, 128, 144,
    "snow", 255, 250, 250,
    "snow1", 255, 250, 250,
    "snow2", 238, 233, 233,
    "snow3", 205, 201, 201,
    "snow4", 139, 137, 137,
    "spring green", 0, 255, 127,
    "SpringGreen", 0, 255, 127,
    "SpringGreen1", 0, 255, 127,
    "SpringGreen2", 0, 238, 118,
    "SpringGreen3", 0, 205, 102,
    "SpringGreen4", 0, 139, 69,
    "steel blue", 70, 130, 180,
    "SteelBlue", 70, 130, 180,
    "SteelBlue1", 99, 184, 255,
    "SteelBlue2", 92, 172, 238,
    "SteelBlue3", 79, 148, 205,
    "SteelBlue4", 54, 100, 139,
    "tan", 210, 180, 140,
    "tan1", 255, 165, 79,
    "tan2", 238, 154, 73,
    "tan3", 205, 133, 63,
    "tan4", 139, 90, 43,
    "thistle", 216, 191, 216,
    "thistle1", 255, 225, 255,
    "thistle2", 238, 210, 238,
    "thistle3", 205, 181, 205,
    "thistle4", 139, 123, 139,
    "tomato", 255, 99, 71,
    "tomato1", 255, 99, 71,
    "tomato2", 238, 92, 66,
    "tomato3", 205, 79, 57,
    "tomato4", 139, 54, 38,
    "turquoise", 64, 224, 208,
    "turquoise1", 0, 245, 255,
    "turquoise2", 0, 229, 238,
    "turquoise3", 0, 197, 205,
    "turquoise4", 0, 134, 139,
    "violet", 238, 130, 238,
    "violet red", 208, 32, 144,
    "VioletRed", 208, 32, 144,
    "VioletRed1", 255, 62, 150,
    "VioletRed2", 238, 58, 140,
    "VioletRed3", 205, 50, 120,
    "VioletRed4", 139, 34, 82,
    "wheat", 245, 222, 179,
    "wheat1", 255, 231, 186,
    "wheat2", 238, 216, 174,
    "wheat3", 205, 186, 150,
    "wheat4", 139, 126, 102,
    "white", 255, 255, 255,
    "white smoke", 245, 245, 245,
    "WhiteSmoke", 245, 245, 245,
    "yellow", 255, 255, 0,
    "yellow green", 154, 205, 50,
    "yellow1", 255, 255, 0,
    "yellow2", 238, 238, 0,
    "yellow3", 205, 205, 0,
    "yellow4", 139, 139, 0,
    "YellowGreen", 154, 205, 50,
    NULL, 0, 0, 0
};


/*
 * This value will be set to the number of colors in the color table
 * the first time it is needed.
 */

static int numXColors = 0;

/*
 * Forward declarations for functions used only in this file.
 */

static int	FindColor(const char *name, XColor *colorPtr);

int strcasecmp(const char *a, const char *b)
{
	int i=0,c;
	if((a==NULL)||(b==NULL)) return -1;
	
	while(((!(c=toupper(a[i])-toupper(b[i])))&&a[i]&&b[i])) i++;
	return c;
}
/*
 *----------------------------------------------------------------------
 *
 * FindColor --
 *
 *	This routine finds the color entry that corresponds to the
 *	specified color.
 *
 * Results:
 *	Returns non-zero on success.  The RGB values of the XColor
 *	will be initialized to the proper values on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
FindColor(name, colorPtr)
    const char *name;
    XColor *colorPtr;
{
    int l, u, r, i;

    /*
     * Count the number of elements in the color array if we haven't
     * done so yet.
     */

    if (numXColors == 0) {
	XColorEntry *ePtr;
	for (ePtr = xColors; ePtr->name != NULL; ePtr++) {
	    numXColors++;
	}
    }

    /*
     * Perform a binary search on the sorted array of colors.
     */

    l = 0;
    u = numXColors - 1;
    while (l <= u) {
	i = (l + u) / 2;
	r = strcasecmp(name, xColors[i].name);
	if (r == 0) {
	    break;
	} else if (r < 0) {
	    u = i-1;
	} else {
	    l = i+1;
	}
    }
    if (l > u) {
	return 0;
    }
    colorPtr->red = xColors[i].red << 8;
    colorPtr->green = xColors[i].green << 8;
    colorPtr->blue = xColors[i].blue << 8;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XParseColor --
 *
 *	Partial implementation of X color name parsing interface.
 *
 * Results:
 *	Returns non-zero on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Status
XParseColor(display, map, spec, colorPtr)
    Display *display;
    Colormap map;
    const char* spec;
    XColor *colorPtr;
{
    if (spec[0] == '#') {
	char fmt[16];
	int i, red, green, blue;
	
	if ((i = strlen(spec+1))%3) {
	    return 0;
	}
	i /= 3;

	sprintf(fmt, "%%%dx%%%dx%%%dx", i, i, i);
	if (sscanf(spec+1, fmt, &red, &green, &blue) != 3) {
	    return 0;
	}
	colorPtr->red = ((unsigned short) red) << (4 * (4 - i));
	colorPtr->green = ((unsigned short) green) << (4 * (4 - i));
	colorPtr->blue = ((unsigned short) blue) << (4 * (4 - i));
    } else {
	if (!FindColor(spec, colorPtr)) {
	    return 0;
	}
    }
    colorPtr->pixel = ((colorPtr->red)>>8)&0xff;
    colorPtr->flags = DoRed|DoGreen|DoBlue;
    colorPtr->pad = 0;
    return 1;
}

#ifdef ARPUS_CE
/***************************************************************
*  
*  Routines added by Robert Styma to support extra functionality
*  required by ARPUS/Ce.
*
***************************************************************/
                       
char *XDisplayName(const char  *dpyname)   /* string */
{
}/* end of XDisplayName */

XSetWindowBorder(Display      *display,
                 Window        w         /* w */,
                 unsigned long border_pixel) /* border_pixel */
{
}/* end of XSetWindowBorder */

Status XGetGCValues(Display       *display,
                    GC             gc,
                    unsigned long  valuemask ,
                    XGCValues     *values_return)
{
}/* end of XGetGCValues */

char **XListFonts(Display       *display,
                  const char    *pattern,
                  int            maxnames,
                  int           *actual_count_return)
{
}/* end of XListFonts */

XFreeColors(Display       *display,
            Colormap       colormap,
            unsigned long *pixels,
            int            npixels,
            unsigned long  planes)
{
}/* end of XFreeColors */

Status XGetWMName(Display       *display,
                  Window         w,
                  XTextProperty *text_prop_return)
{
}/* end of XGetWMName */

XSetStandardProperties(Display       *display,
                       Window         w,
                       const char    *window_name,
                       const char    *icon_name,
                       Pixmap         icon_pixmap,
                       char         **argv,
                       int            argc,
                       XSizeHints    *hints)
{
}/* end of XSetStandardProperties */

Cursor XCreatePixmapCursor(Display       *display,
                           Pixmap         source,
                           Pixmap         mask,
                           XColor        *foreground_color,
                           XColor        *background_color,
                           unsigned int   x,
                           unsigned int   y)
{
return None;
}/* end of XCreatePixmapCursor */

extern Pixmap XCreatePixmap(Display     *display,
                            Drawable     d,
                            unsigned int width,
                            unsigned int height,
                            unsigned int depth)
{
NT_window  *canvas;

xtrace("XCreatePixmap\n");

canvas = NT_new_window();
canvas->frame = NULL;
canvas->x = 0;
canvas->y = 0;
canvas->wdth = width;
canvas->hght = height;
canvas->bg=d->bg;
canvas->parent=d;  /* ????? */
canvas->client=NULL;
canvas->title_text = NULL;
canvas->props = NULL;

canvas->w = CreateCompatibleBitmap(d->hDC,         /* device context */
                                   canvas->wdth,   /* bitmap width, in pixels */
                                   canvas->hght);  /* bitmap height, in pixels */

if (!canvas->w)
   {
      NT_delete_window(canvas);
      canvas = NULL;
   }

return(canvas);

}/* end of XCreatePixmap */



XFreePixmap(Display       *display,
            Pixmap         pixmap)
{

if (pixmap->w != INVALID_HANDLE)
   {
      DeleteObject(pixmap->w);
      pixmap->w = INVALID_HANDLE;
   }
return(NT_delete_window(pixmap));

}/* end of XFreePixmap */

XCopyArea(Display       *display,
          Drawable       src,
          Drawable       dest,
          GC             gc,
          int            src_x,
          int            src_y,
          unsigned int   width,
          unsigned int   height,
          int            dest_x,
          int            dest_y)
{
return BitBlt(dest->hDC, // handle to destination device context 
            dest_x,    // x-coordinate of destination rectangle’s upper-left corner 
            dest_y,    // y-coordinate of destination rectangle’s upper-left corner 
            width,     // width of destination rectangle 
            height,    // height of destination rectangle 
            src->hDC,  // handle to source device context 
            src_x,     // x-coordinate of source rectangle’s upper-left corner 
            src_y,     // y-coordinate of source rectangle’s upper-left corner 
            SRCCOPY); // raster operation code 

 
}/* end of XCopyArea */


XIOErrorHandler XSetIOErrorHandler(XIOErrorHandler handler)
{
return NULL;
}/* end of XSetIOErrorHandler */

int (*XSynchronize(Display *dpy, int onoff))()
{
return 0;
}/* end of XSynchronize */

XDisableAccessControl(Display       *display)
{
return(0);
}/* end of XDisableAccessControl */

XWMHints *XGetWMHints(Display       *display,
                      Window         w)
{
}/* end of XGetWMHints */

XConfigureWindow(Display        *display,
                 Window          w,
                 unsigned int    value_mask,
                 XWindowChanges *values)
{
/*BOOL SetWindowPos( 
HWND hWnd, // handle of window 
HWND hWndInsertAfter, // placement-order handle 
int X, // horizontal position 
int Y, // vertical position 
int cx, // width 
int cy, // height 
UINT uFlags  // window-positioning flags */

return 0;
 
}/* end of XConfigureWindow */

char *XKeysymToString(KeySym  keysym)
{
}/* end of XKeysymToString */

KeyCode XKeysymToKeycode(Display       *display,
                         KeySym         keysym)
{
}/* end of XKeysymToKeycode */

char *XResourceManagerString(Display       *display)
{
return NULL;
}/* end of XResourceManagerString */

XFreeFontNames(char    **list)
{
}/* end of XFreeFontNames */

Status XStringListToTextProperty(char          **list,
                                 int             count,
                                 XTextProperty  *Text_prop_return)
{
}/* end of XStringListToTextProperty */

int XReadBitmapFile(Display       *display,
                    Drawable       d,
                    const char    *filename,
                    unsigned int  *width_return,
                    unsigned int  *height_return,
                    Pixmap        *bitmap_return,
                    int           *x_hot_return,
                    int           *y_hot_return)
{
}/* end of XReadBitmapFile */

void XSetWMClientMachine(Display       *display,
                         Window         w,
                         XTextProperty *text_prop)
{
return 0;
}/* end of XSetWMClientMachine */

XSetCommand(Display       *display,
            Window         w,
            char         **argv,
            int            argc)
{
return 0;
}/* end of XSetCommand */

XIfEvent(Display       *display,
         XEvent        *event_return,
         Bool (*predicate) ( Display*            /* display */,
                    XEvent*         /* event */,
                    char*           /* arg */
                  )     ,
        char           *arg)
{
}/* end of XIfEvent */

Bool XCheckIfEvent(Display       *display,
                   XEvent        *event_return,
                   Bool (*predicate) ( Display*            /* display */,
                              XEvent*         /* event */,
                              char*           /* arg */
                            )     ,
                   char          *arg)
{
}/* end of XCheckIfEvent */

Status XIconifyWindow(Display       *display,
                      Window         w,
                      int            screen_number)
{
return CloseWindow(w->w);
}/* end of XIconifyWindow */





#endif /* ARPUS_CE */

#endif
#else /* not WIN32_XNT */
/* still might want the winmain */
#ifdef WINDOWSAPP
/*****************************************************************\

	Function: WinMain
	Inputs:   instance, previous instance, command line arguments,
		  default start up.

	Comments: Called instead of main() as the execution entry point.

\*****************************************************************/

int main(int argc, char *argv[]);


#include <windows.h>
#include <sys/types.h>

#define MAX_COMMAND_ARGS 20
int  WINAPI 
WinMain(HINSTANCE hInst,HINSTANCE hPrevInst,LPSTR lpCmdLine,int nCmdShow)
{
        static char *command_args[MAX_COMMAND_ARGS];
        static int num_command_args;
        char *wordPtr,*tempPtr,*cmdname;
        int i,quote;
/*	hInstance=hInst;
	hPrevInstance=hPrevInst;*/
        for (i=0;i<MAX_COMMAND_ARGS;i++)
          command_args[i] = NULL;


        cmdname = GetCommandLine();
#ifdef blah
MessageBox(NULL, /*hInst, /* handle of owner window */
           (cmdname ? cmdname : "cmdname is NULL"), /* address of text in message box */
           "Args to Ce", /* address of title of message box  */
           MB_OK | MB_TASKMODAL /* style of message box */ ); 
#endif
        cmdname = strdup(cmdname); /* get local copy */
        /* Observed format has the command name in quotes followed by args */
        /* eg:  "U:\Ce\Debug\ce.exe" zjunk.test  */
        if (*cmdname == '"')
           {
              tempPtr = strchr(++cmdname, '"');
              if (tempPtr)
                 *tempPtr = '\0';
           }
        else
           {
              i = strlen(cmdname) - strlen(lpCmdLine);
              cmdname[i] = '\0';
              i = strlen(cmdname);
              while((i > 0) && ((cmdname[i-1] == ' ') || (cmdname[i-1] == '\t')))
                 cmdname[--i] = '\0'; /* trim trailing blanks */
           }
        wordPtr = lpCmdLine;
        quote = 0;
        num_command_args = 1;
        command_args[0] = cmdname;
        while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
           wordPtr++;
        if (*wordPtr == '\"')
        {
          quote = 1;
          wordPtr++;
        }
/*        if (!*wordPtr)
          main(0,NULL);
        else
        {*/
          while (*wordPtr && num_command_args < MAX_COMMAND_ARGS)
          {
            tempPtr = wordPtr;
            if (quote)
            {
              while (*tempPtr && *tempPtr != '\"')
                tempPtr++;
              quote = 0;
            }
            else
              while (*tempPtr && *tempPtr != ' ')
                tempPtr++;
            if (*tempPtr)
              *(tempPtr++) = '\0';
            command_args[num_command_args++] = wordPtr;
            wordPtr = tempPtr;
            while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
              wordPtr++;
            if (*wordPtr == '\"')
            {
              quote = 1;
              wordPtr++;
            }
          }
          main(num_command_args,command_args);
     /* }*/

return(0);
}
#endif

#endif
