package com.gelakinetic.magfont;

import java.awt.image.BufferedImage;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import javax.imageio.ImageIO;

public class Magfont {

    private static final String chars = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

    private static void writeFontFiles(File imageFile, String fontName) {
        int xInd = 0;
        int charsIdx = 0;
        try (BufferedWriter cFontFile = new BufferedWriter(new FileWriter(fontName + ".c"));
             BufferedWriter hFontFile = new BufferedWriter(new FileWriter(fontName + ".h"))) {

            BufferedImage bi = ImageIO.read(imageFile);

            int fontHeight = bi.getHeight() - 2;

            hFontFile.write("#ifndef " + fontName.toUpperCase() + "_H \n");
            hFontFile.write("#define " + fontName.toUpperCase() + "_H \n");
            hFontFile.write("\n");
            hFontFile.write("#include \"font.h\"\n");
            hFontFile.write("\n");
            hFontFile.write("#define FONT_HEIGHT_" + fontName.toUpperCase() + " " + fontHeight + "\n");
            hFontFile.write("\n");
            hFontFile.write("extern sprite_t font_" + fontName + "[];\n");
//            hFontFile.write("uint8_t plotChar" + fontName + "(bool dispBuffer[WIDTH][HEIGHT], uint8_t x, uint8_t y, char character);\n");
            hFontFile.write("\n");
            hFontFile.write("#endif\n");

//            System.out.print(
//                    "typedef struct {\n"
//                            + "    uint8_t  width;\n"
//                            + "    uint16_t data[FONT_HEIGHT];\n"
//                            + "} sprite_t;\n\n");

            cFontFile.write("#include \"" + fontName + ".h\"\n");
            cFontFile.write("\n");
            cFontFile.write("sprite_t font_" + fontName + "[] = {\n");

            // Scan over the whole bitmap
            while (xInd < bi.getWidth()) {
                int charStartX = xInd;

                // Scan the line under each char to see how wide it is
                int charWidth = 0;
                while (xInd < bi.getWidth() && 0 == (bi.getRGB(xInd, fontHeight + 1) & 0x00FFFFFF)) {
                    xInd++;
                    charWidth++;
                }

                // Scan the char above the line
                int[] rowData = new int[fontHeight];
                for (int charY = 0; charY < fontHeight; charY++) {
                    rowData[charY] = 0;
                    for (int charX = charStartX; charX < xInd; charX++) {
                        rowData[charY] <<= 1;
                        if (0 == (bi.getRGB(charX, charY) & 0x00FFFFFF)) {
                            rowData[charY] |= 1;
//                            System.out.print("X");
                        } else {
//                            System.out.print(" ");
                        }
                    }
//                    System.out.print("\n");
                }
//                System.out.print("\n");

                String charName = toCamelCase(Character.getName(chars.charAt(charsIdx)));
                cFontFile.write(
                        "    {\n"
                                + "        .width = " + charWidth + ",\n"
                                + "        .data = {\n");
                for (int i = 0; i < fontHeight; i++) {
                    cFontFile.write("            0b" + String.format("%16s", Integer.toBinaryString(rowData[i])).replace(' ', '0') + ", \n");
                }
                cFontFile.write(
                        "         },\n"
                                + "    },\n");
                xInd++;
                charsIdx++;

                while (xInd < bi.getWidth() &&
                        0 != (bi.getRGB(xInd, fontHeight + 1) & 0x00FFFFFF)) {
                    xInd++;
                }
            }

            cFontFile.write("};\n");
            cFontFile.write("\n");
//            cFontFile.write("uint8_t plotChar" + fontName + "(bool dispBuffer[WIDTH][HEIGHT], uint8_t x, uint8_t y, char character) {\n");
//            cFontFile.write("	if('a' <= character && character <= 'z') {\n");
//            cFontFile.write("		character = (char)(character - 'a' + 'A');\n");
//            cFontFile.write("	}\n");
//            cFontFile.write("	const sprite_t * sprite = &font_" + fontName + "[character - ' '];\n");
//            cFontFile.write("\n");
//            cFontFile.write("	for (uint8_t charX = 0; charX < sprite->width; charX++) {\n");
//            cFontFile.write("		for (uint8_t charY = 0; charY < FONT_HEIGHT_" + fontName.toUpperCase() + "; charY++) {\n");
//            cFontFile.write("			uint8_t xPx = (uint8_t)(x + (sprite->width - charX) - 1);\n");
//            cFontFile.write("			uint8_t yPx = (uint8_t)(y + charY);\n");
//            cFontFile.write("			if (xPx < WIDTH && yPx < HEIGHT) {\n");
//            cFontFile.write("				if (0 != (sprite->data[charY] & (1 << charX))) {\n");
//            cFontFile.write("					dispBuffer[xPx][yPx] = true;\n");
//            cFontFile.write("				} else {\n");
//            cFontFile.write("					dispBuffer[xPx][yPx] = false;\n");
//            cFontFile.write("				}\n");
//            cFontFile.write("			}\n");
//            cFontFile.write("		}\n");
//            cFontFile.write("	}\n");
//            cFontFile.write("\n");
//            cFontFile.write("	return (uint8_t)(x + sprite->width + 1);\n");
//            cFontFile.write("}\n");

//            System.out.print("const sprite_t * charMap[95] = {0};\n");
//            System.out.print("void initCharMap(void) {\n");
//            for (charsIdx = 0; charsIdx < chars.length(); charsIdx++) {
//                System.out.print("    charMap['" + chars.charAt(charsIdx) + "' - ' '] = &"
//                        + toCamelCase(Character.getName(chars.charAt(charsIdx))) + ";\n");
//                if (Character.isAlphabetic(chars.charAt(charsIdx))) {
//                    System.out.print("    charMap['" + Character.toLowerCase(chars.charAt(charsIdx)) + "' - ' '] = &"
//                            + toCamelCase(Character.getName(chars.charAt(charsIdx))) + ";\n");
//                }
//            }
//            System.out.print("}\n\n");
//
//            System.out.print("const sprite_t * getCharSprite(char character) {\n");
//            System.out.print("    if (' ' <= character && character <= '~') {\n");
//            System.out.print("        return charMap[character - ' '];\n");
//            System.out.print("    }\n");
//            System.out.print("    return NULL;\n");
//            System.out.print("}\n");

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        writeFontFiles(new File("radiostars_12_clean_round_thin.bmp"), "Radiostars");
        writeFontFiles(new File("tom_thumb.bmp"), "TomThumb");
        writeFontFiles(new File("ibm_vga_8.bmp"), "IbmVga8");
    }

    private static String toCamelCase(final String init) {
        if (init == null) {
            return null;
        }

        final StringBuilder ret = new StringBuilder(init.length());

        for (final String word : init.split("[ -]")) {
            if (!word.isEmpty()) {
                ret.append(Character.toUpperCase(word.charAt(0)));
                ret.append(word.substring(1).toLowerCase());
            }
        }

        return ret.toString();
    }
}
