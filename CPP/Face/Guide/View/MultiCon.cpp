/*******************************************************************************
 FILE         :   MultiCon.cpp

 COPYRIGHT    :   DMAlex, 2001-2012

 DESCRIPTION  :   консольный ввод/вывод со многими буферами вывода

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   11/25/1997

 LAST UPDATE  :   01/18/2012
*******************************************************************************/



#include "stdafx.h"

#include <algorithm>
#include <strstream>

#include "multicon.h"


using namespace std;
using namespace Its;
using namespace DCommon;


#undef OPTIMIZE_CURSOR // что бы хуже был виден курсор :-)


const char cursorSize  = 20;  // размер курсора
const char timeDelay   = 10;  // время на которое тормозятся "неактивные нити" (что бы лучше был виден курсор)
const char fillChar    = ' '; // заполняющий символ

bool MultiCon::fLoaded = false;


// вынесены из MultiCon так как такая многонитевая переменная не может быть
// инициализирована в run-time ;-(
__declspec(thread) MultiCon::sizeList currentPage  = 0;
__declspec(thread) long               cursorPos    = 0;
__declspec(thread) WORD               currentColor = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;


WORD color::colorTable[2][16] =
{
   {
   0,
   FOREGROUND_BLUE,
   FOREGROUND_GREEN,
   FOREGROUND_RED,
   FOREGROUND_BLUE  | FOREGROUND_GREEN,
   FOREGROUND_BLUE  | FOREGROUND_RED,
   FOREGROUND_GREEN | FOREGROUND_RED,
   FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED,
   FOREGROUND_INTENSITY,
   FOREGROUND_BLUE  | FOREGROUND_INTENSITY,
   FOREGROUND_GREEN | FOREGROUND_INTENSITY,
   FOREGROUND_RED   | FOREGROUND_INTENSITY,
   FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
   FOREGROUND_BLUE  | FOREGROUND_RED   | FOREGROUND_INTENSITY,
   FOREGROUND_GREEN | FOREGROUND_RED   | FOREGROUND_INTENSITY,
   FOREGROUND_BLUE  | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
   },

   {
   0,
   BACKGROUND_BLUE,
   BACKGROUND_GREEN,
   BACKGROUND_RED,
   BACKGROUND_BLUE  | BACKGROUND_GREEN,
   BACKGROUND_BLUE  | BACKGROUND_RED,
   BACKGROUND_GREEN | BACKGROUND_RED,
   BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED,
   BACKGROUND_INTENSITY,
   BACKGROUND_BLUE  | BACKGROUND_INTENSITY,
   BACKGROUND_GREEN | BACKGROUND_INTENSITY,
   BACKGROUND_RED   | BACKGROUND_INTENSITY,
   BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_INTENSITY,
   BACKGROUND_BLUE  | BACKGROUND_RED   | BACKGROUND_INTENSITY,
   BACKGROUND_GREEN | BACKGROUND_RED   | BACKGROUND_INTENSITY,
   BACKGROUND_BLUE  | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY
   }
};



void MultiCon::PageManager::run()
{
   INPUT_RECORD iRec;
   DWORD lpcRead;

   mconsole.lastKey       = 0;
   mconsole.lastKeyCode   = 0; // key codes begin from 0x01 (VK_LBUTTON)
   mconsole.exitRequested = false;

   try
   {
     while(!shouldExit())
        {
        if(!ReadConsoleInput(mconsole.hInput, &iRec, 1, &lpcRead))
          throw XMError("can't read input buffer");

        if(iRec.EventType != KEY_EVENT)
          continue;

        // KEY_EVENT_RECORD keyRec = iRec.Event.KeyEvent;
        // тут же должна ссылка быть? Как же оно раньше работало? 8-Q
        KEY_EVENT_RECORD& keyRec = iRec.Event.KeyEvent;

        if(keyRec.wVirtualKeyCode   == keyPrevBuf.first &&
           keyRec.dwControlKeyState == keyPrevBuf.second)
          {
          if(keyRec.bKeyDown == FALSE)
              mconsole.switchPage(MultiCon::prev);
          continue;
          }

        if(keyRec.wVirtualKeyCode   == keyNextBuf.first &&
           keyRec.dwControlKeyState == keyNextBuf.second)
          {
          if(keyRec.bKeyDown == FALSE)
              mconsole.switchPage(MultiCon::next);
          continue;
          }

        if(keyRec.bKeyDown == FALSE)
          continue;

        // может таки изменять через InterlockedExchange()?
        mconsole.lastKey     = keyRec.uChar.AsciiChar;
        mconsole.lastKeyCode = keyRec.wVirtualKeyCode;

        mconsole.pageList[ mconsole.activePage ].keyPressed.pulse();
        }
   }
   catch( ... )
   {
   }
   
   mconsole.exitRequested = true;
   mconsole.pageList[ mconsole.activePage ].keyPressed.pulse();
}



//   Некоторые замечания:
//
//  1) imho, лучше сделать выделение новых экранных буферов по запросу каждой нити (a la регистрация)
//     тогда каждая нить будет иметь по крайней мере гарантированное окно вывода;
//
//  2) если все положить в DLL и написать подходящую ф-цию DllEntryPoint, то можно будет
//     создавать не только одну переменную типа MultiCon;
//
//  3) при изменении размера буфера пользователем (из Menu/Properties окна) надо сделать
//     пересчет всего что пересчитывается :-)
//
MultiCon::PageDescr::PageDescr():
   inputLine(1,1),
   keyPressed()
{
   CONSOLE_CURSOR_INFO cursInfo;
   cursInfo.dwSize   = cursorSize;
   cursInfo.bVisible = FALSE;

   if((pageHandle = CreateConsoleScreenBuffer(
                       GENERIC_READ | GENERIC_WRITE,       // access flag
                       FILE_SHARE_READ | FILE_SHARE_WRITE, // buffer share mode
                       NULL,                               // address of security attributes
                       CONSOLE_TEXTMODE_BUFFER,            // type of buffer to create
                       NULL                                // reserved
                       )) == INVALID_HANDLE_VALUE)
                       throw XMError("can't create screen buffer");

   if(!SetConsoleCursorInfo(pageHandle, &cursInfo))
      throw XMError("can't set console cursor info");
}



MultiCon::MultiCon(sizeList  numBuffers, const char* title, bool _fAutoScroll,
                   KeyCode_t keyNextBuf, KeyCode_t keyPrevBuf):
   pageManager(*this, keyNextBuf, keyPrevBuf),
   pageList(numBuffers),
   hInput(GetStdHandle(STD_INPUT_HANDLE)),
   activePage(0),
   fAutoScroll(_fAutoScroll)
{
   CONSOLE_SCREEN_BUFFER_INFO screenInfo;

   if(MultiCon::fLoaded)
      throw XMError("can't create second instance MultiCon");

   MultiCon::fLoaded = true;

   if(numBuffers < 1)
      throw XMError("out of range pages");

   if(!GetConsoleScreenBufferInfo(pageList[activePage].pageHandle, &screenInfo))
      throw XMError("can't get console screen buffer info");

   if(title && !SetConsoleTitle(title))
      throw XMError("can't set console title");

   sizeScreenBuf.X = screenInfo.dwSize.X;
   sizeScreenBuf.Y = screenInfo.dwSize.Y;

   switchPage(MultiCon::init);
   pageManager.start();
}


MultiCon::~MultiCon()
{
   pageManager.stop();
}


MultiCon& MultiCon::operator >> (consolenumber& c)
{
  DSemaphore::Lock       sem( pageList[ currentPage ].inputLine );
  MultiCon::CursorSaver  curs( *this, cursorPos );

  ULONG value  = c.Value();

  WORD control = enterLimitedNumber( value, c.fieldSize, c.lowerBound, c.upperBound, c.fieldColor );

  c.value = value;
  c.focus = control == VK_UP ? FieldPrevious : FieldNext;

  return *this;
}



MultiCon& MultiCon::operator >> (multichoice& selector)
{
  DSemaphore::Lock       sem( pageList[ currentPage ].inputLine );
  MultiCon::CursorSaver  curs( *this, cursorPos );

  vector<string>&  items = selector.items;
  string  selectedString = "";
  long    savedCursorPos = cursorPos;
  color   savedColor     = currentColor; // we have only one input line per thread and so its color
  int     maxLength      = 0;

  setColor( selector.fieldColor );

  while( items.size() > 0 )
  {
    if( selector.selectedItem < 0 )
    {
      selector.selectedItem = items.size() - 1;
    }
    else if( selector.selectedItem > items.size() - 1 )
    {
      selector.selectedItem = 0;
    }

    cursorPos       = savedCursorPos;
    selectedString  = items[ selector.selectedItem ];
    maxLength       = max<int>( maxLength, selectedString.length());
    
    // To overwrite small other items
    selectedString.resize( maxLength, fillChar );

    switch( enterMultiChoice( selectedString ))
    {
      case VK_LEFT    : selector.selectedItem--;        continue;
      case VK_RIGHT   : selector.selectedItem++;        continue;
      case VK_UP      : selector.focus = FieldPrevious; break;
      case VK_DOWN    : selector.focus = FieldNext;     break;
      case VK_RETURN  : selector.focus = FieldNext;     break;
      default: break;
    }

    break;
  }

  cursorPos = savedCursorPos;
  setColor( savedColor );

  *this << selectedString;

  return *this;
}


MultiCon& MultiCon::operator >> (string& str)
{
   DSemaphore::Lock       sem( pageList[ currentPage ].inputLine );
   MultiCon::CursorSaver  curs( *this, cursorPos );

   enterString( str );

   return *this;
}



MultiCon& MultiCon::operator >> ( char& key )
{
   DSemaphore::Lock      sem( pageList[ currentPage ].inputLine );
   MultiCon::CursorSaver curs( *this, cursorPos );
   WORD  keyCode;
   long  savedCursorPos = cursorPos;

   *this << key;

   // стоит ли и здесь свигать курсор как при вводе string?
   while( inkey( key, keyCode ) && !validKey( key ));

   cursorPos = savedCursorPos;
   printString( &key, 1, true );

   return *this;
}


WORD MultiCon::enterLimitedNumber( ULONG& c, int fieldSize, long lowerBound, long upperBound, color fieldColor )
{
  long  savedCursorPos  = cursorPos;
  color savedColor      = currentColor; // we have only one input line per thread and so its color
  WORD  control         = 0;

  setColor( fieldColor );

  //
  // 27 Jan 2012 - DMA - ToDo: limit the number within the bounds when entered 
  // manually, non with arrows.
  //
  // We could copy c to temp value to avoid side effects
  // as now we change the actual reference but do not return control 
  // to the calling thread
  while( true )
  {
    cursorPos = savedCursorPos;
    control   = enterPositiveNumberString( c, fieldSize, lowerBound, upperBound );

    switch( control )
    {
      case VK_LEFT  : c > lowerBound ? c-- : c; continue;
      case VK_RIGHT : c < upperBound ? c++ : c; continue;
    }

    break;
  }

  cursorPos = savedCursorPos;
  setColor( savedColor );

  convertAndPrintNumber( c, fieldSize );

  return control;
}


MultiCon& MultiCon::operator >> (BYTE& c)
{
  DSemaphore::Lock       sem( pageList[ currentPage ].inputLine );
  MultiCon::CursorSaver  curs( *this, cursorPos );

  ULONG returnValue = c;

  enterLimitedNumber( returnValue, 3, 0, 0xFF, color( currentColor ));

  c = (BYTE)returnValue;
  
  return *this;
}



MultiCon& MultiCon::operator >> (ULONG& c)
{
  DSemaphore::Lock       sem( pageList[ currentPage ].inputLine );
  MultiCon::CursorSaver  curs( *this, cursorPos );

  enterLimitedNumber( c, 7, 0, 0xFFFFFFFF, color( currentColor ) );

  return *this;
}


MultiCon& MultiCon::operator >> (page p)
{
  setPage( p );

  return *this;
}


MultiCon& MultiCon::operator >> (pos p)
{
  setPos( p );

  return *this;
}


MultiCon& MultiCon::operator >> (color c)
{
  setColor( c );

  return *this;
}



// Input text sequence, stop on Enter or any control key 
WORD MultiCon::enterString( string& str )
{
  SHORT  maxStrLen = absoluteSizeScreenBuf() - cursorPos - 1L;
  CHAR   key;
  WORD   keyCode = 0;

  *this << str;

  if( fAutoScroll )
  {
    maxStrLen += sizeScreenBuf.X * cursorRelativeCoord(cursorPos).Y;
  }

  while( inkey( key, keyCode ) && 
      !( keyCode == VK_RETURN || keyCode == VK_LEFT || keyCode == VK_RIGHT ||
         keyCode == VK_UP     || keyCode == VK_DOWN ))
  {
    if( isdigit( key ) || isalpha( key ) || isspace( key ) || ispunct( key ))
    {
      if( str.length() >= maxStrLen ) 
      {
        continue;
      }
      
      str += key;
      printString( &key, 1, true );
      cursorPos++;
      
      if( fAutoScroll && cursorPos > absoluteSizeScreenBuf() - 1L )
      {
        scrollPage( -1 );
        cursorPos -= sizeScreenBuf.X;
      }
      else
      {
        cursorPos = min( cursorPos, absoluteSizeScreenBuf() - 1L );
      }
     }
      
    // пока из всех ф-ций редактирования - только backspace. Зачем перегружать интерфейс? :-)
    if( key == VK_BACK && str.length() > 0 )
    {
      cursorPos--;
      printString( &fillChar, 1, true );
      str.resize( str.length() - 1 );
    }
    
    setCursor( cursorPos );
  }

  return keyCode;
}


string MultiCon::convertAndPrintNumber( ULONG& number, int fieldSize )
{
  char    buffer[ 33 ];
  long    overhang;
  string  str = "";

  _ltoa( number, buffer, 10 );

  str       = buffer;
  overhang  = fieldSize - str.length();
  
  if( overhang < 0 )
  {
    str.resize( fieldSize );
    *this << str;
    return "";
  }
  
  *this << str;
  *this << string( overhang, fillChar );

  cursorPos -= overhang;

  return str;
}


bool MultiCon::numberBoundsCheck( string numberString, char key, long lowerBound, long upperBound )
{
  string str;

  if( key == 0 )
  {
    str = numberString;
    str.resize( str.length() - 1 );
  }
  else
  {
    str = numberString + key;
  }

  long number = atol( str.c_str() );

  return lowerBound <= number && number <= upperBound;
}


// Input number string, positive only 
// Field size could be more than required to show the number
// and we will clear the field
WORD MultiCon::enterPositiveNumberString( ULONG& number, int fieldSize, long lowerBound, long upperBound )
{
  SHORT   maxStrLen = absoluteSizeScreenBuf() - cursorPos - 1L;
  string  str       = convertAndPrintNumber( number, fieldSize );
  WORD    keyCode   = 0;
  CHAR    key;
  
  if( str.empty() )
  {
    return 0;
  }  

  if( fAutoScroll )
  {
    maxStrLen += sizeScreenBuf.X * cursorRelativeCoord( cursorPos ).Y;
  }

  while( inkey( key, keyCode ) && 
      !( keyCode == VK_RETURN || keyCode == VK_LEFT || keyCode == VK_RIGHT ||
         keyCode == VK_UP     || keyCode == VK_DOWN ))
  {
    if( isdigit( key ))
    {
      if( str.length() >= maxStrLen || !numberBoundsCheck( str, key, lowerBound, upperBound )) 
      {
        continue;
      }

      // special case, 0 like empty string      
      if( str.compare( "0" ) == 0 )
      {
        str = key;
        cursorPos--;
      }
      else
      {
        str += key;
      }
      
      printString( &key, 1, true );
      cursorPos++;
      
      if( fAutoScroll && cursorPos > absoluteSizeScreenBuf() - 1L )
      {
        scrollPage( -1 );
        cursorPos -= sizeScreenBuf.X;
      }
      else
      {
        cursorPos = min( cursorPos, absoluteSizeScreenBuf() - 1L );
      }
    }
   
    if( key == VK_BACK && str.length() > 1 && numberBoundsCheck( str, 0, lowerBound, upperBound ))
    {
      cursorPos--;
      printString( &fillChar, 1, true );
      str.resize( str.length() - 1 );
    }
     
    setCursor( cursorPos );
  }

  number = atol( str.c_str() );
  return keyCode;
}



// Show text and return selected control key
WORD MultiCon::enterMultiChoice( string str )
{
  CHAR   key;
  WORD   keyCode = 0;

  *this << str;

  while( inkey( key, keyCode ) && 
      !( keyCode == VK_RETURN || keyCode == VK_LEFT || keyCode == VK_RIGHT ||
         keyCode == VK_UP     || keyCode == VK_DOWN ))
  {
  }

  return keyCode;
}

void MultiCon::printString(const char *str, DWORD chToWrite, bool fRestoreCurPos)
{
   // может надо ввести скроллирование при выходе за пределы буфера?
   DWORD chWritten;

   if(!FillConsoleOutputAttribute(pageList[currentPage].pageHandle, currentColor, chToWrite, cursorRelativeCoord(cursorPos), &chWritten))
      throw XMError("can't fill console output attribute");

   if(!WriteConsoleOutputCharacter(pageList[currentPage].pageHandle, str, chToWrite, cursorRelativeCoord(cursorPos), &chWritten))
      throw XMError("can't write console output character");

   if(!fRestoreCurPos)
      {
      cursorPos += chWritten;
      cursorPos  = min(cursorPos, absoluteSizeScreenBuf() - 1L);

      // что бы лучше был виден курсор (хотя если вы его сумеете рассмотреть и так при
      // выводе нескольких нитей на одну страницу, значит "вам не нужны очки" (c) Spaceballs :-)
      #ifdef OPTIMIZE_CURSOR
      if(activePage == currentPage)
         {
         DSemaphore::Lock sem(pageList[activePage].inputLine, true);
         if(!sem.wasNonSignaled())
            Sleep(timeDelay);
         }
      #endif
      }
}


MultiCon& MultiCon::operator << (const string& s)
{
   printString(s.c_str(), s.length());
   return *this;
}


MultiCon& MultiCon::operator << (char* c)
{
   return *this << string(c);
}


MultiCon& MultiCon::operator << (char c)
{
   printString(&c, 1);
   return *this;
}


MultiCon& MultiCon::operator << (BYTE c)
{
   return *this << (short)c;
}



MultiCon& MultiCon::operator << (short c)
{
  char buf[33]; 

  _itoa(c, buf, 10 );
  return *this << buf;
}


MultiCon& MultiCon::operator << (ULONG c)
{
  char buf[33]; 

  _ltoa(c, buf, 10 );
  return *this << buf;
}


MultiCon& MultiCon::operator << (page p)
{
  setPage( p );

  return *this;
}


MultiCon& MultiCon::operator << (pos p)
{
  setPos( p );

  return *this;
}



MultiCon& MultiCon::operator << (color c)
{
  setColor( c );

  return *this;
}


MultiCon& MultiCon::operator << (endln)
{
   COORD c = cursorRelativeCoord(cursorPos);

   c.X  = 0;
   c.Y += 1;

   if(fAutoScroll && c.Y > sizeScreenBuf.Y - 1)
      scrollPage(-1);

   c.Y = min(c.Y, short(sizeScreenBuf.Y - 1));
   cursorPos = cursorAbsoluteCoord(c);
   return *this;
}


MultiCon& MultiCon::operator << (cls)
{
   DWORD chWritten;
   COORD zeroCoord;

   zeroCoord.X = 0;
   zeroCoord.Y = 0;

   // а правильно ли брать absoluteSizeScreenBuf() символов или надо больше? '-)
   if(!FillConsoleOutputCharacter(pageList[currentPage].pageHandle, fillChar, absoluteSizeScreenBuf(), zeroCoord, &chWritten))
      throw XMError("can't write console output character");

   return *this;
}


void MultiCon::setCursor(long& c)
{
   if(!SetConsoleCursorPosition(pageList[currentPage].pageHandle, cursorRelativeCoord(c)))
      throw XMError("can't set console position");
}


long MultiCon::getCursor()
{
   CONSOLE_SCREEN_BUFFER_INFO sInfo;
   if(!GetConsoleScreenBufferInfo(pageList[currentPage].pageHandle, &sInfo))
      throw XMError("can't get console screen buffer info");

   return cursorAbsoluteCoord(sInfo.dwCursorPosition);
}


void MultiCon::setPage( page p )
{
  currentPage = p.the_page;

  if( currentPage >= pageList.size() )
    throw XMError("out of range pages");
}


void MultiCon::setPos( pos p )
{
  // стоит ли поставить проверку на выход за пределы буфера?
  // а то ведь преобразует их легко в то что надо и не надо :-)
  if( p.relCoord )
    cursorPos = cursorAbsoluteCoord( p.rel_pos );
  else
    cursorPos = p.abs_pos;
}


void MultiCon::setColor( color c )
{
  currentColor = c.the_color;
}


void MultiCon::cursorShow(bool visible)
{
   CONSOLE_CURSOR_INFO cursInfo;
   cursInfo.dwSize   = cursorSize;
   cursInfo.bVisible = visible ? TRUE : FALSE;

   if(visible == TRUE)
      {
      // если при переключении страниц на ставшей активной странице
      // нет строк ввода, то нету и курсора!
      DSemaphore::Lock sem(pageList[activePage].inputLine, true);
      if(sem.wasNonSignaled())
         return;
      }

   if(!SetConsoleCursorInfo(pageList[activePage].pageHandle, &cursInfo))
      throw XMError("can't set console cursor info");
}


void MultiCon::scrollPage(SHORT shift)
{
   SMALL_RECT srctScrollRect, srctClipRect;
   CHAR_INFO  chiFill;
   COORD      coordDest;

   srctScrollRect.Top     = 0;
   srctScrollRect.Left    = 0;
   srctScrollRect.Bottom  = sizeScreenBuf.Y;
   srctScrollRect.Right   = sizeScreenBuf.X;

   chiFill.Attributes     = currentColor;
   chiFill.Char.AsciiChar = fillChar;

   // а точно _это_ надо менять? '-)
   coordDest.X  = 0;
   coordDest.Y  = shift;
   srctClipRect = srctScrollRect;

   if(!ScrollConsoleScreenBuffer(
          pageList[currentPage].pageHandle,  /* screen buffer handle     */
          &srctScrollRect,                   /* scrolling rectangle      */
          &srctClipRect,                     /* clipping rectangle       */
          coordDest,                         /* top left destination cell*/
          &chiFill))                         /* fill character and color */
      throw XMError("can't Scroll console screen buffer");
}


void MultiCon::switchPage(switchPageType where)
{
   cursorShow(false);

   switch(where)
      {
      case MultiCon::init:
           break;
      case MultiCon::prev:
           InterlockedExchange((long*)&activePage, activePage == 0 ? pageList.size() - 1 : activePage - 1);
           break;
      case MultiCon::next:
           InterlockedExchange((long*)&activePage, activePage < pageList.size() - 1 ? activePage + 1 : 0);
           break;
      default:
           throw XMError("invalid calls switchPage()");
      }

   if(!SetConsoleActiveScreenBuffer(pageList[activePage].pageHandle))
      throw XMError("can't set active screen buffer");

   cursorShow(true);
}


bool MultiCon::validKey( CHAR& key )
{
   // imho, тут надо чего нито еще добавить, а то "маловато будет" :-)
   if( isprint( key ) || key == VK_RETURN) return true;

   return false;
}


// нужнен ли теперь вообще код возврата? 
bool MultiCon::inkey( CHAR& key, WORD& keyCode )
{
   DEvent::Lock sem( pageList[ currentPage ].keyPressed );

   key     = lastKey;
   keyCode = lastKeyCode;
   
   return !exitRequested;
}





