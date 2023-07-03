#include "../include/tty.h"
#include "../include/string.h"
#include "../include/debug.h"
#include "../include/def.h"
size_t terminal_row;
size_t terminal_column;
//static uint16_t *const VGA_MEMORY = 0xb8000;
uint8_t terminal_color;
uint16_t *terminal_buffer;

#define BUFFER_SIZE 1024
#define VGA_DDD 0xb8000
 unsigned char *VGA_MEMORY = (unsigned char *) VGA_DDD;
static inline uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

static inline uint16_t make_vgaentry(char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

int terminal_initialize(enum vga_color font_color, enum vga_color background_color)
{
    terminal_row = 20;
    terminal_column = 0;
    terminal_color = make_color(font_color, background_color);
    terminal_buffer = VGA_MEMORY;
    size_t y;
    for (y = 0; y < VGA_HEIGHT; y++)
    {
        size_t x;
        for (x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = make_vgaentry(' ', terminal_color);
        }
    }
    return 0;
}
int reset_console(enum vga_color font_color, enum vga_color background_color)
{
    terminal_row = 20;
    terminal_column = 0;
    terminal_color = make_color(font_color, background_color);
    terminal_buffer = VGA_MEMORY;
    size_t y;
    for (y = 20; y < VGA_HEIGHT; y++)
    {
        size_t x;
        for (x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = make_vgaentry(' ', terminal_color);
        }
    }
    set_cursor(get_offset(terminal_column,terminal_row));
    return 0;
}
void terminal_scroll()
{
    int i;
    for (i = 20; i < VGA_HEIGHT; i++)
    {
        int m;
        for (m = 0; m < VGA_WIDTH; m++)
        {
            terminal_buffer[i * VGA_WIDTH + m] = terminal_buffer[(i + 1) * VGA_WIDTH + m];
        }
        //terminal_row--;
    }
    terminal_row = VGA_HEIGHT - 0;
}

void terminal_putentryat(char c, uint8_t color, int offset)
{
    // const size_t index = y * VGA_WIDTH + x;
    // terminal_buffer[index] = make_vgaentry(c, color);
    write_serial(c,DEFAULT_COM_DEBUG_PORT);
    unsigned char *vidmem = (unsigned char *) VIDEO_ADDRESS;
    vidmem[offset] = c;
    vidmem[offset + 1] = color;
    
    
}

void terminal_putchar(char c)
{
    if (c == '\n' || c == '\r')
    {
        // terminal_column = 0;
        // terminal_row++;
        // 2 * (row * MAX_COLS + col);
        // row = r
        // MAX_VOLS = m
        //col = c
        // offset = o
        //O=2R2M+2C
        //
        //offset = ;
        set_cursor(move_offset_to_new_line(get_cursor()));
        if (terminal_row == VGA_HEIGHT)
            reset_console(COLOR_LIGHT_GREEN,COLOR_BLACK);
            
        return;
    }
    else if (c == '\t')
    {
        terminal_column += 4;
        set_cursor(get_cursor()+4); 
        return;
    }
    else if (c == '\b')
    {
        terminal_putentryat(' ', terminal_color, get_cursor()-1);
        terminal_putentryat(' ', terminal_color, get_cursor());
        return;
    }
    terminal_putentryat(c, terminal_color,get_cursor());
    int offset = get_cursor();
    set_cursor(offset+2);
    if (++terminal_column == VGA_WIDTH)
    {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
        {
            terminal_row = 0;
        }
    }
}

void terminal_write(const char *data, size_t size)
{
    size_t i;
    for (i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

int putchar(int ic)
{
    char c = (char)ic;
    terminal_write(&c, sizeof(c));
    return ic;
}

void term_putc(char c, enum vga_color char_color)
{
    unsigned int i = 0; // place holder for text string position
    unsigned int j = 0; // place holder for video buffer position

    int index;
    // Remember - we don't want to display ALL characters!
    switch (c)
    {
    case '\n': // Newline characters should return the column to 0, and increment the row
    {
        terminal_column = 0;
        terminal_row += 2;
        break;
    }

    default: // Normal characters just get displayed and then increment the column
    {
        index = (VGA_WIDTH * terminal_row) + terminal_column; // Like before, calculate the buffer index
        VGA_MEMORY[index] = c;
        VGA_MEMORY[index + 1] = char_color;
        // terminal_column += 2;
        break;
    }
    }

    // What happens if we get past the last column? We need to reset the column to 0, and increment the row to get to a new line
    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;
    }

    // What happens if we get past the last row? We need to reset both column and row to 0 in order to loop back to the top of the screen
    if (terminal_row >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row = 0;
    }
}

// This function prints an entire string onto the screen
// void term_print(const char *str)
// {
//     int i;
//     for (i = 0; str[i] != '\0'; i++)
//     { // Keep placing characters until we hit the null-terminating character ('\0')
//         term_putc(str[i]);
//     }
// }

static void print(const char *data, size_t data_length)
{
    size_t i;
    for (i = 0; i < data_length; i++)
        putchar((int)((const unsigned char *)data)[i]);
}

int normalize(double *val)
{
    int exponent = 0;
    double value = *val;

    while (value >= 1.0)
    {
        value /= 10.0;
        ++exponent;
    }

    while (value < 0.1)
    {
        value *= 10.0;
        --exponent;
    }
    *val = value;
    return exponent;
}

static void ftoa_fixed(char *buffer, double value)
{
    /* carry out a fixed conversion of a double value to a string, with a precision of 5 decimal digits.
     * Values with absolute values less than 0.000001 are rounded to 0.0
     * Note: this blindly assumes that the buffer will be large enough to hold the largest possible result.
     * The largest value we expect is an IEEE 754 double precision real, with maximum magnitude of approximately
     * e+308. The C standard requires an implementation to allow a single conversion to produce up to 512
     * characters, so that's what we really expect as the buffer size.
     */

    int exponent = 0;
    int places = 0;
    static const int width = 4;

    if (value == 0.0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    if (value < 0.0)
    {
        *buffer++ = '-';
        value = -value;
    }

    exponent = normalize(&value);

    while (exponent > 0)
    {
        int digit = value * 10;
        *buffer++ = digit + '0';
        value = value * 10 - digit;
        ++places;
        --exponent;
    }

    if (places == 0)
        *buffer++ = '0';

    *buffer++ = '.';

    while (exponent < 0 && places < width)
    {
        *buffer++ = '0';
        --exponent;
        ++places;
    }

    while (places < width)
    {
        int digit = value * 10.0;
        *buffer++ = digit + '0';
        value = value * 10.0 - digit;
        ++places;
    }
    *buffer = '\0';
}

void ftoa_sci(char *buffer, double value)
{
    int exponent = 0;
    int places = 0;
    static const int width = 4;

    if (value == 0.0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    if (value < 0.0)
    {
        *buffer++ = '-';
        value = -value;
    }

    exponent = normalize(&value);

    int digit = value * 10.0;
    *buffer++ = digit + '0';
    value = value * 10.0 - digit;
    --exponent;

    *buffer++ = '.';

    for (int i = 0; i < width; i++)
    {
        int digit = value * 10.0;
        *buffer++ = digit + '0';
        value = value * 10.0 - digit;
    }

    *buffer++ = 'e';
    itoa(buffer, exponent, 10);
}

int printf(const char *format, ...)
{
    va_list parameters;
    va_start(parameters, format);
    int written = 0;
    size_t amount;
    int rejected_bad_specifier = 0;
    while (*format != '\0')
    {
        if (*format != '%')
        {
        print_c:
            amount = 1;
            while (format[amount] && format[amount] != '%')
                amount++;
            print(format, amount);
            format += amount;
            written += amount;
            continue;
        }
        const char *format_begun_at = format;
        if (*(++format) == '%')
            goto print_c;
        if (rejected_bad_specifier)
        {
        incomprehensible_conversion:
            rejected_bad_specifier = 1;
            format = format_begun_at;
            goto print_c;
        }
        if (*format == 'c')
        {
            format++;
            char c = (char)va_arg(parameters, int /* char promotes to int */);
            print(&c, sizeof(c));
        }
        else if (*format == 'd')
        {
            format++;
            char *s;
            itoa(s, va_arg(parameters, int), 10);
            print(s, strlen(s));
        }
        else if (*format == 'f')
        {
            format++;
            char *s;
            ftoa_fixed(s, va_arg(parameters, double));
            print(s, strlen(s));
        }
        else if (*format == 'e')
        {
            format++;
            char *s;
            ftoa_sci(s, va_arg(parameters, double));
            print(s, strlen(s));
        }
        else if (*format == 'x')
        {
            format++;
            char *s;
            itoa(s, va_arg(parameters, unsigned int), 16);
            print("0x", 2);
            print(s, strlen(s));
        }
        else if (*format == 'p')
        {
            format++;
            char *s;
            const void *ptr = va_arg(parameters, void *);
            uintptr_t uptr = (uintptr_t)ptr;
            itoa(s, uptr, 16);
            print("0x", 2);
            print(s, strlen(s));
        }
        else if (*format == 's')
        {
            format++;
            const char *s = va_arg(parameters, const char *);
            print(s, strlen(s));
        }
        else
        {
            goto incomprehensible_conversion;
        }
    }
    va_end(parameters);
    return written;
}

int get_terminal_row(void)
{
    return terminal_row;
}

int get_terminal_col(void)
{
    return terminal_column;
}
int set_terminal_row(int row)
{
    memset(terminal_row,row,sizeof(terminal_row));    
}
int set_terminal_col(int col){
    memset(terminal_column,col,sizeof(terminal_column));
}

void terminal_set_colors(enum vga_color font_color, enum vga_color background_color)
{
    terminal_color = make_color(font_color, background_color);
    // terminal_buffer = VGA_MEMORY;
    // size_t y;
    // for (y = 0; y < VGA_HEIGHT; y++)
    // {
    //     size_t x;
    //     for (x = 0; x < VGA_WIDTH; x++)
    //     {
    //         const size_t index = y * VGA_WIDTH + x;
    //         terminal_buffer[index] = make_vgaentry('\0', terminal_color);
    //     }
    // }
}

void cls_command_output()
{
    
    for (int j = 1; j < 4; j++)
    {
        
        for(int i = 0; i <= 79; i++)
        {
            set_cursor(get_offset(i,j));
            terminal_set_colors(COLOR_WHITE,COLOR_BLACK);
            printf(" ");

        }

    }
    
}

void def_rows()
{
    for (size_t y = 0; y <= 80; y++)
    {
        for (size_t x = 0; x <= 25; x++)
        {
            set_cursor(get_offset(y,x));
            terminal_set_colors(default_font_color,COLOR_BLACK);
            printf(" ");
            //delay(1);
        }
        delay(1);
        
    }
    
}
int refresh_row(int delay_sec,int row)
{
    for (size_t i = 0; i < 80; i++)
    {
        set_cursor(get_offset(i,row));
        terminal_set_colors(COLOR_BLACK,COLOR_BLACK);
        printf(" ");
        delay(delay_sec);
    }
    

}
