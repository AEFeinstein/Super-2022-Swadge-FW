#ifndef MATRIX_FAST_FIRE_H_
#define MATRIX_FAST_FIRE_H_

void mff_setup(void);

void make_fire(void);
void newflare(void);
void glow( int x, int y, int z );
uint32_t isqrt(uint32_t n);
uint16_t pos(uint16_t col, uint16_t row);

#endif // MATRIX_FAST_FIRE_H_