/*******************************************************************************
 FILE         :   MultiCon.h

 COPYRIGHT    :   DMAlex, 2001-2012

 DESCRIPTION  :   консольный ввод/вывод со многими буферами вывода

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   11/25/1997

 LAST UPDATE  :   01/18/2012
*******************************************************************************/

#ifndef __MULTICON_H
#define __MULTICON_H

#include <windows.h>
#include <vector>
#include <exception>
#include <cstring>

#include "DCommonIncl.h"

using namespace std;
using namespace DCommon;


namespace Its
{

class  consolenumber;
class  multichoice;
class  page;
class  pos;
class  color;
struct endln;
struct cls;


// исключени€ дл€ MultiCon
class XMError: public exception
{
public:
   XMError(const char* msg): exception(msg) {}
};


// класс мультиконсольного многонитевого ввода/вывода
class MultiCon
{
public:
   // описатель страницы
   struct PageDescr
      {
      PageDescr();
      PageDescr& operator = (const PageDescr&) { return *this; }

      HANDLE     pageHandle; // указатель страницы буфера
      DSemaphore inputLine;  // счетчик свободных строк ввода на странице (не больше 1)
      DEvent     keyPressed; // отпираетс€ PageManager-ом при нажатии клавиши
      };

   typedef std::pair<WORD, DWORD>            KeyCode_t; // код клавиши, состо€ние управл€ющих клавиш
   typedef std::vector<PageDescr>            descList;
   typedef std::vector<PageDescr>::size_type sizeList;
   typedef std::vector<PageDescr>::iterator  iterList;
   typedef std::string                       string;

   // ѕараметры: число экранных буферов, заголовок окна, флаг наличи€ автопрокрутки,
   //            клавиши переключени€ между буферами вперед и назад
   MultiCon(sizeList numBuffers, const char* title = 0, bool _fAutoScroll = true,
            KeyCode_t keyNextBuf = KeyCode_t(VK_TAB, 0),
            KeyCode_t keyPrevBuf = KeyCode_t(VK_TAB, SHIFT_PRESSED));

   ~MultiCon();

   MultiCon& operator >> (consolenumber& c);
   MultiCon& operator >> (multichoice& c);
   MultiCon& operator >> (string&  s);
   MultiCon& operator >> (char&    c);
   MultiCon& operator >> (BYTE&    c);
   MultiCon& operator >> (ULONG&   c);
   MultiCon& operator >> (page     p);
   MultiCon& operator >> (pos      p);
   MultiCon& operator >> (color    c);

   MultiCon& operator << (const string& s);
   MultiCon& operator << (char*    c);
   MultiCon& operator << (char     c);
   MultiCon& operator << (short    c);
   MultiCon& operator << (BYTE     c);
   MultiCon& operator << (ULONG    c);
   MultiCon& operator << (page     p);
   MultiCon& operator << (pos      p);
   MultiCon& operator << (color    c);
   MultiCon& operator << (endln    c);
   MultiCon& operator << (cls      c);

   int Pages()  { return pageList.size(); }
   int Width()  { return sizeScreenBuf.X; }
   int Height() { return sizeScreenBuf.Y; }
   
protected:
   // обслуживает консольную очередь ввода и занимаетс€ переключением страниц
   class PageManager: public DThread
      {
      public:
         PageManager(MultiCon& _mconsole, KeyCode_t _keyNextBuf, KeyCode_t _keyPrevBuf):
            DThread(),
            mconsole(_mconsole),
            keyNextBuf(_keyNextBuf),
            keyPrevBuf(_keyPrevBuf)
            {}

      protected:
         virtual void run();

      private:
         MultiCon&  mconsole;   // базова€ консоль
         KeyCode_t  keyNextBuf; // клавиша переключени€ на следующий буфер
         KeyCode_t  keyPrevBuf; // клавиша переключени€ на предыдующий буфер
      };


   // курсорные "скобки" (дл€ его восстановлени€)
   // 
   // 30 Jan 2012 - DMA - save color too?
   //
   class CursorSaver
      {
      public:
         CursorSaver(MultiCon& _mconsole, long& cursorPos): mconsole(_mconsole)
            { mconsole.setCursor(cursorPos); mconsole.cursorShow(true); }

         ~CursorSaver()
            { mconsole.cursorShow(false); }

      private:
         MultiCon& mconsole;
      };

   friend PageManager;
   friend CursorSaver;

   enum switchPageType { init, prev, next };
   void switchPage(switchPageType where);

   inline void printString(const char *str, DWORD chToWrite, bool fRestoreCurPos = false);
   inline void cursorShow(bool visible = true);
   inline void scrollPage(SHORT shift);
   inline void setCursor(long& c);
   inline long getCursor();

   inline void setPage  ( page p );
   inline void setPos   ( pos  p );
   inline void setColor ( color c );  

   long cursorAbsoluteCoord(COORD c)
            {
            return sizeScreenBuf.X * c.Y + c.X;
            }

   COORD cursorRelativeCoord(long c)
            {
            COORD ret;
            ret.X = (SHORT)(c % sizeScreenBuf.X);
            ret.Y = (SHORT)(c / sizeScreenBuf.X);
            return ret;
            }

   long absoluteSizeScreenBuf()
            {
            return sizeScreenBuf.X * sizeScreenBuf.Y;
            }

   bool inkey( CHAR& key, WORD& keyCode );
   bool validKey( CHAR& key );
   bool numberBoundsCheck( string numberString, char key, long lowerBound, long upperBound );

   string convertAndPrintNumber( ULONG& number, int fieldSize );

   // Input text sequence, stop on Enter or any control key 
   WORD enterString( string& str );

   // Input number string, positive only 
   // Field size could be more than required to show the number
   // and we will clear the field
   WORD enterPositiveNumberString( ULONG& number, int fieldSize, long lowerBound, long upperBound );

   WORD enterLimitedNumber( ULONG& c, int fieldSize, long lowerBound, long upperBound, color fieldColor );

   // Show text and return selected control key
   WORD enterMultiChoice( string str );

private:
   PageManager pageManager;   // нить, управл€юща€ переключением страниц
   descList    pageList;      // список описателей страниц
   HANDLE      hInput;        // стандартный ввод
   CHAR        lastKey;       // последн€€ нажата€ клавиша (ASCII, no control symbols)
   WORD        lastKeyCode;   // A virtual-key code that identifies the given key in a device-independent manner. 
   bool        exitRequested; // External thread is trying to stop the console
   COORD       sizeScreenBuf; // размер экранного окна
   sizeList    activePage;    // номер активной (отображаемой) страницы
   bool        fAutoScroll;   // автоскроллирование страницы включено?  
   static bool fLoaded;       // флаг, предотвращающий повторное создание обьекта
};


enum FieldFocus { FieldPrevious, FieldNext };


// манипул€торы
class page
{
public:
   page(MultiCon::sizeList _page): the_page(_page) {}

   friend MultiCon;

private:
   MultiCon::sizeList the_page;
};


class pos
{
public:
   pos(): relCoord(false), abs_pos(0) {}
   pos(SHORT x, SHORT y): relCoord(true) { rel_pos.X = x; rel_pos.Y = y; }
   pos(COORD c): relCoord(true),  rel_pos(c) {}
   pos(long c): relCoord(false), abs_pos(c) {}
   pos(pos& p): relCoord(p.relCoord), abs_pos(p.abs_pos) { rel_pos.X = p.rel_pos.X; rel_pos.Y = p.rel_pos.Y; }

   pos operator + ( const pos & that ) { return pos( this->rel_pos.X + that.rel_pos.X, this->rel_pos.Y + that.rel_pos.Y ); }
   pos operator - ( const pos & that ) { return pos( this->rel_pos.X - that.rel_pos.X, this->rel_pos.Y - that.rel_pos.Y ); }

   static SHORT RelativeViewWidth( pos topLeft, pos bottomRight ) 
   {
     return bottomRight.rel_pos.X - topLeft.rel_pos.X;
   }

   void Bound( pos topLeft, pos bottomRight ) 
   { 
     if( relCoord )
     {
       if( rel_pos.X < topLeft.rel_pos.X )
       {
         rel_pos.X = topLeft.rel_pos.X;
       }

       if( rel_pos.Y < topLeft.rel_pos.Y )
       {
         rel_pos.Y = topLeft.rel_pos.Y;
       }

       if( rel_pos.X > bottomRight.rel_pos.X )
       {
         rel_pos.X = bottomRight.rel_pos.X;
       }

       if( rel_pos.Y > bottomRight.rel_pos.Y )
       {
         rel_pos.Y = bottomRight.rel_pos.Y;
       }
     }
   }

   friend MultiCon;

private:
   long  abs_pos;
   COORD rel_pos;
   bool  relCoord;
};


class color
{
public:
   enum colorType { black, blue, green, red, cyan, magenta, brown, lightgray,
                    darkgray, lightblue, lightgreen, lightred, lightcyan,
                    lightmagenta, yellow, white };
   color(WORD c):
      the_color(c) {}

   color(colorType foreground, colorType background):
      the_color(colorTable[0][(int)foreground] | colorTable[1][(int)background]) {}

   color(colorType foreground):
      the_color(colorTable[0][(int)foreground] | colorTable[1][(int)black]) {}

   friend MultiCon;

private:
   WORD the_color;
   static WORD colorTable[2][16];
};


class multichoice
{
public:
  multichoice( color _fieldColor, int itemsCount, ... ):
    fieldColor( _fieldColor )
  {
    va_list args;
    va_start( args, itemsCount );

    for( int i = 0; i < itemsCount; i++ )
    {
      const char* item = va_arg( args, const char* );
      items.push_back( item );
    }

    va_end( args );

    selectedItem  = 0;
    focus         = FieldNext;
  }

  friend MultiCon;

  const char*  Choice() { return items[ selectedItem ].c_str(); }
  FieldFocus   Focus()  { return focus; }

private:
  vector<string>  items;
  int             selectedItem;
  FieldFocus      focus;
  color           fieldColor; // hightlighted (focused) color
};



class consolenumber
{
public:
  consolenumber( ULONG initialValue, int _fieldSize, long _lowerBound, long _upperBound, color _fieldColor ):
    value       ( initialValue ),
    fieldSize   ( _fieldSize   ),
    lowerBound  ( _lowerBound  ),
    upperBound  ( _upperBound  ),
    fieldColor  ( _fieldColor  )
  {
  }

  ULONG       Value() { return value; }
  FieldFocus  Focus() { return focus; }

  friend MultiCon;

private:
  ULONG       value;
  FieldFocus  focus;
  int         fieldSize;
  long        lowerBound;
  long        upperBound;
  color       fieldColor; // hightlighted (focused) color
};



struct cls
{
   cls() {}
};


// а не заменить ли его на функцию?
struct endln
{
   endln() {}
};


}; // namespace Its


#endif // __MULTICON_H
