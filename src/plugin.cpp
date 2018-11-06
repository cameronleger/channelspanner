#include <cstdio>
#include <cmath>
#include <cassert>
#include <X11/Xlib.h>
#include <X11/Xos.h>

#include "logging.h"
#include "aeffect.h"
#include "aeffectx.h"
#include "lglw.h"
#include "spectrumgenerator.h"
#include "spectrumdraw.h"

#define EDITWIN_W 650
#define EDITWIN_H 400

#define RESOLUTION_MAX 4

uint MAX_FFT_SCALE = (uint)log2( MAX_FFT ) - 8;
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
   static VSTPluginWrapper* window_to_wrapper;

   ERect editor_rect;

   lglw_t lglw;

   float sampleRate = 44100.0f;
   uint8_t fftScale = 1;
   float reactivity = 0.25f;
   uint8_t windowScale = 1;
   uint8_t color = 0;

   uint32_t redraw_ival_ms = 1000 / 60;
//   uint32_t redraw_ival_ms = 0;

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
      return 1;
   }

   void closeEffect()
   {
      closeEditor();
      freeTrack();
   }

   void initTrack()
   {
      track = init_sample_data( FFT_SCALER(fftScale) );
      track->color = color;
      ctx = init_draw_ctx( track, windowScale, sampleRate );
   }

   void freeTrack()
   {
      free_draw_ctx( ctx );
      free_sample_data( track );
   }

   void resetTrack()
   {
      freeTrack();
      initTrack();
   }

   void openEditor( void* wnd )
   {

      (void) lglw_window_open( lglw, wnd, 0, 0, editor_rect.right, editor_rect.bottom );

      lglw_mouse_callback_set( lglw, &loc_mouse_cbk );
      lglw_focus_callback_set( lglw, &loc_focus_cbk );
      lglw_keyboard_callback_set( lglw, &loc_keyboard_cbk );
      lglw_timer_callback_set( lglw, &loc_timer_cbk );
      lglw_redraw_callback_set( lglw, &loc_redraw_cbk );

      lglw_timer_start( lglw, redraw_ival_ms );

      window_to_wrapper = this;
   }

   void closeEditor()
   {
      if ( nullptr != window_to_wrapper )
      {
         lglw_window_close( lglw );
         window_to_wrapper = nullptr;
      }
   }

   track_t* getTrack()
   {
      return track;
   }

   static VSTPluginWrapper* FindWrapperByWindow( Window hwnd )
   {
      return window_to_wrapper;
   }

   void redrawWindow()
   {
      // Save host GL context
      lglw_glcontext_push( lglw );

      draw( ctx, track );

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
      int reset = 0;
      if ( sampleRate != _rate ) reset = 1;
      sampleRate = _rate;
      if ( reset == 1 ) resetTrack();
   }

   float getParameter( int uniqueParamId )
   {
      switch ( uniqueParamId )
      {
      default:
         DEBUG_PRINT( "Unused Parameter Request: %i\n", uniqueParamId );
         return 0;
      case 0:
         return (float) fftScale / MAX_FFT_SCALE;
      case 1:
         return sqrtf( reactivity );
      case 2:
         return (float) color / COLOR_MAX;
      case 3:
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
         fftScale = (uint8_t) roundf( value * MAX_FFT_SCALE );
         if ( track->frameSize != FFT_SCALER( fftScale ) )
            resetTrack();
         break;
      }
      case 1:
         reactivity = value * value;
         break;
      case 2:
      {
         color = (uint8_t) roundf( value * COLOR_MAX );
         if ( nullptr != track )
            track->color = color;
         break;
      }
      case 3:
      {
         windowScale = (uint8_t) roundf( value * (RESOLUTION_MAX - 1) + 1 );
         if ( editor_rect.bottom != EDITWIN_H * windowScale)
         {
            editor_rect.left = 0;
            editor_rect.top = 0;
            editor_rect.right = (VstInt16) (EDITWIN_W * windowScale);
            editor_rect.bottom = (VstInt16) (EDITWIN_H * windowScale);
            (void) lglw_window_resize( lglw, editor_rect.right, editor_rect.bottom );
            resetTrack();
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
         ::strncpy( s, "Color", sMaxLen );
         break;
      case 3:
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
         ::snprintf( s, sMaxLen, "%i", color );
         break;
      case 3:
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
         prop->stepFloat = 1.0f / MAX_FFT_SCALE;
         prop->smallStepFloat = 1.0f / MAX_FFT_SCALE;
         prop->largeStepFloat = 1.0f / MAX_FFT_SCALE;

         ::strncpy( prop->label, "FFT Size", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "FFT Sz", kVstMaxShortLabelLen );

         prop->displayIndex = 0;
         prop->category = 1;
         prop->numParametersInCategory = 4;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 1:
         ::strncpy( prop->label, "Speed", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "Speed", kVstMaxShortLabelLen );

         prop->displayIndex = 1;
         prop->category = 1;
         prop->numParametersInCategory = 4;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 2:
         prop->stepFloat = 1.0f / COLOR_MAX;
         prop->smallStepFloat = 1.0f / COLOR_MAX;
         prop->largeStepFloat = 1.0f / COLOR_MAX;

         ::strncpy( prop->label, "Color", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "Color", kVstMaxShortLabelLen );

         prop->displayIndex = 2;
         prop->category = 1;
         prop->numParametersInCategory = 4;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;

      case 3:
         prop->stepFloat = 1.0f / RESOLUTION_MAX;
         prop->smallStepFloat = 1.0f / RESOLUTION_MAX;
         prop->largeStepFloat = 1.0f / RESOLUTION_MAX;

         ::strncpy( prop->label, "Window Size", kVstMaxLabelLen );
         ::strncpy( prop->shortLabel, "WnSize", kVstMaxShortLabelLen );

         prop->displayIndex = 3;
         prop->category = 1;
         prop->numParametersInCategory = 4;

         ::strncpy( prop->categoryLabel, "Channel Spanner", kVstMaxCategLabelLen );

         prop->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex | kVstParameterSupportsDisplayCategory;
         return 1;
      }
   }

private:
   audioMasterCallback _vstHostCallback;
   AEffect _vstPlugin;
   track_t* track = nullptr;
   draw_ctx_t* ctx = nullptr;
};

VSTPluginWrapper* VSTPluginWrapper::window_to_wrapper = nullptr;


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

      add_sample_data( track, (size_t)i, inputSamples, (size_t)sampleFrames );

      for ( int j = 0; j < sampleFrames; j++ )
      {
         outputSamples[j] = inputSamples[j];
      }
   }

   process_samples( track, wrapper->reactivity );
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
      r = 1;
      break;

   case effSetProgram:
      DEBUG_PRINT( "effSetProgram\n" );
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
      r = 1;
      break;

   case effStopProcess:
      DEBUG_PRINT( "effStopProcess\n" );
      r = 1;
      break;

   case effEditIdle:
      DEBUG_PRINT( "effEditIdle\n" );
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
   auto* wrapper = (VSTPluginWrapper*) lglw_userdata_get( _lglw );
   wrapper->setMousePosition( _x, _y );

   if ( LGLW_IS_MOUSE_LBUTTON_DOWN() )
   {
      lglw_mouse_grab( _lglw, LGLW_MOUSE_GRAB_WARP );
   }
   else if ( LGLW_IS_MOUSE_LBUTTON_UP() )
   {
      lglw_mouse_ungrab( _lglw );
   }
}

static void loc_focus_cbk( lglw_t _lglw, uint32_t _focusState, uint32_t _changedFocusState )
{
   if ( _focusState == 0 )
   {
      auto* wrapper = (VSTPluginWrapper*) lglw_userdata_get( _lglw );
      wrapper->setMousePosition( 0, 0 );
   }
}

static lglw_bool_t loc_keyboard_cbk( lglw_t _lglw, uint32_t _vkey, uint32_t _kmod, lglw_bool_t _bPressed )
{
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

extern AEffect
*
VSTPluginMain( audioMasterCallback
               vstHostCallback )
{
   DEBUG_PRINT( "Debugging is enabled...\n" );

   assert( MAX_FFT > 0 );
   assert( MAX_FFT % 256 == 0 );
   assert( MAX_CHANNELS > 0 );

   DEBUG_PRINT( "Max FFT Size: %i\n", MAX_FFT );
   DEBUG_PRINT( "Max Channels: %i\n", MAX_CHANNELS );

   auto* plugin =
           new VSTPluginWrapper( vstHostCallback,
                                 CCONST( 't', 'e', 's', 't' ), // unregistered
                                 PLUGIN_VERSION,
                                 4, // params
                                 0, // programs
                                 MAX_CHANNELS,   // inputs
                                 MAX_CHANNELS ); // outputs

   return plugin->getVSTPlugin();
}
