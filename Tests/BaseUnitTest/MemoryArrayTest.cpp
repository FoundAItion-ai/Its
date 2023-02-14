/*******************************************************************************
 FILE         :   MemoryArrayTest.cpp

 COPYRIGHT    :   DMAlex, 2016

 DESCRIPTION  :   MaiTrix - MemoryArray class unit tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   01/11/2016

 LAST UPDATE  :   01/11/2016
*******************************************************************************/


#include "stdafx.h"

#include "BaseUnitTest.h"
#include "CubeUtils.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;


namespace BaseUnitTest
{		
  TEST_CLASS(MemoryArrayUnitTest), BaseTest
  {
  public:    
    TEST_METHOD(MemoryArrayBasics)
    {
      WriteMessage( "MemoryArray basics unit tests" );
      MemoryArray::Array memory = { 0 };
      MemoryArray memoryArray( memory );
      Link linkZero( 0 );
      Link link, anotherLink;

      // Initial content
      // Death tests unsupported.
      // memoryArray.Get(NULL);
      Assert::IsTrue( memoryArray.IsEmpty() );
      Assert::IsTrue( link == linkZero );
      link = memoryArray.Get();

      // memoryArray.Set( Link( 0 )) must crash but there's no death tests in the framework.
      for( int loopLink = 1; loopLink <= 0xFF; loopLink++ )  
      {
        // Get/Set
        Link multiLink( loopLink );
        memoryArray.Set( multiLink );
        link = memoryArray.Get();
        Assert::IsTrue( link == multiLink );
        Assert::IsFalse( memoryArray.IsEmpty() );

        // Double get/set
        memoryArray.Set( Link( 1 ));
        memoryArray.Set( Link( 7 ));
        memoryArray.Set( multiLink );
        link = memoryArray.Get();
        anotherLink = memoryArray.Get();
        Assert::IsFalse( memoryArray.IsEmpty() );
        Assert::IsTrue( link == multiLink );
        Assert::IsTrue( anotherLink == multiLink );

        // Clearing
        memoryArray.Clear();
        link = memoryArray.Get();
        Assert::IsTrue( link == linkZero );
        Assert::IsTrue( memoryArray.IsEmpty() );
      }
   }
  };
}