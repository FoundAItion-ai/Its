/*******************************************************************************
 FILE         :   LayerSavingTest.cpp

 COPYRIGHT    :   DMAlex, 2013

 DESCRIPTION  :   MaiTrix - layer saving tests

 PROGRAMMED BY:   Alex Fedosov

 CREATION DATE:   08/07/2013

 LAST UPDATE  :   04/17/2014
*******************************************************************************/


#include "stdafx.h"

#include "BaseUnitTest.h"
#include "ISaver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Its;



namespace BaseUnitTest
{		
  bool operator != ( Celllayer& one, Celllayer& two )
  {
    return !( one == two );
  }

  bool operator == ( Celllayer& one, Celllayer& two )
  {
    if( one.type != two.type ||
        one.maiTrixName != two.maiTrixName ||
        one.size != two.size ||
        one.positionZ != two.positionZ ||
        one.cycle != two.cycle )
    {
      return false;
    }

    for( SizeIt loopCell = 0; loopCell < one.size.X * one.size.Y; loopCell++ )
    {
      Cell& cellOne = *(( Cell* )one + loopCell );
      Cell& cellTwo = *(( Cell* )two + loopCell );

      if(!( cellOne == cellTwo ))
      {
        return false;
      }
    }

    return true;
  }


  TEST_CLASS(LayerSavingUnitTest), BaseTest
  {
    TEST_CLASS_INITIALIZE(ClassInitialize)
    {
      WriteMessage( "Layer Saving Tests initialized" );
      tempFolder = L"";
      TCHAR buffer[ MAX_PATH ];
      DWORD length = GetTempPath( MAX_PATH, buffer );

      if( length != 0 && length <= MAX_PATH )
      {
        tempFolder = wstring( buffer ) + L"ItsUnitTest";

        if( _wmkdir( tempFolder.c_str()) != 0 && errno != EEXIST )
        {
          tempFolder = L"";
        }
      }
    }

    TEST_CLASS_CLEANUP(ClassCleanup)
    {
      WriteMessage( "Layer Saving tests Cleanup" );

      if( tempFolder.size() > 0 )
      {
        WIN32_FIND_DATA findData;
        HANDLE findHandle;
        wstring tempFolderAllFiles = tempFolder + L"\\*.*";

        findHandle = FindFirstFile( tempFolderAllFiles.c_str(), &findData );

        if( findHandle != INVALID_HANDLE_VALUE )
        {
          do {
            wstring fileToDelete = tempFolder + L"\\" + findData.cFileName;
            wstring fileName = findData.cFileName;
            if( fileName == L"." || fileName == L".." ) continue;
            if( _wunlink( fileToDelete.c_str() ) != 0 ) break; 
          } while ( FindNextFile( findHandle, &findData ));

          FindClose( findHandle );
        }

        if( _wrmdir( tempFolder.c_str()) != 0 )
        {
          WriteMessage( L"Can not clear temp folder: %s", tempFolder );
        }
      }
    }

  public:
    TEST_METHOD(LayerSavingInMemory)
    {
      WriteMessage( "LayerSavingInMemory" );

      ISaverInMemory saver;
      ISaverInMemory saver_with_correction( true );

      WriteMessage( "In memory saver" );
      LayerSavingTest( saver, false );

      WriteMessage( "In memory saver with autocorrection" );
      LayerSavingTest( saver_with_correction, true );
    }

    TEST_METHOD(LayerSaving)
    {
      WriteMessage( "LayerSaving" );

      if( tempFolder.empty() )
      {
        Assert::Fail( L"LayerSaving test fails as no temp folder is found" );
      }

      std::string folder( tempFolder.begin(), tempFolder.end() );
      bool autoCorrectionDisabled = false;

      ISaverFile saver( folder.c_str(), autoCorrectionDisabled );
      ISaverFileCached cached_1_saver( folder.c_str(), 1, autoCorrectionDisabled ); // effectively no caching
      ISaverFileCached cached_10_saver( folder.c_str(), 10, autoCorrectionDisabled );

      WriteMessage( "Simple saver" );
      LayerSavingTest( saver, autoCorrectionDisabled );
      LayerSavingTest( saver, autoCorrectionDisabled );
      saver.Flush();
      LayerSavingTest( saver, autoCorrectionDisabled );

      WriteMessage( "Cached saver with ratio 1 (non-caching)" );
      LayerSavingTest( cached_1_saver, autoCorrectionDisabled );
      LayerSavingTest( cached_1_saver, autoCorrectionDisabled );
      cached_1_saver.Flush();
      LayerSavingTest( cached_1_saver, autoCorrectionDisabled );
      
      WriteMessage( "Cached saver with ratio 10 (caching)" );
      LayerSavingTest( cached_10_saver, autoCorrectionDisabled );
      LayerSavingTest( cached_10_saver, autoCorrectionDisabled );
      cached_10_saver.Flush();
      LayerSavingTest( cached_10_saver, autoCorrectionDisabled );

      bool autoCorrectionEnabled = true;
      ISaverFile saver_with_correction( folder.c_str(), autoCorrectionEnabled );
      ISaverFileCached cached_1_saver_with_correction( folder.c_str(), 1, autoCorrectionEnabled ); // effectively no caching
      ISaverFileCached cached_10_saver_with_correction( folder.c_str(), 10, autoCorrectionEnabled );

      WriteMessage( "Simple saver with autocorrection" );
      LayerSavingTest( saver_with_correction, autoCorrectionEnabled );
      WriteMessage( "Cached saver with ratio 1 and autocorrection" );
      LayerSavingTest( cached_1_saver_with_correction, autoCorrectionEnabled );
      WriteMessage( "Cached saver with ratio 10 and autocorrection" );
      LayerSavingTest( cached_10_saver_with_correction, autoCorrectionEnabled );
    }
    
  private:
    void SetCellsInLayer( Celllayer& layer, ByteIt type, ByteIt data, ByteIt updata, 
                          ByteIt uplink, ByteIt shape )
    {
      for( SizeIt loopCell = 0; loopCell < layer.size.X * layer.size.Y; loopCell++ )
      {
        Cell& cell = *(( Cell* )layer + loopCell );

        cell.type       = type;
        cell.data       = data;
        cell.updata     = updata;
        cell.uplink     = uplink;
        cell.reflection = INITIAL_REFLECTION;

        memset( cell.shape,       shape,        sizeof( cell.shape ));
        memset( cell.reflections, INITIAL_DATA, sizeof( cell.reflections ));
        memset( cell.memory,      INITIAL_DATA, sizeof( cell.memory ));
      }
    }

    // TODO: write a test for Header/Void
    void LayerSavingTest( ISaver& saver, bool autoCorrection )
    {
      Celllayer testLayer1( "testLayer1", SizeIt2D( 3, 3 ), 33, 55 );
      Celllayer testLayer2( "testLayer1", SizeIt2D( 3, 3 ), 33, 55 );
      SetCellsInLayer( testLayer1, GOOD_CELL, '1', '2', '3', '4' );
      SetCellsInLayer( testLayer2, GOOD_CELL, '1', '2', '3', '4' );
      Assert::IsTrue( testLayer1 == testLayer2 );
      SetCellsInLayer( testLayer2, GOOD_CELL, '1', '2', '3', '7' );
      Assert::IsFalse( testLayer1 == testLayer2 );

      // name must be the same
      Celllayer layer( "layer", SizeIt2D( 0, 0 ), 0, 0 );
      Celllayer layerToCompare( "layer", SizeIt2D( 0, 0 ), 0, 0 );

      WriteMessage( "Saving empty layer" );
      Assert::IsTrue( layer == layerToCompare );
      saver.Save( layer );
      Assert::IsTrue( saver.Exist( layer ));
      saver.Load( layerToCompare );
      Assert::IsTrue( layer == layerToCompare );

      WriteMessage( "Saving empty layer with changed members (but cells)" );
      layerToCompare.maiTrixName = "changedlayer";
      layerToCompare.type = Layer_t::textlayer;
      layerToCompare.positionZ = 11;
      layerToCompare.cycle = 22;
      layer = layerToCompare;
      Assert::IsTrue( layer == layerToCompare );
      saver.Save( layer );
      Assert::IsTrue( saver.Exist( layer ));
      saver.Load( layerToCompare );
      Assert::IsTrue( layer == layerToCompare );

      WriteMessage( "Saving full layer" );
      Celllayer layerFull( "layerfull", SizeIt2D( 2, 2 ), 33, 55 );

      SetCellsInLayer( layerFull, GOOD_CELL, 'd', 'u', 'l', 's' );

      layer = layerFull;
      Assert::IsTrue( layer == layerFull );
      saver.Save( layer );
      SetCellsInLayer( layerFull, NO_CELL, '0', '0', '0', '0' );
      Assert::IsTrue( saver.Exist( layer ));
      saver.Load( layerFull );
      Assert::IsTrue( layer == layerFull );

      WriteMessage( "Saving multiple layers" );
      Celllayer layer1, layer2, layer3;
      layer.positionZ++;
      layer1 = layer;
      SetCellsInLayer( layer1, BAD_CELL, '1', '1', '1', '1' );
      saver.Save( layer1 );
      Assert::IsTrue( saver.Exist( layer1 ));
      layer.positionZ++;
      layer2 = layer;
      SetCellsInLayer( layer2, BAD_CELL, '2', '2', '2', '2' );
      saver.Save( layer2 );
      Assert::IsTrue( saver.Exist( layer2 ));
      layer.positionZ++;
      layer3 = layer;
      SetCellsInLayer( layer3, BAD_CELL, '3', '3', '3', '3' );
      saver.Save( layer3 );
      Assert::IsTrue( saver.Exist( layer3 ));

      layer.positionZ = layer1.positionZ;
      SetCellsInLayer( layer, NO_CELL, '0', '0', '0', '0' );
      saver.Load( layer );
      Assert::IsTrue( layer == layer1 );
      Assert::IsFalse( layer == layer2 );
      Assert::IsFalse( layer == layer3 );

      layer.positionZ = layer2.positionZ;
      SetCellsInLayer( layer, NO_CELL, '0', '0', '0', '0' );
      saver.Load( layer );
      Assert::IsTrue( layer == layer2 );
      Assert::IsFalse( layer == layer1 );
      Assert::IsFalse( layer == layer3 );

      layer.positionZ = layer3.positionZ;
      SetCellsInLayer( layer, NO_CELL, '0', '0', '0', '0' );
      saver.Load( layer );
      Assert::IsTrue( layer == layer3 );
      Assert::IsFalse( layer == layer1 );
      Assert::IsFalse( layer == layer2 );

      WriteMessage( "Saving multiple layers over several cycles" );

      for( PeriodIt loopCycle = 0; loopCycle < 200; loopCycle++ )
      {
        layer1.cycle++;
        layer2.cycle++;
        layer3.cycle++;
      
        saver.Save( layer1 );
        saver.Save( layer2 );
        saver.Save( layer3 );
        Assert::IsTrue( saver.Exist( layer1 ));
        Assert::IsTrue( saver.Exist( layer2 ));
        Assert::IsTrue( saver.Exist( layer3 ));

        layer = layer1;
        SetCellsInLayer( layer, NO_CELL, '0', '0', '0', '0' );
        saver.Load( layer );
        Assert::IsTrue( layer == layer1 );

        layer.positionZ++;
        saver.Load( layer );
        Assert::IsTrue( layer == layer2 );

        layer.positionZ++;
        if( autoCorrection )
        {
          layer.cycle = 1;
        }

        saver.Load( layer );

        if( autoCorrection )
        {
          Assert::IsFalse( layer == layer3 );
          layer.cycle = layer3.cycle;
        }

        Assert::IsTrue( layer == layer3 );
      }

      if( autoCorrection )
      {
        Celllayer layer_to_correct( "layer_to_correct", SizeIt2D( 5, 5 ), 88, 99 );
        SetCellsInLayer( layer_to_correct, GOOD_CELL, 'a', 'b', 'c', 'd' );

        layer = layer_to_correct;
        Assert::IsTrue( layer == layer_to_correct );
        saver.Save( layer );
        Assert::IsTrue( saver.Exist( layer ));

        SetCellsInLayer( layer, NO_CELL, '0', '0', '0', '0' );
        layer.cycle++;
        saver.Load( layer );

        Assert::IsFalse( layer == layer_to_correct );
        layer_to_correct.cycle++;
        Assert::IsTrue( layer == layer_to_correct );
      }
    }

    static wstring tempFolder;
  };

  wstring LayerSavingUnitTest::tempFolder = L"";
}
