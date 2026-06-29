#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef s32 b32;

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define true 1
#define false 0

static b32 GlobalRunning;

typedef enum
{
    Start,
    Play,
} game_state;

static game_state State = Start;

typedef struct 
{
    BITMAPINFO Info;
    void *Memory;
    u32 Width;
    u32 Height;
    u32 Pitch;
    u8 BytesPerPixel;    
} game_offscreen_buffer;

static game_offscreen_buffer GlobalBackBuffer;

typedef struct 
{
    u32 Width;
    u32 Height;    
} window_dimension;

typedef struct 
{
    s32 HalfTransitionCount;
    b32 EndedDown;    
} game_button_state;

typedef struct 
{   
    union
    {
        game_button_state Buttons[2];
        struct 
        {
          game_button_state MoveUp;
          game_button_state MoveDown;
        };
    };    
} game_controller_input;

typedef struct 
{    
    game_controller_input Controllers[1];    
} game_input;

typedef struct 
{
    f32 X;
    f32 Y;

    u8 Width;
    u8 Height;
    f32 Speed;
    f32 YVelocity;
    f32 YAcceleration;
    u8 Score;
    u32 Color;    
} paddle;

typedef struct 
{
    f32 X;
    f32 Y;

    u8 Radius;
    u32 Color;
    
    f32 XVelocity;
    f32 YVelocity;  
} ball;

inline f32
Abs(f32 A)
{
    if (A < 0)
    {
        A *= -1;
    }
    
    return A;
}

inline f32
Lerp(f32 A, f32 B, f32 C)
{
    return A + C*(B - A);
}

typedef struct
{
    f32 X;
    f32 Y;
} v2;

typedef struct
{
    v2 Min;
    v2 Max;
    u32 Color;
} wall;

typedef struct
{
    v2 Min;
    v2 Max;
    u32 Color;
} net;

typedef struct
{
    u32 Color;
} background;

typedef struct
{
    paddle LeftPlayer;
    paddle RightPlayer;
    paddle *Player1;
    paddle *Player2;
    ball Ball;
    wall Walls[2];
    net Net;
    background Background;    
} game_entities;

u16 Numbers[6][35] =
{
    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,1,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },
    
    {
        0,0,1,0,0,
        0,1,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        1,1,1,1,1,
    },
       
    {
        0,1,1,1,0,
        1,0,0,0,1,
        0,0,0,0,1,
        0,1,1,1,0,
        1,0,0,0,0,
        1,0,0,0,0,
        1,1,1,1,1,
    },
       
    {
        0,1,1,1,0,
        1,0,0,0,1,
        0,0,0,0,1,
        0,0,1,1,0,
        0,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

    {
        0,0,1,1,1,
        0,1,0,0,1,
        1,0,0,0,1,
        1,1,1,1,1,
        0,0,0,0,1,
        0,0,0,0,1,
        0,0,0,0,1,
    },

    {
        1,1,1,1,1,
        1,0,0,0,0,
        1,0,0,0,0,
        1,1,1,1,0,
        0,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

};

u16 Letters[16][35] =
{
    {
        1,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,1,1,1,0,
        1,0,0,0,0,
        1,0,0,0,0,
        1,0,0,0,0,
    },
       
    {
        1,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,1,1,1,0,
        1,0,1,0,0,
        1,0,0,1,0,
        1,0,0,0,1,
    },
       
    {
        1,1,1,1,1,
        1,0,0,0,0,
        1,0,0,0,0,
        1,1,1,1,0,
        1,0,0,0,0,
        1,0,0,0,0,
        1,1,1,1,1,
    },

    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,0,
        0,1,1,1,0,
        0,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,0,
        0,1,1,1,0,
        0,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

    {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
    },

    {
        1,0,0,0,1,
        1,0,0,0,1,
        0,1,0,1,0,
        0,0,1,0,0,
        0,1,0,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
    },

    {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
    },

    {
        1,1,1,1,1,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
    },

    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

    {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
    },

    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,0,
        0,1,1,1,0,
        0,0,0,0,1,
        1,0,0,0,1,
        0,1,1,1,0,
    },

    {
        1,1,1,1,1,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
    },

    {
        0,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,1,1,1,1,
        1,0,0,0,1,
        1,0,0,0,1,
        1,0,0,0,1,
    },

    {
        1,1,1,1,0,
        1,0,0,0,1,
        1,0,0,0,1,
        1,1,1,1,0,
        1,0,1,0,0,
        1,0,0,1,0,
        1,0,0,0,1,
    },

    {
        1,1,1,1,1,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
        0,0,1,0,0,
    },

};


    
