/*
 * bresenham.h
 *
 *  Created on: Mar 3, 2019
 *      Author: Adam Feinstein
 */

#ifndef SRC_BRESENHAM_H_
#define SRC_BRESENHAM_H_

void plotLine(int x0, int y0, int x1, int y1);
void plotRect(int x0, int y0, int x1, int y1);
void plotEllipse(int xm, int ym, int a, int b);
void plotOptimizedEllipse(int xm, int ym, int a, int b);
void plotCircle(int xm, int ym, int r);
void plotEllipseRect(int x0, int y0, int x1, int y1);
void plotQuadBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2);
void plotQuadBezier(int x0, int y0, int x1, int y1, int x2, int y2);
void plotQuadRationalBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2, float w);
void plotQuadRationalBezier(int x0, int y0, int x1, int y1, int x2, int y2, float w);
void plotRotatedEllipse(int x, int y, int a, int b, float angle);
void plotRotatedEllipseRect(int x0, int y0, int x1, int y1, long zd);
void plotCubicBezierSeg(int x0, int y0, float x1, float y1, float x2, float y2, int x3, int y3);
void plotCubicBezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
void plotQuadSpline(int n, int x[], int y[]);
void plotCubicSpline(int n, int x[], int y[]);

#endif /* SRC_BRESENHAM_H_ */
