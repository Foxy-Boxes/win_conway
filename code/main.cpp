/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Berk Aydogmus $
   $Notice: (C) Copyright 2021 by SuckGames, Inc. All Rights Reserved. $
   ======================================================================== */
#include <stdio.h>
#include <stdint.h>
#include <windows.h>


#define internal static
#define local_persist static
#define global_variable static

enum state_enum{
    EDIT = 1,
    ITERATION = 2,
    PAUSED = 4
};

enum pallette{
    GRAY = 120 << 16 | 120 << 8 | 120 ,
    BLACKISH =  20 << 16 | 20 << 8 | 20,
    WHITEISH =  200 << 16 | 200 << 8 | 200 
};

struct bitmap_buffer{
    void* buffer;
    int width;
    int height;
};

struct field{
    int x_size;
    int y_size;
    void* buffer;
};

struct window_proportions{
    int width;
    int height;
};

global_variable bool running;
global_variable state_enum state; 
global_variable bitmap_buffer bitmap_memory;
global_variable field conway_field;
global_variable field swap_field;
global_variable field edit_base_field;
global_variable BITMAPINFO bitmap_info;
constexpr uint8_t byte_per_pixel = 4;

inline void allocate_field(int x, int y, field* f){
    f->x_size = x;
    f->y_size = y;
    f->buffer = VirtualAlloc(NULL, ((x*y) >> 3) + 1,  MEM_COMMIT, PAGE_READWRITE);
}
inline void free_field(field *f){
    VirtualFree(f->buffer,0,MEM_RELEASE);
}
inline void zero_field(field *f){
    ZeroMemory(f->buffer,((f->x_size*f->y_size) >> 3)+1);
}
void equate_field(field* fdst, field* fsrc){
    if(fdst->x_size != fsrc->x_size || fdst->y_size != fsrc->y_size){
        if(fdst->buffer)
            VirtualFree(fdst->buffer,0,MEM_RELEASE);
        allocate_field(fsrc->x_size,fsrc->y_size,fdst);
    }
}
bool get(int x, int y, field f){
    int which_byte = y*f.x_size+x;
    int which_bit  = which_byte & 7;
    which_byte = which_byte >> 3;
    return ((uint8_t*)f.buffer)[which_byte] & (1 << which_bit);
}

void set(int x, int y, field f, bool value){
    int which_byte = y*f.x_size+x;
    int which_bit  = which_byte & 7;
    which_byte = which_byte >> 3;
    value && (((uint8_t*)f.buffer)[which_byte] |= (1 << which_bit));
    value || (((uint8_t*)f.buffer)[which_byte] &= ~(1 << which_bit));
}
void toggle(int x, int y, field f){
    int which_byte = y*f.x_size+x;
    int which_bit  = which_byte & 7;
    which_byte = which_byte >> 3;
    (((uint8_t*)f.buffer)[which_byte] ^= (1 << which_bit));

}
int neighbours(int x, int y, field f){
    int row_size = f.x_size;
    int col_size = f.y_size;
    int up_row = 0,row = 0,down_row = 0;
    y && (up_row = (int) (x+1 < row_size && get(x+1,y-1,f)) + (int) get(x, y-1, f) + (int) (x && get(x-1 , y-1,f)));
    row = (int) (x+1 < row_size && get(x+1,y,f)) + (int) (x && get(x-1,y,f));
    (y+1 < col_size) && (down_row =  (int) (x+1 < row_size && get(x+1,y+1,f)) + (int) get(x, y+1, f) + (int) (x && get(x-1 , y+1,f)));
    return up_row + row + down_row;
}

void ResizeBitmap(int width, int height){


    if(width != bitmap_memory.width || height != bitmap_memory.height){
        
        void * old_bitmap = bitmap_memory.buffer;

        bitmap_info.bmiHeader.biWidth  = width;
        bitmap_info.bmiHeader.biHeight = -height;

        bitmap_memory.width = width;
        bitmap_memory.height = height;
        
        int bitmap_size = width*height*byte_per_pixel;
        bitmap_memory.buffer = VirtualAlloc(0,bitmap_size,MEM_COMMIT,PAGE_READWRITE);
        if(old_bitmap)
            VirtualFree(old_bitmap,0,MEM_RELEASE);
    }
}
void ResizeGameField(int width, int height){
    if(height != conway_field.y_size || width != conway_field.x_size){
        field old_field = conway_field;
        // if old field has more area than base field make it the new base
        if(old_field.x_size*old_field.y_size > edit_base_field.x_size*edit_base_field.y_size){
            free_field(&edit_base_field);
            edit_base_field = old_field;
        } else{
            free_field(&old_field);
        }
        allocate_field(width,height,&conway_field);
        zero_field(&conway_field);


        //refill with base
        int byte_x;
        int remainder_x;
        if(width > edit_base_field.x_size){
            byte_x = edit_base_field.x_size >> 3;
            remainder_x = edit_base_field.x_size & 7;
        }else{
            byte_x = width >> 3;
            remainder_x = width & 7;
        }
        int row = min(edit_base_field.y_size,height);
        for(int i = 0; i < row; i++){
            memcpy(((uint8_t*)conway_field.buffer) + i*byte_x,((uint8_t*)edit_base_field.buffer) + i*byte_x, byte_x);
            for(int j = 0; j < remainder_x; j++){
                set((byte_x<<3)+j,i,conway_field,get((byte_x<<3)+j,i,edit_base_field));
            }
        }
       
    }
}

window_proportions GetWidthAndHeight(HWND window){
    RECT rect;
    GetClientRect(window,&rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    return {width,height};

}
void Win32DisplayBuffer(HDC device_context, HWND window){
    window_proportions proportions;
    proportions = GetWidthAndHeight(window);
    StretchDIBits(device_context,
                  0,0,proportions.width,proportions.height,
                  0,0,bitmap_memory.width,bitmap_memory.height,
                  bitmap_memory.buffer,&bitmap_info,DIB_RGB_COLORS, SRCCOPY);
}

void ApplyFieldToBitmap(field f){
    int x = bitmap_memory.width >> 2;
    int y = bitmap_memory.height,
        remainder_x = bitmap_memory.width & 3;
    int i,j,pitch;
    uint8_t* row = (uint8_t*)bitmap_memory.buffer;
    pitch = x * byte_per_pixel;
    for(i = 0; i < y;++i){
        uint32_t* pixel = (uint32_t*)row;
        for(j = 0; j < x; ++j){
            pixel[0] = GRAY;
            if(get(j, i >> 2,f)){

                pixel[1] = BLACKISH;
                pixel[2] = BLACKISH;
                pixel[3] = BLACKISH;
            } else {

                pixel[1] = WHITEISH;
                pixel[2] = WHITEISH;
                pixel[3] = WHITEISH;
            }
            pixel += 4;
        }
        for(j = 0; j < remainder_x; ++j){
            *pixel++ = GRAY;
        }
        row = (uint8_t*)pixel;
    }
}

LRESULT CALLBACK MainWindowProc(
    HWND   window, UINT   message,
    WPARAM w_param, LPARAM l_param
                                ){
    LRESULT result = 0;
    switch(message){
        case WM_SIZE:
        {
            if(state & EDIT){
// Now We can resize
                int width = LOWORD(l_param);
                int height = HIWORD(l_param);

                ResizeGameField((width >> 2) + 1,(height >> 2) + 1);
                ResizeBitmap(width,height);
            }
        }
        break;

        case WM_SIZING:
        {
            if(state & EDIT){
                //resize only in edit mode
                result = DefWindowProc(window,message,w_param,l_param);
            }
        }
        break;
        /*
          case WM_ACTIVEWINDOW:
          {}
          break;
        */
        case WM_CHAR:
        {
            if(w_param == 'e'){
                state = EDIT;
                allocate_field(conway_field.x_size,conway_field.y_size,&edit_base_field);
                zero_field(&edit_base_field);
                zero_field(&conway_field);
            } else if(w_param == 'r'){
                state = ITERATION;
                if(edit_base_field.buffer)
                    free_field(&edit_base_field);
                edit_base_field.buffer = NULL;
            }
        }
        break;

        case WM_LBUTTONDOWN:
        {
            if(state & EDIT){
                int x = LOWORD(l_param);
                int y = HIWORD(l_param);
                if(edit_base_field.x_size < conway_field.x_size ||
                   edit_base_field.y_size < conway_field.y_size){
                    free_field(&edit_base_field);
                    allocate_field(conway_field.x_size,conway_field.y_size,&edit_base_field);
                    memcpy(edit_base_field.buffer,conway_field.buffer,((conway_field.x_size*conway_field.y_size) >> 3) + 1);
                }
                toggle(x >> 2,y >> 2,conway_field);
                toggle(x >> 2,y >> 2,edit_base_field);
                
            } else if (state & ITERATION){
                state = PAUSED;
            } else{
                state = ITERATION;
            }
        }
        break;
        
        case WM_QUIT:
        {
            running = false;
        }
        break;

        case WM_DESTROY:
        {
            running = false;
        }
        break;

        case WM_CLOSE:
        {
            running = false;
        }
        break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);

            Win32DisplayBuffer(device_context, window);

            EndPaint(window, &paint);
        }
        break;
        
        default:
        {
            result = DefWindowProc(window, message, w_param, l_param);
        }
        break;
    }
    return result;
}

void SetBitmapInfo(){
    bitmap_info.bmiHeader.biSize     = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biPlanes   = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
}


int CALLBACK WinMain(
    HINSTANCE instance,  HINSTANCE prev_instance,
    LPSTR     cmd_line,  int       show_option
                     ){
// No error handling for such a simple project
    WNDCLASS window_class_attributes = {};
    window_class_attributes.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class_attributes.lpszClassName = "ConwayWindowClass";
    window_class_attributes.hInstance = instance;
    window_class_attributes.lpfnWndProc = MainWindowProc;

    RegisterClassA(&window_class_attributes);
    HWND window = CreateWindowA(window_class_attributes.lpszClassName,   "Conway's Game Of Life",   WS_VISIBLE | WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,   CW_USEDEFAULT,   100,   100,
                                0,   0,   instance,   0);

    SetBitmapInfo();
    running = true;
    state = EDIT;
    {
        window_proportions proportions;
        proportions = GetWidthAndHeight(window);
        allocate_field(proportions.width,proportions.height,&conway_field);
        // allocate_field(proportions.width,proportions.height,&edit_base_field);
        ResizeBitmap(proportions.width,proportions.height);
    }
    HDC device_context = GetDC(window);

    while(running){
      
        MSG message_structure;

        while(PeekMessage(&message_structure, 0, 0, 0, PM_REMOVE))
        {
            if(message_structure.message == WM_QUIT)
            {
                running = false;
            }
                    
            TranslateMessage(&message_structure);
            DispatchMessageA(&message_structure);
        }
        switch(state){
            case ITERATION:
            {
                int i,j,n;
                equate_field(&swap_field,&conway_field);
                for(i = 0; i < conway_field.x_size; i++){
                    for(j = 0; j < conway_field.y_size; j++){
                        n = neighbours(i,j,conway_field);
                        if(get(i,j,conway_field)){
                            set(i,j,swap_field,!(n < 2|| n > 3));
                       
                        }
                        else{
                             set(i,j,swap_field,n == 3);
                        }
                    }
                }
                ApplyFieldToBitmap(swap_field);
                void* tmp = swap_field.buffer;
                swap_field.buffer = conway_field.buffer;
                conway_field.buffer = tmp;
                Win32DisplayBuffer(device_context,window);
                Sleep(100);
            }
            break;

            case PAUSED:
            {
                Sleep(20); // Somewhere I heard human reaction time is 35 ms so this should be
                // pretty responsive
            }
            break;

            case EDIT:
            {
                ApplyFieldToBitmap(conway_field);
                Win32DisplayBuffer(device_context,window);
            }
            break;
                
        }
        
        
    }    
}
