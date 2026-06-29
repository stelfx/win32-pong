#include <windows.h>
#include "pong.h"

static window_dimension
GetWindowDimension(HWND Window)
{
    
    window_dimension Result;    
    RECT ClientRect;

    if (GetClientRect(Window, &ClientRect))
    {
        Result.Width = ClientRect.right - ClientRect.left;
        Result.Height = ClientRect.bottom - ClientRect.top;
    }
   
    return Result;
}

static void
ResizeDIBSection(game_offscreen_buffer *Buffer, u32 Width, u32  Height) 
{
    
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
   
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
  
    s32 BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*Buffer->BytesPerPixel;
}


static void
Win32DisplayBufferToWindow(HDC DeviceContext, game_offscreen_buffer *Buffer, u32 WindowWidth, u32 WindowHeight)
{    
    StretchDIBits(DeviceContext,
                  0, 0, Buffer->Width, Buffer->Height,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

inline
void DisableMaximizeButton(HWND Window)
{
    SetWindowLong(Window, GWL_STYLE,
                  GetWindowLong(Window, GWL_STYLE) & ~WS_MAXIMIZEBOX);
}

inline
b32 WasPressed(game_button_state State)
{
    b32 Result = ((State.HalfTransitionCount > 1) ||
                  ((State.HalfTransitionCount == 1) && (State.EndedDown)));

    return Result;
}

static void
InitGame(game_offscreen_buffer *Buffer, game_entities *Entities)
{

    paddle *LeftPlayer = &Entities->LeftPlayer;
    paddle *RightPlayer = &Entities->RightPlayer;
    ball *Ball = &Entities->Ball;
    

    Entities->Background.Color = 0x00609050;
    Entities->Player1 = LeftPlayer;
    Entities->Player2 = RightPlayer;   

    Entities->Walls[0].Min.X = 0;
    Entities->Walls[0].Min.Y = 0;
    Entities->Walls[0].Max.X = GlobalBackBuffer.Width;
    Entities->Walls[0].Max.Y = 10;
    Entities->Walls[0].Color = 0xFF5C5580;

    Entities->Walls[1].Min.X = 0;
    Entities->Walls[1].Min.Y = GlobalBackBuffer.Height - 10;
    Entities->Walls[1].Max.X = GlobalBackBuffer.Width;
    Entities->Walls[1].Max.Y = GlobalBackBuffer.Height;
    Entities->Walls[1].Color = 0xFF5C5580;

    Entities->Net.Min.X = GlobalBackBuffer.Width/2 - 1.5f;
    Entities->Net.Min.Y = 0;
    Entities->Net.Max.X = GlobalBackBuffer.Width/2 + 1.5f;
    Entities->Net.Max.Y = GlobalBackBuffer.Height - 22;
    Entities->Net.Color = 0xDDDDDDDD;
    
    LeftPlayer->X = 40;
    LeftPlayer->Y = Buffer->Height / 2;
    LeftPlayer->Width = 11;
    LeftPlayer->Height = 100;
    LeftPlayer->YVelocity = 0.0f;
    LeftPlayer->YAcceleration = 0.0f;
    LeftPlayer->Score = 0;
    LeftPlayer->Color = 0xFF905050;

    RightPlayer->X = Buffer->Width - 40;
    RightPlayer->Y = Buffer->Height / 2;
    RightPlayer->Width = 11;
    RightPlayer->Height = 100;
    RightPlayer->YVelocity = 0.0f;
    RightPlayer->YAcceleration = 0.0f;
    RightPlayer->Score = 0;
    RightPlayer->Color = 0xFF2070A0;


    Ball->X = Buffer->Width / 2;
    Ball->Y = Buffer->Height / 2;
    Ball->Radius = 8;
    Ball->Color = 0xFFFFFFFF;
    Ball->XVelocity = ((rand() % 2 == 0) ? 4.0f : -4.0f);
    Ball->YVelocity = 0.0f;
    
}

static void
DrawWall(game_offscreen_buffer *Buffer, wall *Wall)
{
    u8 *Row;
    u32 *Pixel;
    
    for (s32 Y = Wall->Min.Y; Y < Wall->Max.Y; Y++)
    {
        for (s32 X = Wall->Min.X; X < Wall->Max.X; X++)
        {
            Row = ((u8 *)Buffer->Memory + (s32)(X)*Buffer->BytesPerPixel + (s32)(Y)*Buffer->Pitch);
            Pixel = (u32 *)Row;
            if (X % 5 == 0)
            {
                *Pixel++ = 0x40404080;
            }
            else
            {
                *Pixel++ = Wall->Color;
            }
 
        }
        Row += Buffer->Pitch;
    }

}

static void
DrawNet(game_offscreen_buffer *Buffer, net *Net)
{
    
    u8 *Row;
    u32 *Pixel;
    
    for (s32 Y = Net->Min.Y; Y < Net->Max.Y; Y++)
    {
        for (s32 X = Net->Min.X; X <= Net->Max.X; X++)
        {
            if (Y % 17 != 0)
            {
                Row = ((u8 *)Buffer->Memory + (s32)(X)*Buffer->BytesPerPixel + (s32)(Y)*Buffer->Pitch);
                Pixel = (u32 *)Row;
                *Pixel++ = Net->Color;
            }
            else
            {
                Y += 12;
            }
 
        }
        Row += Buffer->Pitch;
    }
}

static void
DrawBall(game_offscreen_buffer *Buffer, ball *Ball, u32 Color)
{
    
    u32 *EndOfBuffer = ((u32 *)Buffer->Memory + Buffer->Pitch*Buffer->Height);

    s32 StartX = Ball->X - Ball->Radius;
    s32 StartY = Ball->Y - Ball->Radius;
    
    u8 *Row;
    u32 *Pixel;

    for (s32 Y = -Ball->Radius;
         Y < Ball->Radius;
         Y++)
    {
        
        for (s32 X = -Ball->Radius;
             X < Ball->Radius;
             X++)
        {
            Row = ((u8 *)Buffer->Memory + (s32)(X+Ball->X)*Buffer->BytesPerPixel + (s32)(Y+Ball->Y)*Buffer->Pitch);
            Pixel = (u32 *)Row;

            if ((X*X + Y*Y) < Ball->Radius*Ball->Radius &&
                X + Ball->X > 0 &&
                X + Ball->X < Buffer->Width)
            {
                if (X % 3 == 0 && Y % 3 == 0)
                {
                    *Pixel++ = 0xFFAAAAAA;
                }
                else
                {
                    *Pixel++ = Color;
                }
            }
        }
    }
}    

static void
DrawBackground(game_offscreen_buffer *Buffer, game_entities *Entities)
{
    
    u8 *Row = (u8 *)Buffer->Memory;
    
    for (s32 Y = 0; Y < Buffer->Height; Y++)
    {
        u32 *Pixel = (u32 *)Row;
        for (s32 X = 0; X < Buffer->Width; X++)
        {
            if (X % 20 == 0 && X != 0 && Y % 30 == 0 && X != Buffer->Width/2)
            {
                *Pixel++ = 0xFFFFFFFF;   
            }
            else
            {
                *Pixel++ = Entities->Background.Color;   
            }
             
        }
        Row += Buffer->Pitch;
    }

    DrawWall(Buffer, &Entities->Walls[0]);
    DrawWall(Buffer, &Entities->Walls[1]);
    DrawNet(Buffer, &Entities->Net);
}

static void
DisplayText(game_offscreen_buffer *Buffer, s16 X, s16 Y, u32 Color, f32 Size)
{
    
    f32 StartX = X;
    f32 StartY = Y;

    u8 *Row;
    u32 *Pixel;
    
    for (s32 R = 0; R < ArrayCount(Letters); R++)
    {
        for (s32 C = 0; C < ArrayCount(Letters[R]); C++)
        {
            if (Letters[R][C] == 1)
            {
                for (s32 Y = StartY; Y < StartY + Size; ++Y)
                {
                    for (s32 X = StartX; X < StartX + Size; ++X)
                    {
                        Row = ((u8 *)Buffer->Memory + X*Buffer->BytesPerPixel + Y*Buffer->Pitch);
                        Pixel = (u32 *)Row;
                        *Pixel++ = Color;                        
                    }                
                }
            }
        
            StartX += Size;
            if ((C + 1)%5 == 0)
            {
                StartY += Size;
                StartX -= Size * 5;
            }
        }
        StartY -= Size*7;
        StartX += Size*7;
    }    
}


static void
DisplayScore(game_offscreen_buffer *Buffer, paddle *Player, s16 X, s16 Y, s16 Size)
{
    
    s32 StartX = X - (Size * 5)/2;
    s32 StartY = Y - (Size * 5)/2;

    u8 *Row;
    u32 *Pixel;
    
    for (s32 C = 0; C < ArrayCount(Numbers[Player->Score]); C++)
    {
        if (Numbers[Player->Score][C] == 1)
        {
            for (s32 Y = StartY; Y < StartY + Size; Y++)
            {
                for (s32 X = StartX; X < StartX + Size; X++)
                {
                    Row = ((u8 *)Buffer->Memory + X*Buffer->BytesPerPixel + Y*Buffer->Pitch);
                    Pixel = (u32 *)Row;
                    *Pixel++ = Player->Color;
                }
            }
        }

        StartX += Size;
        if ((C + 1)%5 == 0)
        {
            StartY += Size;
            StartX -= Size * 5;
        }

    }
    
}

static void
DrawPlayer(game_offscreen_buffer *Buffer, paddle *Player)
{
    
    u8 *EndOfBuffer = ((u8 *)Buffer->Memory + Buffer->Pitch*Buffer->Height);

    s32 StartX = Player->X - Player->Width/2;
    s32 StartY = Player->Y - Player->Height/2;
    
    u8 *Row;
    u32 *Pixel;
       

    for (s32 Y = 0;
         Y < Player->Height;
         Y++)
    {
        
        for (s32 X = 0;
             X < Player->Width;
             X++)
        {
            Row = ((u8 *)Buffer->Memory + (StartX+X)*Buffer->BytesPerPixel + (StartY+Y)*Buffer->Pitch);
            Pixel = (u32 *)Row;

            if (X == 0 || X == Player->Width - 1 || Y == 0 || Y == Player->Height - 1)
            {
                *Pixel++ = 0x30303030;
            }
            else
            {
                *Pixel++ = Player->Color;
            }
        }

        Row += Buffer->Pitch;
        
    }

    if (State == Play)
    {
        f32 NumX = Lerp(Player->X, Buffer->Width/2, 0.8);
        DisplayScore(Buffer, Player, NumX, 45, 8);
    }
}


static void
CalculateHitPosition(ball *Ball, paddle *Player)
{
    s32 HitPos = (Player->Y + Player->Height/2) - Ball->Y;
    f32 PYVelocity = Abs(Player->YVelocity);

    if (HitPos < 10)
    {
        Ball->YVelocity += 1.0f + (PYVelocity*0.2f);
        if (Ball->YVelocity < 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 10 && HitPos < 20)
    {
        Ball->YVelocity += 0.8f + (PYVelocity*0.2f);
        if (Ball->YVelocity < 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 20 && HitPos < 30)
    {
        Ball->YVelocity += 0.5f + (PYVelocity*0.2f);
        if (Ball->YVelocity < 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 30 && HitPos < 40)
    {
        Ball->YVelocity += 0.3f + (PYVelocity*0.2f);
        if (Ball->YVelocity < 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 40 && HitPos < 48)
    {
        Ball->YVelocity += 0.2f + (PYVelocity*0.2f);
    }
    else if (HitPos >= 48 && HitPos < 52)
    {
        Ball->YVelocity += (rand() % 2) ? 0.1f : -0.1f;
    }
    else if (HitPos >= 52 && HitPos < 60)
    {
        Ball->YVelocity += -0.2f - (PYVelocity*0.2f);
    }
    else if (HitPos >= 60 && HitPos < 70)
    {
        Ball->YVelocity += -0.3f - (PYVelocity*0.2f);
        if (Ball->YVelocity > 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 70 && HitPos < 80)
    {
        Ball->YVelocity += -0.5f - (PYVelocity*0.2f);
        if (Ball->YVelocity > 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 80 && HitPos < 90)
    {
        Ball->YVelocity += -0.8f - (PYVelocity*0.2f);
        if (Ball->YVelocity > 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }
    else if (HitPos >= 90)
    {
        Ball->YVelocity += -1.0f - (PYVelocity*0.2f);
        if (Ball->YVelocity > 0.0f)
        {
            Ball->YVelocity = -Ball->YVelocity;
        }
    }

    if (Ball->YVelocity > 5.0f)
    {
        Ball->YVelocity = 5.0f;
    }
    else if (Ball->YVelocity < -5.0f)
    {
        Ball->YVelocity = -5.0f;
    }

}

static void
ChangeBallVelocity(ball *Ball, paddle *Player)
{
    
    if (Ball->XVelocity < 0.0f)
    {
        Ball->XVelocity -= 1.5f;
    }
    else
    {
        Ball->XVelocity += 1.5f;
    }

    Ball->XVelocity = -Ball->XVelocity;
    
    if (Ball->XVelocity > 20.0f)
    {
        Ball->XVelocity = 20.0f;
    }

    CalculateHitPosition(Ball, Player);

}

static b32
CheckPlayerWallCollision(game_offscreen_buffer *Buffer, paddle *Player)
{

    b32 Collision = false;
    if (Player->Y - 60 < 0)
    {
        Player->Y = Player->Height/2 + 10;
        Collision = true;
    }
    else if (Player->Y + 60 > Buffer->Height)
    {
        Player->Y = Buffer->Height-(Player->Height/2 + 10);
        Collision = true;
    }

    return Collision;
}

static b32
CheckPlayerBallCollision(ball *Ball, paddle *Player)
{

    f32 BallLeft = Ball->X - Ball->Radius;
    f32 BallRight = Ball->X + Ball->Radius;
    f32 BallTop = Ball->Y - Ball->Radius;
    f32 BallBottom = Ball->Y + Ball->Radius;

    f32 PlayerLeft = Player->X - Player->Width/2;
    f32 PlayerRight = Player->X + Player->Width/2;
    f32 PlayerTop = Player->Y - Player->Height/2;
    f32 PlayerBottom = Player->Y + Player->Height/2;
    
    if (BallLeft >= PlayerRight)
    {
        return false;
    }

    if (BallRight <= PlayerLeft)
    {
        return false;
    }

    if (BallTop >= PlayerBottom)
    {
        return false;
    }

    if (BallBottom <= PlayerTop)
    {
        return false;
    }
        
    return true;
    
}

static void
UpdateBall(game_offscreen_buffer *Buffer, ball *Ball,  paddle *LeftPlayer, paddle *RightPlayer)
{    
    
    Ball->X += Ball->XVelocity;
    Ball->Y += Ball->YVelocity;

    if ((Ball->Y - Ball->Radius - 10) < 0)
    {
        Ball->YVelocity = -Ball->YVelocity;
        Ball->YVelocity += 0.2f;
        Ball->Y = Ball->Radius + 10;
    }
    else if ((Ball->Y + Ball->Radius + 10) > Buffer->Height)
    {
        Ball->YVelocity = -Ball->YVelocity;
        Ball->YVelocity -= 0.2f;
        Ball->Y = Buffer->Height - Ball->Radius - 10;
    }

    
    if (Ball->X < -Ball->Radius)
    {
            if (State == Play)
            {
                RightPlayer->Score++;
            }
            Ball->YVelocity = 0.0f;
            Ball->XVelocity = 4.0f;
            Ball->X = Buffer->Width/2;
            Ball->Y = Buffer->Height/2;
    }
    else if (Ball->X > Buffer->Width + Ball->Radius)
    {
            if (State == Play)
            {
                LeftPlayer->Score++;
            }
            Ball->YVelocity = 0.0f;
            Ball->XVelocity = -4.0f;
            Ball->X = Buffer->Width/2;
            Ball->Y = Buffer->Height/2;
    }


    if (CheckPlayerBallCollision(Ball, LeftPlayer))
    {
        ChangeBallVelocity(Ball, LeftPlayer);
        Ball->X = LeftPlayer->X + LeftPlayer->Width/2 + Ball->Radius;        
    }
    else if (CheckPlayerBallCollision(Ball, RightPlayer))
    {
        ChangeBallVelocity(Ball, RightPlayer);        
        Ball->X = RightPlayer->X - RightPlayer->Width/2 - Ball->Radius;        
    }

}


static void
UpdateAI( game_offscreen_buffer *Buffer, paddle *AI, ball *Ball)
{
    u32 BallXPos = Ball->X;
    u32 BallYPos = Ball->Y;
    
    f32 BallYVelocity = Ball->YVelocity;
    f32 BallXVelocity = Ball->XVelocity;
    u8 R1 = ((u32)Abs(BallXPos+BallYVelocity)%4) + 2;
    u8 R2 = ((u32)Abs(BallYPos+BallXVelocity)%4) + 2;

    if (AI->X > Buffer->Width/2)
    {
        if (BallXVelocity > 0.0f && !CheckPlayerWallCollision(Buffer, AI))
        {
            if ((AI->Y - AI->Height/R1 > Ball->Y) && (AI->Y - AI->Height/R2 > Ball->Y))
            {
                AI->Y -= 4 + Abs(BallYVelocity);
            }
            else if ((AI->Y + AI->Height/R1 < Ball->Y) && (AI->Y + AI->Height/R2 < Ball->Y))
            {
                AI->Y += 4 + Abs(BallYVelocity);
            }

            CheckPlayerWallCollision(Buffer, AI);
        }        
    }
    else
    {
        if (BallXVelocity < 0.0f && !CheckPlayerWallCollision(Buffer, AI))
        {

            if ((AI->Y - AI->Height/R1 > Ball->Y) && (AI->Y - AI->Height/R2 > Ball->Y))
            {
                AI->Y -= 4 + Abs(BallYVelocity);
            }
            else if ((AI->Y + AI->Height/R1 < Ball->Y) && (AI->Y + AI->Height/R2 < Ball->Y))
            {
                AI->Y += 4 + Abs(BallYVelocity);
            }

            CheckPlayerWallCollision(Buffer, AI);
        }
    }
}
   

static void
GameUpdateAndRender(game_offscreen_buffer *Buffer, game_input *Input, game_entities *Entities)
{
    paddle *LeftPlayer = &Entities->LeftPlayer;
    paddle *RightPlayer = &Entities->RightPlayer;
    paddle *Player1 = Entities->Player1;
    paddle *Player2 = Entities->Player2;
    ball *Ball = &Entities->Ball;    
    game_controller_input *Controller = &Input->Controllers[0];    

        
    DrawBackground(Buffer, Entities);
    
    if (State == Play)
    {
        
        Player1->YAcceleration = 0.0f;
        if (Controller->MoveUp.EndedDown)
        {
            
            Player1->YAcceleration = -1.0f * 0.787186781187f;   
        }
        else if (Controller->MoveDown.EndedDown)
        {

            Player1->YAcceleration = 1.0f * 0.787186781187f;    
        }
        else
        {
            if (Player1->YVelocity < 0)
            {
                Player1->YVelocity += 0.1f;

                if (Player1->YVelocity > 0)
                {
                    Player1->YVelocity = 0;
                }
            }
            else if (Player1->YVelocity > 0)
            {
                Player1->YVelocity -= 0.1f;

                if (Player1->YVelocity < 0)
                {
                    Player1->YVelocity = 0;
                }
            }
        }

        Player1->YVelocity += Player1->YAcceleration*0.5f;
        Player1->Y += Player1->YVelocity*0.5f;

                
        if (CheckPlayerWallCollision(Buffer, Player1))
        {
            Player1->YVelocity = Player1->YVelocity - 1.5f*(Player1->YVelocity * 1.0f)*1.0f;
        }
    }
    else if (State == Start)
    {
        UpdateAI(Buffer, RightPlayer, Ball);
        UpdateAI(Buffer, LeftPlayer, Ball);
        DisplayText(Buffer, 140, 280, 0xEEEEEEEE, 5);
    }   

    UpdateBall(Buffer, Ball, LeftPlayer, RightPlayer);
    UpdateAI(Buffer, Player2, Ball);
    
    DrawPlayer(Buffer, Player1);
    DrawPlayer(Buffer, Player2);
    DrawBall(Buffer, Ball, Ball->Color);
}



static void
ProcessKeyboardMessage(game_button_state *NewState, b32 IsDown)
{
    if (NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}


static void
Win32ProcessPendingMessages(game_controller_input *KeyboardController, game_offscreen_buffer *Buffer, game_entities *Entities)
{
    
    MSG Message;
    while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        if (Message.message == WM_QUIT)
        {
            GlobalRunning = 0;
        }
        switch (Message.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                                
                u32 VKCode = (u32)Message.wParam;
                b32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                b32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if (WasDown != IsDown)
                {
                    if (VKCode == 'W' || VKCode == VK_UP)
                    {
                        ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'S' || VKCode == VK_DOWN)
                    {
                        ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'X')
                    {
                        if (State == Start)
                        {
                            if (GetKeyState(VKCode) & 0x8000)
                            {
                                InitGame(Buffer, Entities);
                                State = Play;
                            }
                        }

                    }
                    else if (VKCode == 'R')
                    {
                        if (State == Play)
                        {
                            if (GetKeyState(VKCode) & 0x8000)
                            {                        
                                InitGame(Buffer, Entities);
                            }
                        }
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        if (GetKeyState(VKCode) & 0x8000)
                        {  
                            if (State == Play)
                            {
                                State = Start;
                            }
                            else
                            {
                                GlobalRunning = false;
                            }
                        }
                    }                                                        
                }
            
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
    }
}


LRESULT CALLBACK
MainWindowCallBack(
    HWND   Window,
    UINT   Message,
    WPARAM WParam,
    LPARAM LParam
                   )
{
    
    LRESULT Result = 0;
    
    switch (Message)
    {
        
        case WM_DESTROY:
        {
            GlobalRunning = false;
            
        } break;
        
        case WM_CLOSE:
        {
            GlobalRunning = false;
            
        } break;

        case WM_ACTIVATEAPP:
        {

        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            s16 X = Paint.rcPaint.left;
            s16 Y = Paint.rcPaint.top;
            u16 Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            u16 Width = Paint.rcPaint.right - Paint.rcPaint.left;
         
            window_dimension Dimension = GetWindowDimension(Window);
                        
            Win32DisplayBufferToWindow(DeviceContext, &GlobalBackBuffer, Dimension.Width, Dimension.Height);
            
            EndPaint(Window, &Paint);
            
        } break;

        
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
            
        } break;
    }
    
    return Result;
}


static inline s64
GetTicks()
{
    LARGE_INTEGER Ticks;
    QueryPerformanceCounter(&Ticks);

    return Ticks.QuadPart;
}

static inline s64
GetPerformanceFrequency()
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    return Frequency.QuadPart;
}

int CALLBACK
WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR     CommandLine,
    int       ShowCode)
{
    ResizeDIBSection(&GlobalBackBuffer, 800, 600);

    WNDCLASSA WindowClass = {0};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = MainWindowCallBack;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = "Handmade Pong";
    WindowClass.hIcon = (HICON)LoadImage(NULL, "pong.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE|LR_DEFAULTSIZE);
    
    RECT Rect;
    Rect.top = 0;
    Rect.left = 0;
    Rect.right = GlobalBackBuffer.Width;
    Rect.bottom = GlobalBackBuffer.Height;
    
    AdjustWindowRectEx(&Rect, WS_CAPTION, FALSE, 0);

    Rect.right -= Rect.left;
    Rect.bottom -= Rect.top;

    if (RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx(
            0,
            WindowClass.lpszClassName,
            "Handmade Pong",
            WS_OVERLAPPED|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            Rect.right,
            Rect.bottom,
            0,
            0,
            Instance,
            0);
        
        if (Window)
        {
            DisableMaximizeButton(Window);
            s32 MonitorRefreshRate = 60;
            HDC RefreshDC = GetDC(Window);
            s32 Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 1)
            {
                MonitorRefreshRate = Win32RefreshRate;
            }
            f32 TargetRefreshRate = 1.0f / MonitorRefreshRate;
            
            game_input Input[2] = {0};
            game_input *NewInput = &Input[0];
            game_input *OldInput = &Input[1];
            game_entities Entities = {0};
            
            srand(time(0));
            InitGame(&GlobalBackBuffer, &Entities);
            
            f64 StartTime, StopTime, DeltaTime, Frequency = 0;
            f64 FrameAccumulator = 0;

            Frequency = GetPerformanceFrequency();

            GlobalRunning = true;
            StartTime = GetTicks();

            while (GlobalRunning)
            {
                StopTime = GetTicks();
                DeltaTime = (StopTime - StartTime)/Frequency;
                StartTime = StopTime;
                
                if (Abs(DeltaTime - TargetRefreshRate) < 0.0002f)
                {
                    DeltaTime = TargetRefreshRate;
                }                

                
                if ((Entities.Player1->Score == 5) || (Entities.Player2->Score == 5))
                {
                    State = Start;
                    Entities.Player1->Score = 0;
                    Entities.Player2->Score = 0;
                }
                
                game_controller_input *NewKeyboardController = &OldInput->Controllers[0];
                game_controller_input *OldKeyboardController = &NewInput->Controllers[0];
                game_controller_input ZeroController = {0};
                *NewKeyboardController = ZeroController;

                for (int ButtonIndex = 0;
                     ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                     ++ButtonIndex)
                {
                    NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                        OldKeyboardController->Buttons[ButtonIndex].EndedDown;

                }

                Win32ProcessPendingMessages(NewKeyboardController, &GlobalBackBuffer, &Entities);
                
                FrameAccumulator += DeltaTime;
                while (FrameAccumulator >= TargetRefreshRate)
                {
                    GameUpdateAndRender(&GlobalBackBuffer, NewInput, &Entities);
                    FrameAccumulator -= TargetRefreshRate;
                }
               
                window_dimension Dimension = GetWindowDimension(Window);
                
                HDC DeviceContext = GetDC(Window);
                Win32DisplayBufferToWindow(DeviceContext, &GlobalBackBuffer, Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);
                
                game_input *Temp = NewInput;
                NewInput = OldInput;
                OldInput = Temp;

               
            }               
        }
    }
        
    return 0;
};

