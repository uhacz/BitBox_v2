#pragma once

#include <foundation/type.h>

struct BXInput
{
    // --- 
    struct PadState
    {
        enum
        {
            // uni
            eUP = 1 << ( 1 ),
            eDOWN = 1 << ( 2 ),
            eLEFT = 1 << ( 3 ),
            eRIGHT = 1 << ( 4 ),

            // ps
            eCROSS = 1 << ( 5 ),
            eSQUARE = 1 << ( 6 ),
            eTRIANGLE = 1 << ( 7 ),
            eCIRCLE = 1 << ( 8 ),
            eL1 = 1 << ( 9 ),
            eL2 = 1 << ( 10 ),
            eL3 = 1 << ( 11 ),
            eR1 = 1 << ( 12 ),
            eR2 = 1 << ( 13 ),
            eR3 = 1 << ( 14 ),
            eSELECT = 1 << ( 15 ),
            eSTART = 1 << ( 16 ),

            // xbox
            eA = eCROSS,
            eB = eCIRCLE,
            eX = eSQUARE,
            eY = eTRIANGLE,
            eLSHOULDER = eL1,
            eLTRIGGER = eL2,
            eLTHUMB = eL3,
            eRSHOULDER = eR1,
            eRTRIGGER = eR2,
            eRTHUMB = eR3,
            eBACK = eSELECT,
        };

        struct
        {
            f32 left_X = 0;
            f32 left_Y = 0;
            f32 right_X = 0;
            f32 right_Y = 0;

            f32 L2 = 0;
            f32 R2 = 0;
        } analog;

        struct
        {
            u32 buttons = 0;
        } digital;

        u32 connected = 0;
    };

    // --- 
    struct KeyboardState
    {
        u8 keys[256] = {};
    };

    // --- 
    struct MouseState
    {
        u16 x = 0;
        u16 y = 0;
        i16 dx = 0;
        i16 dy = 0;

        u8 lbutton = 0;
        u8 mbutton = 0;
        u8 rbutton = 0;
        i8 wheel = 0;
    };

    /////////////////////////////////////////////////////////////////
    struct Pad
    {
        PadState state[2];
        u32 current_state_index = 0;

              PadState* CurrentState ()       { return &state[current_state_index]; }
        const PadState& CurrentState () const { return state[current_state_index]; }
        const PadState& PreviousState() const { return state[!current_state_index]; }
    };

    struct Keyboard
    {
        KeyboardState state[2];
        u32 current_state_index = 0;

              KeyboardState* CurrentState ()       { return &state[current_state_index]; }
        const KeyboardState& CurrentState () const { return state[current_state_index]; }
        const KeyboardState& PreviousState() const { return state[!current_state_index]; }
    };


    struct Mouse
    {
        MouseState prevState;
        MouseState currState;

              MouseState* CurrentState ()       { return &currState; }
        const MouseState& CurrentState () const { return currState; }
        const MouseState& PreviousState() const { return prevState; }
    };

    enum
    {
        eKEY_ESC      = 27,
        eKEY_LEFT     = 37,
        eKEY_UP       = 38,
        eKEY_RIGHT    = 39,
        eKEY_DOWN     = 40,
        eKEY_LSHIFT   = 16,
        eKEY_CAPSLOCK = 0x14,
    };

    Keyboard kbd;
    Mouse mouse;
    Pad pad;

    static void Swap( Pad* pad )      { pad->current_state_index = !pad->current_state_index; }
    static void Swap( Keyboard* kbd ) { kbd->current_state_index = !kbd->current_state_index; }
    static void Swap( Mouse* mouse )  { mouse->prevState = mouse->currState; }
    static void ComputeMouseDelta( Mouse* mouse );
    //////////////////////////////////////////////////////////////////////////
    inline bool IsKeyPressed( unsigned char key ) const 
    {
        return ( kbd.CurrentState().keys[key] ) != 0;
    }
    inline bool IsKeyPressedOnce( unsigned char key )
    {
        const unsigned char curr_state = kbd.CurrentState()->keys[key];
        const unsigned char prev_state = kbd.PreviousState().keys[key];

        return !prev_state && curr_state;
    }
    
    inline const Pad& GetPadState() const
    {
        return pad;
    }
    inline bool IsPadButtonPressedOnce( u16 button_mask ) const
    {
        const PadState& prev_state = pad.PreviousState();
        const PadState& curr_state = pad.CurrentState();

        const u16 prev = prev_state.digital.buttons & button_mask;
        const u16 curr = curr_state.digital.buttons & button_mask;

        return !prev && curr;
    }

    inline bool IsPadButtonPressed( u16 button_mask ) const
    {
        const PadState& curr_state = pad.CurrentState();
        const u16 curr = curr_state.digital.buttons & button_mask;
        return curr != 0;
    }
};

/////////////////////////////////////////////////////////////////
void InputClear( BXInput* input, bool keys, bool mouse, bool pad );
void InputSwap( BXInput* input );

void InputUpdatePad_XInput( BXInput::Pad* pads, u32 n );
