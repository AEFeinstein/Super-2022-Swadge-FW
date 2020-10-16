#include "math.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef enum
{
    WMT_W1 = 0,
    WMT_W2 = 1,
    WMT_W3 = 2,
    WMT_C  = 3,
    WMT_E  = 4,
    WMT_S  = 5,
} WorldMapTile_t;

int main (void)
{
    int w, h, n;
    unsigned char* data = stbi_load("map.png", &w, &h, &n, 0);
    if(NULL != data)
    {
        printf("%d by %d (%d)\n", w, h, n);

        int dataIdx = 0;
        for (int y = 0; y < h; y++)
        {
            printf("{");
            for (int x = 0; x < w; x++)
            {
                int r = (data[dataIdx++]);
                int g = (data[dataIdx++]);
                int b = (data[dataIdx++]);

                if(r == 0xFF && g == 0xFF && b == 0xFF)
                {
                    printf("%d, ", WMT_E); // Empty
                }
                else if(r == 0x80 && g == 0x80 && b == 0x80)
                {
                    printf("%d, ", WMT_S); // Spawn
                }
                else if(r == 0xFF)
                {
                    printf("%d, ", WMT_W1); // Wall 1
                }
                else if(g == 0xFF)
                {
                    printf("%d, ", WMT_W2); // Wall 2
                }
                else if(b == 0xFF)
                {
                    printf("%d, ", WMT_W3); // Wall 3
                }
                else
                {
                    printf("%d, ", WMT_C); // Column
                }
            }
            printf("},\n");
        }
        printf("\n");
        // ... process data if not NULL ...
        // ... x = width, y = height, n = # 8-bit components per pixel ...
        // ... replace "0" with "1".."4" to force that many components per pixel
        // ... but "n" will always be the number that it would have been if you said 0
        stbi_image_free(data);
    }
    return 0;
}