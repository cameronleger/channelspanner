#include <cstdio>
#include <cmath>
#include <cassert>
#include <jansson.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>

#include "logging.h"
#include "aeffect.h"
#include "aeffectx.h"
#include "lglw.h"
#include "spectrumgenerator.h"
#include "spectrumdraw.h"
#include "spanner.h"

#define EDITWIN_W 650
#define EDITWIN_H 400

#define RESOLUTION_MAX 4

uint FFT_SCALE_MAX = (uint)log2( MAX_FFT ) - 8;
#define FFT_SCALER(r) (256 * (uint)exp2f(r))

#define COLOR_MAX 6

// Since the host is expecting a very specific API we need to make sure it has C linkage (not C++)
extern "C" {
extern AEffect* VSTPluginMain( audioMasterCallback vstHostCallback );
}

const VstInt32 PLUGIN_VERSION = 1000;

extern "C" {
static void loc_mouse_cbk( lglw_t _lglw, int32_t _x, int32_t _y, uint32_t _buttonState, uint32_t _changedButtonState );
static void loc_focus_cbk( lglw_t _lglw, uint32_t _focusState, uint32_t _changedFocusState );
static lglw_bool_t loc_keyboard_cbk( lglw_t _lglw, uint32_t _vkey, uint32_t _kmod, lglw_bool_t _bPressed );
static void loc_timer_cbk( lglw_t _lglw );
static void loc_redraw_cbk( lglw_t _lglw );
}

/**
 * Encapsulates the plugin as a C++ class. It will keep both the host callback and the structure required by the
 * host (VSTPlugin). This class will be stored in the `VSTPlugin.object` field (circular reference) so that it can
 * be accessed when the host calls the plugin back (for example in `processDoubleReplacing`).
 */
class VSTPluginWrapper
{
public:
   static long counter;

   ERect editor_rect;
   lglw_t lglw;

   float sampleRate = 44100.0f;
   uint8_t fftScale = 1;
   float reactivity = 0.25f;
   uint8_t color = 0;
   uint8_t windowScale = 1;
   uint8_t group = 1;

   uint32_t redraw_ival_ms = 1000 / 60;
//   uint32_t redraw_ival_ms = 0;

   int process = 1;

public:
   VSTPluginWrapper( audioMasterCallback vstHostCallback,
                     VstInt32 vendorUniqueID,
                     VstInt32 vendorVersion,
                     VstInt32 numParams,
                     VstInt32 numPrograms,
                     VstInt32 numInputs,
                     VstInt32 numOutputs );

   ~VSTPluginWrapper();

   inline AEffect* getVSTPlugin()
   {
      return &_vstPlugin;
   }

   inline VstInt32 getNumInputs() const
   {
      return _vstPlugin.numInputs;
   }

   inline VstInt32 getNumOutputs() const
   {
      return _vstPlugin.numOutputs;
   }

   int openEffect()
   {
      initTrack();
      shmem = open_shared_memory( VSTPluginWrapper::counter++ );
      return 1;
   }

   void closeEffect()
   {
      closeEditor();
      freeCtx();
      close_shared_memory( shmem );
      freeTrack();
      if( nullptr != savedState )
      {
         ::free( savedState );
         savedState = nullptr;
      }
   }

   void initTrack()
   {
      if ( nullptr != track )
         freeTrack();
      track = init_sample_data( FFT_SCALER(fftScale) );
      track->color = color;
      track->group = group;
   }

   void initCtx()
   {
      if ( nullptr != ctx )
         freeCtx();
      ctx = init_draw_ctx( windowScale, sampleRate );
   }

   void freeTrack()
   {
      if ( nullptr != track )
         free_sample_data( track );
      track = nullptr;
   }

   void freeCtx()
   {
      if ( nullptr != ctx )
         free_draw_ctx( ctx );
      ctx = nullptr;
   }

   void updateFFTSize()
   {
      update_frame_size( track, FFT_SCALER(fftScale) );
   }

   void updateTrack()
   {
      process_samples( track, reactivity );
      update_shared_memory( shmem, track );
   }

   void openEditor( void* wnd )
   {
      initCtx();

      (void) lglw_window_open( lglw, wnd, 0, 0, editor_rect.right, editor_rect.bottom );

      lglw_mouse_callback_set( lglw, &loc_mouse_cbk );
      lglw_focus_callback_set( lglw, &loc_focus_cbk );
      lglw_keyboard_callback_set( lglw, &loc_keyboard_cbk );
      lglw_timer_callback_set( lglw, &loc_timer_cbk );
      lglw_redraw_callback_set( lglw, &loc_redraw_cbk );

      lglw_timer_start( lglw, redraw_ival_ms );
   }

   void closeEditor()
   {
      lglw_window_close( lglw );
   }

   track_t* getTrack()
   {
      return track;
   }

   void redrawWindow()
   {
      // Save host GL context
      lglw_glcontext_push( lglw );

      draw( ctx, track, shmem );

      lglw_swap_buffers( lglw );

      // Restore host GL context
      lglw_glcontext_pop( lglw );
   }

   void setMousePosition( int32_t x, int32_t y )
   {
      set_mouse( ctx, x, y );
   }

   void setSampleRate( float _rate )
   {
      sampleRate = _rate;

      if ( nullptr != ctx )
      {
         ctx->sr = sampleRate;
         ctx->fm = sampleRate / 2.0f;
      }
   }

   float getParameter( int uniqueParamId )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Request: %i\n", uniqueParamId );
         return 0;
      case 0:
         return (float) fftScale / FFT_SCALE_MAX;
      case 1:
         return sqrtf( reactivity );
      case 2:
         return (float) (group - 1) / (MAX_INSTANCES - 1);
      case 3:
         return (float) color / COLOR_MAX;
      case 4:
         return (float) (windowScale - 1) / RESOLUTION_MAX;
      }
   }

   void setParameter( int uniqueParamId, float value )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Name Request: %i\n", uniqueParamId );
         break;
      case 0:
      {
         fftScale = (uint8_t) roundf( value * FFT_SCALE_MAX );
         if ( track->frameSize != FFT_SCALER( fftScale ) )
            updateFFTSize();
         break;
      }
      case 1:
         reactivity = value * value;
         break;
      case 2:
      {
         group = (uint8_t) (roundf( value * (MAX_INSTANCES - 1) ) + 1);
         if ( nullptr != track )
            track->group = group;
         break;
      }
      case 3:
      {
         color = (uint8_t) roundf( value * COLOR_MAX );
         if ( nullptr != track )
            track->color = color;
         break;
      }
      case 4:
      {
         windowScale = (uint8_t) roundf( value * (RESOLUTION_MAX - 1) + 1 );
         if ( editor_rect.bottom != EDITWIN_H * windowScale)
         {
            editor_rect.left = 0;
            editor_rect.top = 0;
            editor_rect.right = (VstInt16) (EDITWIN_W * windowScale);
            editor_rect.bottom = (VstInt16) (EDITWIN_H * windowScale);

            freeCtx();
            (void) lglw_window_resize( lglw, editor_rect.right, editor_rect.bottom );
            initCtx();
         }
         break;
      }
      }
   }

   void getParameterName( int uniqueParamId, char* s, size_t sMaxLen )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Name Request: %i\n", uniqueParamId );
         break;
      case 0:
         ::strncpy( s, "FFTSize", sMaxLen );
         break;
      case 1:
         ::strncpy( s, "Speed", sMaxLen );
         break;
      case 2:
         ::strncpy( s, "Group", sMaxLen );
         break;
      case 3:
         ::strncpy( s, "Color", sMaxLen );
         break;
      case 4:
         ::strncpy( s, "WinSize", sMaxLen );
         break;
      }
   }

   void getParameterValue( int uniqueParamId, char* s, size_t sMaxLen )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Value Request: %i\n", uniqueParamId );
         break;
      case 0:
         ::snprintf( s, sMaxLen, "%i", FFT_SCALER(fftScale) );
         break;
      case 1:
         ::snprintf( s, sMaxLen, "%.2f", reactivity );
         break;
      case 2:
         ::snprintf( s, sMaxLen, "%i", group );
         break;
      case 3:
         ::snprintf( s, sMaxLen, "%i", color );
         break;
      case 4:
         ::snprintf( s, sMaxLen, "%i", windowScale );
         break;
      }
   }

   void getParameterUnits( int uniqueParamId, char* s, size_t sMaxLen )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Unit Request: %i\n", uniqueParamId );
         break;
      case 0:
         ::strncpy( s, "", sMaxLen );
         break;
      case 1:
         ::strncpy( s, "%", sMaxLen );
         break;
      case 2:
         ::strncpy( s, "", sMaxLen );
         break;
      case 3:
         ::strncpy( s, "", sMaxLen );
         break;
      case 4:
         ::strncpy( s, "x", sMaxLen );
         break;
      }
   }

   int getParameterProperties( int uniqueParamId, VstParameterProperties* prop )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Property Request: %i\n", uniqueParamId );
         return 0;
      case 0:
         prop->stepFloat = 1.0f / FFT_SCALE_MAX;
         prop->smallStepFloat = 1.0f / FFT_SCALE_MAX;
         prop->largeStepFloat = 1.0f / FFT_SCALE_MAX;

         ::strncpy( prop->label, "FFT Size", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "FFT Sz", kVstMaxShortLabelLen );

         prop->displayIndex = 0;
         prop->category = 1;
         prop->numParametersInCategory = 5;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 1:
         ::strncpy( prop->label, "Speed", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "Speed", kVstMaxShortLabelLen );

         prop->displayIndex = 1;
         prop->category = 1;
         prop->numParametersInCategory = 5;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 2:
         prop->stepFloat = 1.0f / MAX_INSTANCES;
         prop->smallStepFloat = prop->stepFloat;
         prop->largeStepFloat = prop->stepFloat;

         ::strncpy( prop->label, "Group", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "Group", kVstMaxShortLabelLen );

         prop->displayIndex = 2;
         prop->category = 1;
         prop->numParametersInCategory = 5;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 3:
         prop->stepFloat = 1.0f / COLOR_MAX;
         prop->smallStepFloat = prop->stepFloat;
         prop->largeStepFloat = prop->stepFloat;

         ::strncpy( prop->label, "Color", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "Color", kVstMaxShortLabelLen );

         prop->displayIndex = 3;
         prop->category = 1;
         prop->numParametersInCategory = 5;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 4:
         prop->stepFloat = 1.0f / RESOLUTION_MAX;
         prop->smallStepFloat = prop->stepFloat;
         prop->largeStepFloat = prop->stepFloat;

         ::strncpy( prop->label, "Window Size", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "WnSize", kVstMaxShortLabelLen );

         prop->displayIndex = 4;
         prop->category = 1;
         prop->numParametersInCategory = 5;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;
      }
   }

   int getSavedState( uint8_t** _addr )
   {
      if ( nullptr != savedState )
      {
         ::free( savedState );
         savedState = nullptr;
      }

      json_t* rootJ = json_object();

      json_t* fts = json_integer( fftScale );
      json_object_set_new( rootJ, "fftScale", fts );

      json_t* rct = json_real( reactivity );
      json_object_set_new( rootJ, "reactivity", rct );

      json_t* col = json_integer( color );
      json_object_set_new( rootJ, "color", col );

      json_t* wns = json_integer( windowScale );
      json_object_set_new( rootJ, "windowScale", wns );

      json_t* grp = json_integer( group );
      json_object_set_new( rootJ, "group", grp );

      savedState = json_dumps( rootJ, JSON_INDENT( 2 ) | JSON_REAL_PRECISION( 4 ) );
      json_decref( rootJ );

      DEBUG_PRINT( "Saving patch state: %s\n", savedState );

      if ( nullptr != savedState )
      {
         *_addr = (uint8_t*) savedState;
         return (int) strlen( savedState ) + 1/*ASCIIZ*/;
      }
      return 0;
   }

   int setSavedState( size_t _size, uint8_t* _addr )
   {
      int r;
      json_error_t error = {0};
      json_t* rootJ = json_loads( (const char*) _addr, 0/*flags*/, &error );

      if ( rootJ )
      {
         DEBUG_PRINT( "Loading patch state: %s\n", (const char*) _addr );

         {
            json_t* fts = json_object_get( rootJ, "fftScale" );
            if ( fts ) fftScale = uint8_t( json_number_value( fts ) );
         }

         {
            json_t* rct = json_object_get( rootJ, "reactivity" );
            if ( rct ) reactivity = float( json_number_value( rct ) );
         }

         {
            json_t* col = json_object_get( rootJ, "color" );
            if ( col ) color = uint8_t( json_number_value( col ) );
         }

         {
            json_t* wns = json_object_get( rootJ, "windowScale" );
            if ( wns ) windowScale = uint8_t( json_number_value( wns ) );
         }

         {
            json_t* grp = json_object_get( rootJ, "group" );
            if ( grp ) group = uint8_t( json_number_value( grp ) );
         }

         json_decref( rootJ );

         r = 1;
      }
      else
      {
         DEBUG_PRINT( "JSON parsing error at %s %d:%d %s\n", error.source, error.line, error.column, error.text );
         r = 0;
      }

      if ( r == 1 )
      {
         initTrack();
         initCtx();
      }
      return r;
   }

private:
   audioMasterCallback _vstHostCallback;
   AEffect _vstPlugin;
   shared_memory_t* shmem = nullptr;
   track_t* track = nullptr;
   draw_ctx_t* ctx = nullptr;
   char* savedState = nullptr;
};


/*******************************************
 * Callbacks: Host -> Plugin
 *
 * Defined here because they are used in the rest of the code later
 */

extern "C" {
void VSTPluginProcessSamplesFloat32( AEffect* vstPlugin, float** inputs, float** outputs, VstInt32 sampleFrames )
{
   auto* wrapper = static_cast<VSTPluginWrapper*>(vstPlugin->object);
   DEBUG_PRINT( "Processing %i sample frames for %i inputs (max %i)\n", sampleFrames, wrapper->getNumInputs(), MAX_CHANNELS );

   track_t* track = wrapper->getTrack();

   for ( int i = 0; i < wrapper->getNumInputs() && i < MAX_CHANNELS; i++ )
   {
      auto inputSamples = inputs[i];
      auto outputSamples = outputs[i];

      if ( 1 == wrapper->process )
         add_sample_data( track, (size_t)i, inputSamples, (size_t)sampleFrames );

      if ( nullptr != inputSamples && nullptr != outputSamples && inputSamples != outputSamples )
         memcpy( outputSamples, inputSamples, (size_t) sampleFrames * sizeof( float ) );
   }

   if ( 1 == wrapper->process )
      wrapper->updateTrack();
}
}

extern "C" {
VstIntPtr
VSTPluginDispatcher( AEffect* vstPlugin, VstInt32 opCode, VstInt32 index, VstIntPtr value, void* ptr, float opt )
{
   VstIntPtr r = 0;

   auto* wrapper = static_cast<VSTPluginWrapper*>(vstPlugin->object);
   switch ( opCode )
   {
   default:
      DEBUG_PRINT( "unhandled VSTPluginDispatcher opcode=%d\n", opCode );
      break;

   case effGetVstVersion:
      DEBUG_PRINT( "effGetVstVersion\n" );
      r = kVstVersion;
      break;

   case effGetPlugCategory:
      DEBUG_PRINT( "effGetPlugCategory\n" );
      return kPlugCategEffect;

   case effOpen:
      DEBUG_PRINT( "effOpen\n" );
      r = wrapper->openEffect();
      break;

   case effClose:
      DEBUG_PRINT( "effClose\n" );
      wrapper->closeEffect();
      delete wrapper;
      break;

   case effGetEffectName:
      DEBUG_PRINT( "effGetEffectName\n" );
      ::strncpy( (char*) ptr, "Channel Spanner", kVstMaxEffectNameLen );
      r = 1;
      break;

   case effGetProductString:
      DEBUG_PRINT( "effGetProductString\n" );
      ::strncpy( (char*) ptr, "Channel Spanner", kVstMaxProductStrLen );
      r = 1;
      break;

   case effGetVendorString:
      DEBUG_PRINT( "effGetVendorString\n" );
      ::strncpy( static_cast<char*>(ptr), "Cameron Leger", kVstMaxVendorStrLen );
      r = 1;
      break;

   case effGetVendorVersion:
      DEBUG_PRINT( "effGetVendorVersion\n" );
      return PLUGIN_VERSION;

   case effGetNumMidiInputChannels:
      DEBUG_PRINT( "effGetNumMidiInputChannels\n" );
      r = 16;
      break;

   case effGetNumMidiOutputChannels:
      DEBUG_PRINT( "effGetNumMidiOutputChannels\n" );
      r = 0;
      break;

   case effCanDo:
      DEBUG_PRINT( "effCanDo\n" );
      // ptr:
      // "sendVstEvents"
      // "sendVstMidiEvent"
      // "sendVstTimeInfo"
      // "receiveVstEvents"
      // "receiveVstMidiEvent"
      // "receiveVstTimeInfo"
      // "offline"
      // "plugAsChannelInsert"
      // "plugAsSend"
      // "mixDryWet"
      // "noRealTime"
      // "multipass"
      // "metapass"
      // "1in1out"
      // "1in2out"
      // "2in1out"
      // "2in2out"
      // "2in4out"
      // "4in2out"
      // "4in4out"
      // "4in8out"
      // "8in4out"
      // "8in8out"
      // "midiProgramNames"
      // "conformsToWindowRules"
      if ( !strcmp( (char*) ptr, "receiveVstEvents" ) )
         r = 1;
      else if ( !strcmp( (char*) ptr, "receiveVstMidiEvent" ) )  // (note) required by Jeskola Buzz
         r = 1;
      else if ( !strcmp( (char*) ptr, "2in2out" ) )
         r = 1;
      else if ( !strcmp( (char*) ptr, "1in1out" ) )
         r = 1;
      else if ( !strcmp( (char*) ptr, "noRealTime" ) )
         r = 1;
      else
         r = 0;
      break;

   case effGetTailSize:
//      DEBUG_PRINT( "effGetTailSize\n" );
      r = 1;
      break;

   case effGetInputProperties:
      DEBUG_PRINT( "effGetInputProperties\n" );
      {
         auto* pin = (VstPinProperties*) ptr;
         ::snprintf( pin->label, kVstMaxLabelLen, "Input #%d", index );
         pin->flags = kVstPinIsActive | ((0 == (index & 1)) ? kVstPinIsStereo : 0);
         pin->arrangementType = ((0 == (index & 1)) ? kSpeakerArrStereo : kSpeakerArrMono);
         ::snprintf( pin->shortLabel, kVstMaxShortLabelLen, "in%d", index );
         memset( (void*) pin->future, 0, 48 );
         r = 1;
      }
      break;

   case effGetOutputProperties:
      DEBUG_PRINT( "effGetOutputProperties\n" );
      {
         auto* pin = (VstPinProperties*) ptr;
         ::snprintf( pin->label, kVstMaxLabelLen, "Output #%d", index );
         pin->flags = kVstPinIsActive | ((0 == (index & 1)) ? kVstPinIsStereo : 0);
         pin->arrangementType = ((0 == (index & 1)) ? kSpeakerArrStereo : kSpeakerArrMono);
         ::snprintf( pin->shortLabel, kVstMaxShortLabelLen, "out%d", index );
         memset( (void*) pin->future, 0, 48 );
         r = 1;
      }
      break;

   case effSetSampleRate:
      DEBUG_PRINT( "effSetSampleRate\n" );
      wrapper->setSampleRate( opt );
      r = 1;
      break;

   case effSetBlockSize:
      DEBUG_PRINT( "effSetBlockSize\n" );
      r = 1;
      break;

   case effMainsChanged:
      DEBUG_PRINT( "effMainsChanged\n" );
      wrapper->process = ( value == 0 ) ? 0 : 1;
      r = 1;
      break;

   case effSetProgram:
//      DEBUG_PRINT( "effSetProgram\n" );
      r = 1;
      break;

   case effGetProgram:
//      DEBUG_PRINT( "effGetProgram\n" );
      r = 0;
      break;

   case effGetProgramName:
      DEBUG_PRINT( "effGetProgramName\n" );
      ::snprintf( (char*) ptr, kVstMaxProgNameLen, "default" );
      r = 1;
      break;

   case effSetProgramName:
      DEBUG_PRINT( "effSetProgramName\n" );
      r = 1;
      break;

   case effGetProgramNameIndexed:
      DEBUG_PRINT( "effGetProgramNameIndexed\n" );
      ::sprintf( (char*) ptr, "default" );
      r = 1;
      break;

   case effGetParamName:
      DEBUG_PRINT( "effGetParamName\n" );
      wrapper->getParameterName( index, (char*) ptr, kVstMaxParamStrLen );
      r = 1;
      break;

   case effGetParamLabel:
      DEBUG_PRINT( "effGetParamLabel\n" );
      wrapper->getParameterUnits( index, (char*) ptr, kVstMaxParamStrLen );
      break;

   case effGetParamDisplay:
      DEBUG_PRINT( "effGetParamDisplay\n" );
      wrapper->getParameterValue( index, (char*) ptr, kVstMaxParamStrLen );
      break;

   case effCanBeAutomated:
      DEBUG_PRINT( "effCanBeAutomated\n" );
      r = 1;
      break;

   case effStartProcess:
      DEBUG_PRINT( "effStartProcess\n" );
      wrapper->process = 1;
      r = 1;
      break;

   case effStopProcess:
      DEBUG_PRINT( "effStopProcess\n" );
      wrapper->process = 0;
      r = 1;
      break;

   case effEditIdle:
//      DEBUG_PRINT( "effEditIdle\n" );
      if ( lglw_window_is_visible( wrapper->lglw ) )
      {
         lglw_events( wrapper->lglw );
         if( 0 == wrapper->redraw_ival_ms )
         {
            wrapper->redrawWindow();
         }
      }

      break;

   case effEditGetRect:
      DEBUG_PRINT( "effEditGetRect\n" );
      if ( nullptr != ptr )
      {
         wrapper->editor_rect.left = 0;
         wrapper->editor_rect.top = 0;
         wrapper->editor_rect.right = (VstInt16)(EDITWIN_W * wrapper->windowScale);
         wrapper->editor_rect.bottom = (VstInt16)(EDITWIN_H * wrapper->windowScale);
         *(void**) ptr = (void*) &wrapper->editor_rect;
         r = 1;
      }
      else
      {
         r = 0;
      }
      break;

   case effEditOpen:
      DEBUG_PRINT( "effEditOpen\n" );
      wrapper->openEditor( ptr );
      r = 1;
      break;

   case effEditClose:
      DEBUG_PRINT( "effEditClose\n" );
      wrapper->closeEditor();
      r = 1;
      break;

   case effGetParameterProperties:
      DEBUG_PRINT( "effGetParameterProperties\n" );
      r = wrapper->getParameterProperties( index, (VstParameterProperties*) ptr );
      break;

   case effGetChunk:
      DEBUG_PRINT( "effGetChunk\n" );
      r = wrapper->getSavedState( (uint8_t**) ptr );
      break;

   case effSetChunk:
      DEBUG_PRINT( "effSetChunk\n" );
      r = wrapper->setSavedState( size_t( value ), (uint8_t*) ptr );
      break;

   case effGetMidiKeyName:
//      DEBUG_PRINT( "effGetMidiKeyName\n" );
      r = 0;
      break;

   case effBeginSetProgram:
      DEBUG_PRINT( "effBeginSetProgram\n" );
      break;

   case effEndSetProgram:
      DEBUG_PRINT( "effEndSetProgram\n" );
      break;

   case effBeginLoadBank:
      DEBUG_PRINT( "effBeginLoadBank\n" );
      break;

   case effBeginLoadProgram:
      DEBUG_PRINT( "effBeginLoadProgram\n" );
      break;

   }

   return r;
}
}

extern "C" {
void VSTPluginSetParameter( AEffect* vstPlugin, VstInt32 index, float parameter )
{
   DEBUG_PRINT( "VSTPluginSetParameter: %i = %f\n", index, parameter );
   auto* wrapper = static_cast<VSTPluginWrapper*>(vstPlugin->object);
   wrapper->setParameter( index, parameter );
}
}

extern "C" {
float VSTPluginGetParameter( AEffect* vstPlugin, VstInt32 index )
{
   DEBUG_PRINT( "VSTPluginGetParameter: %i\n", index );
   auto* wrapper = static_cast<VSTPluginWrapper*>(vstPlugin->object);
   return wrapper->getParameter( index );
}
}

extern "C" {
static void loc_mouse_cbk( lglw_t _lglw, int32_t _x, int32_t _y, uint32_t _buttonState, uint32_t _changedButtonState )
{
   (void)_buttonState;
   (void)_changedButtonState;
   auto* wrapper = (VSTPluginWrapper*) lglw_userdata_get( _lglw );
   wrapper->setMousePosition( _x, _y );
}

static void loc_focus_cbk( lglw_t _lglw, uint32_t _focusState, uint32_t _changedFocusState )
{
   (void)_changedFocusState;
   if ( _focusState == 0 )
   {
      auto* wrapper = (VSTPluginWrapper*) lglw_userdata_get( _lglw );
      wrapper->setMousePosition( 0, 0 );
   }
}

static lglw_bool_t loc_keyboard_cbk( lglw_t _lglw, uint32_t _vkey, uint32_t _kmod, lglw_bool_t _bPressed )
{
   (void)_lglw;
   (void)_vkey;
   (void)_kmod;
   (void)_bPressed;
   return LGLW_FALSE;
}

static void loc_timer_cbk( lglw_t _lglw )
{
   lglw_redraw( _lglw );
}

static void loc_redraw_cbk( lglw_t _lglw )
{
   auto* wrapper = (VSTPluginWrapper*) lglw_userdata_get( _lglw );
   wrapper->redrawWindow();
}
}

VSTPluginWrapper::VSTPluginWrapper( audioMasterCallback vstHostCallback,
                                    VstInt32 vendorUniqueID,
                                    VstInt32 vendorVersion,
                                    VstInt32 numParams,
                                    VstInt32 numPrograms,
                                    VstInt32 numInputs,
                                    VstInt32 numOutputs ) :
        editor_rect(),
        _vstHostCallback( vstHostCallback ),
        _vstPlugin()
{
   memset( &_vstPlugin, 0, sizeof( _vstPlugin ) );

   _vstPlugin.magic = kEffectMagic;

   _vstPlugin.object = this;

   _vstPlugin.flags =
           effFlagsNoSoundInStop |
           effFlagsProgramChunks |
           effFlagsCanReplacing |
           effFlagsHasEditor;

   _vstPlugin.uniqueID = vendorUniqueID;
   _vstPlugin.version = vendorVersion;
   _vstPlugin.numParams = numParams;
   _vstPlugin.numPrograms = numPrograms;
   _vstPlugin.numInputs = numInputs;
   _vstPlugin.numOutputs = numOutputs;

   _vstPlugin.dispatcher = VSTPluginDispatcher;
   _vstPlugin.getParameter = VSTPluginGetParameter;
   _vstPlugin.setParameter = VSTPluginSetParameter;
   _vstPlugin.processReplacing = VSTPluginProcessSamplesFloat32;

   editor_rect.top = 0;
   editor_rect.left = 0;
   editor_rect.right = (VstInt16)(EDITWIN_W * windowScale);
   editor_rect.bottom = (VstInt16)(EDITWIN_H * windowScale);

   lglw = lglw_init( editor_rect.right, editor_rect.bottom );

   lglw_userdata_set( lglw, this );

   track = nullptr;
   ctx = nullptr;
}

VSTPluginWrapper::~VSTPluginWrapper()
{
   lglw_exit( lglw );
}

long VSTPluginWrapper::counter = 100;

extern AEffect
*
VSTPluginMain( audioMasterCallback
               vstHostCallback )
{
   DEBUG_PRINT( "Debugging is enabled...\n" );

   assert( MAX_FFT > 0 );
   assert( MAX_FFT % 256 == 0 );
   assert( MAX_CHANNELS > 0 );
   assert( MAX_INSTANCES > 0 );

   DEBUG_PRINT( " Max FFT Size: %i\n", MAX_FFT );
   DEBUG_PRINT( " Max Channels: %i\n", MAX_CHANNELS );
   DEBUG_PRINT( "Max Instances: %i\n", MAX_INSTANCES );

   auto* plugin =
           new VSTPluginWrapper( vstHostCallback,
                                 CCONST( '0', 'e', 't', 'u' ),
                                 PLUGIN_VERSION,
                                 5, // params
                                 0, // programs
                                 MAX_CHANNELS,   // inputs
                                 MAX_CHANNELS ); // outputs

   return plugin->getVSTPlugin();
}
