#ifndef ARABIC_FONT_H
#define ARABIC_FONT_H

void arabicFontInit(void);
float arabicMeasureText(float scale, const char *utf8);
void arabicDrawTextRTL(float right_x, float y, float scale, unsigned int color, const char *utf8);
void arabicDrawTextCenter(float center_x, float y, float scale, unsigned int color, const char *utf8);
void arabicDrawTextLeft(float left_x, float y, float scale, unsigned int color, const char *utf8);

#endif
