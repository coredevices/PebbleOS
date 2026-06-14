/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// @generated
// THIS FILE HAS BEEN GENERATED, PLEASE DON'T MODIFY ITS CONTENT MANUALLY
// USE <TINTIN_ROOT>/tools/snowy_colors.py TO MAKE CHANGES

//! @addtogroup Graphics
//! @{

//! @addtogroup GraphicsTypes
//! @{

//! Convert RGBA to GColor.
//! @param red Red value from 0 - 255
//! @param green Green value from 0 - 255
//! @param blue Blue value from 0 - 255
//! @param alpha Alpha value from 0 - 255
//! @return GColor created from the RGB values
#define GColorFromRGBA(red, green, blue, alpha) ((GColor8){ \
  .a = (uint8_t)(alpha) >> 6, \
  .r = (uint8_t)(red) >> 6, \
  .g = (uint8_t)(green) >> 6, \
  .b = (uint8_t)(blue) >> 6, \
  })

//! Convert RGB to GColor.
//! @param red Red value from 0 - 255
//! @param green Green value from 0 - 255
//! @param blue Blue value from 0 - 255
//! @return GColor created from the RGB values
#define GColorFromRGB(red, green, blue) \
  GColorFromRGBA(red, green, blue, 255)

//! Convert hex integer to GColor.
//! @param v Integer hex value (e.g. 0x64ff46)
//! @return GColor created from the hex value
#define GColorFromHEX(v) GColorFromRGB(((v) >> 16) & 0xff, ((v) >> 8) & 0xff, ((v) & 0xff))

//! @addtogroup ColorDefinitions Color Definitions
//! A list of all of the named colors available with links to the color map on the Pebble Developer website.
//! @{

// 8bit color values of all natively supported colors
//                                                   AARRGGBB
#define GColorBlackARGB8                 ((uint8_t)0b11000000)
#define GColorOxfordBlueARGB8            ((uint8_t)0b11000001)
#define GColorDukeBlueARGB8              ((uint8_t)0b11000010)
#define GColorBlueARGB8                  ((uint8_t)0b11000011)
#define GColorDarkGreenARGB8             ((uint8_t)0b11000100)
#define GColorMidnightGreenARGB8         ((uint8_t)0b11000101)
#define GColorCobaltBlueARGB8            ((uint8_t)0b11000110)
#define GColorBlueMoonARGB8              ((uint8_t)0b11000111)
#define GColorIslamicGreenARGB8          ((uint8_t)0b11001000)
#define GColorJaegerGreenARGB8           ((uint8_t)0b11001001)
#define GColorTiffanyBlueARGB8           ((uint8_t)0b11001010)
#define GColorVividCeruleanARGB8         ((uint8_t)0b11001011)
#define GColorGreenARGB8                 ((uint8_t)0b11001100)
#define GColorMalachiteARGB8             ((uint8_t)0b11001101)
#define GColorMediumSpringGreenARGB8     ((uint8_t)0b11001110)
#define GColorCyanARGB8                  ((uint8_t)0b11001111)
#define GColorBulgarianRoseARGB8         ((uint8_t)0b11010000)
#define GColorImperialPurpleARGB8        ((uint8_t)0b11010001)
#define GColorIndigoARGB8                ((uint8_t)0b11010010)
#define GColorElectricUltramarineARGB8   ((uint8_t)0b11010011)
#define GColorArmyGreenARGB8             ((uint8_t)0b11010100)
#define GColorDarkGrayARGB8              ((uint8_t)0b11010101)
#define GColorLibertyARGB8               ((uint8_t)0b11010110)
#define GColorVeryLightBlueARGB8         ((uint8_t)0b11010111)
#define GColorKellyGreenARGB8            ((uint8_t)0b11011000)
#define GColorMayGreenARGB8              ((uint8_t)0b11011001)
#define GColorCadetBlueARGB8             ((uint8_t)0b11011010)
#define GColorPictonBlueARGB8            ((uint8_t)0b11011011)
#define GColorBrightGreenARGB8           ((uint8_t)0b11011100)
#define GColorScreaminGreenARGB8         ((uint8_t)0b11011101)
#define GColorMediumAquamarineARGB8      ((uint8_t)0b11011110)
#define GColorElectricBlueARGB8          ((uint8_t)0b11011111)
#define GColorDarkCandyAppleRedARGB8     ((uint8_t)0b11100000)
#define GColorJazzberryJamARGB8          ((uint8_t)0b11100001)
#define GColorPurpleARGB8                ((uint8_t)0b11100010)
#define GColorVividVioletARGB8           ((uint8_t)0b11100011)
#define GColorWindsorTanARGB8            ((uint8_t)0b11100100)
#define GColorRoseValeARGB8              ((uint8_t)0b11100101)
#define GColorPurpureusARGB8             ((uint8_t)0b11100110)
#define GColorLavenderIndigoARGB8        ((uint8_t)0b11100111)
#define GColorLimerickARGB8              ((uint8_t)0b11101000)
#define GColorBrassARGB8                 ((uint8_t)0b11101001)
#define GColorLightGrayARGB8             ((uint8_t)0b11101010)
#define GColorBabyBlueEyesARGB8          ((uint8_t)0b11101011)
#define GColorSpringBudARGB8             ((uint8_t)0b11101100)
#define GColorInchwormARGB8              ((uint8_t)0b11101101)
#define GColorMintGreenARGB8             ((uint8_t)0b11101110)
#define GColorCelesteARGB8               ((uint8_t)0b11101111)
#define GColorRedARGB8                   ((uint8_t)0b11110000)
#define GColorFollyARGB8                 ((uint8_t)0b11110001)
#define GColorFashionMagentaARGB8        ((uint8_t)0b11110010)
#define GColorMagentaARGB8               ((uint8_t)0b11110011)
#define GColorOrangeARGB8                ((uint8_t)0b11110100)
#define GColorSunsetOrangeARGB8          ((uint8_t)0b11110101)
#define GColorBrilliantRoseARGB8         ((uint8_t)0b11110110)
#define GColorShockingPinkARGB8          ((uint8_t)0b11110111)
#define GColorChromeYellowARGB8          ((uint8_t)0b11111000)
#define GColorRajahARGB8                 ((uint8_t)0b11111001)
#define GColorMelonARGB8                 ((uint8_t)0b11111010)
#define GColorRichBrilliantLavenderARGB8 ((uint8_t)0b11111011)
#define GColorYellowARGB8                ((uint8_t)0b11111100)
#define GColorIcterineARGB8              ((uint8_t)0b11111101)
#define GColorPastelYellowARGB8          ((uint8_t)0b11111110)
#define GColorWhiteARGB8                 ((uint8_t)0b11111111)

// GColor values of all natively supported colors

//! <span class="gcolor_sample" style="background-color: #000000;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#000000">GColorBlack</a>
#define GColorBlack                 (GColor8){.argb=GColorBlackARGB8}

//! <span class="gcolor_sample" style="background-color: #000055;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#000055">GColorOxfordBlue</a>
#define GColorOxfordBlue            (GColor8){.argb=GColorOxfordBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #0000AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#0000AA">GColorDukeBlue</a>
#define GColorDukeBlue              (GColor8){.argb=GColorDukeBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #0000FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#0000FF">GColorBlue</a>
#define GColorBlue                  (GColor8){.argb=GColorBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #005500;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#005500">GColorDarkGreen</a>
#define GColorDarkGreen             (GColor8){.argb=GColorDarkGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #005555;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#005555">GColorMidnightGreen</a>
#define GColorMidnightGreen         (GColor8){.argb=GColorMidnightGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #0055AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#0055AA">GColorCobaltBlue</a>
#define GColorCobaltBlue            (GColor8){.argb=GColorCobaltBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #0055FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#0055FF">GColorBlueMoon</a>
#define GColorBlueMoon              (GColor8){.argb=GColorBlueMoonARGB8}

//! <span class="gcolor_sample" style="background-color: #00AA00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00AA00">GColorIslamicGreen</a>
#define GColorIslamicGreen          (GColor8){.argb=GColorIslamicGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #00AA55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00AA55">GColorJaegerGreen</a>
#define GColorJaegerGreen           (GColor8){.argb=GColorJaegerGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #00AAAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00AAAA">GColorTiffanyBlue</a>
#define GColorTiffanyBlue           (GColor8){.argb=GColorTiffanyBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #00AAFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00AAFF">GColorVividCerulean</a>
#define GColorVividCerulean         (GColor8){.argb=GColorVividCeruleanARGB8}

//! <span class="gcolor_sample" style="background-color: #00FF00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00FF00">GColorGreen</a>
#define GColorGreen                 (GColor8){.argb=GColorGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #00FF55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00FF55">GColorMalachite</a>
#define GColorMalachite             (GColor8){.argb=GColorMalachiteARGB8}

//! <span class="gcolor_sample" style="background-color: #00FFAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00FFAA">GColorMediumSpringGreen</a>
#define GColorMediumSpringGreen     (GColor8){.argb=GColorMediumSpringGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #00FFFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#00FFFF">GColorCyan</a>
#define GColorCyan                  (GColor8){.argb=GColorCyanARGB8}

//! <span class="gcolor_sample" style="background-color: #550000;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#550000">GColorBulgarianRose</a>
#define GColorBulgarianRose         (GColor8){.argb=GColorBulgarianRoseARGB8}

//! <span class="gcolor_sample" style="background-color: #550055;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#550055">GColorImperialPurple</a>
#define GColorImperialPurple        (GColor8){.argb=GColorImperialPurpleARGB8}

//! <span class="gcolor_sample" style="background-color: #5500AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#5500AA">GColorIndigo</a>
#define GColorIndigo                (GColor8){.argb=GColorIndigoARGB8}

//! <span class="gcolor_sample" style="background-color: #5500FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#5500FF">GColorElectricUltramarine</a>
#define GColorElectricUltramarine   (GColor8){.argb=GColorElectricUltramarineARGB8}

//! <span class="gcolor_sample" style="background-color: #555500;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#555500">GColorArmyGreen</a>
#define GColorArmyGreen             (GColor8){.argb=GColorArmyGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #555555;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#555555">GColorDarkGray</a>
#define GColorDarkGray              (GColor8){.argb=GColorDarkGrayARGB8}

//! <span class="gcolor_sample" style="background-color: #5555AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#5555AA">GColorLiberty</a>
#define GColorLiberty               (GColor8){.argb=GColorLibertyARGB8}

//! <span class="gcolor_sample" style="background-color: #5555FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#5555FF">GColorVeryLightBlue</a>
#define GColorVeryLightBlue         (GColor8){.argb=GColorVeryLightBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #55AA00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55AA00">GColorKellyGreen</a>
#define GColorKellyGreen            (GColor8){.argb=GColorKellyGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #55AA55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55AA55">GColorMayGreen</a>
#define GColorMayGreen              (GColor8){.argb=GColorMayGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #55AAAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55AAAA">GColorCadetBlue</a>
#define GColorCadetBlue             (GColor8){.argb=GColorCadetBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #55AAFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55AAFF">GColorPictonBlue</a>
#define GColorPictonBlue            (GColor8){.argb=GColorPictonBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #55FF00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55FF00">GColorBrightGreen</a>
#define GColorBrightGreen           (GColor8){.argb=GColorBrightGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #55FF55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55FF55">GColorScreaminGreen</a>
#define GColorScreaminGreen         (GColor8){.argb=GColorScreaminGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #55FFAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55FFAA">GColorMediumAquamarine</a>
#define GColorMediumAquamarine      (GColor8){.argb=GColorMediumAquamarineARGB8}

//! <span class="gcolor_sample" style="background-color: #55FFFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#55FFFF">GColorElectricBlue</a>
#define GColorElectricBlue          (GColor8){.argb=GColorElectricBlueARGB8}

//! <span class="gcolor_sample" style="background-color: #AA0000;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA0000">GColorDarkCandyAppleRed</a>
#define GColorDarkCandyAppleRed     (GColor8){.argb=GColorDarkCandyAppleRedARGB8}

//! <span class="gcolor_sample" style="background-color: #AA0055;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA0055">GColorJazzberryJam</a>
#define GColorJazzberryJam          (GColor8){.argb=GColorJazzberryJamARGB8}

//! <span class="gcolor_sample" style="background-color: #AA00AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA00AA">GColorPurple</a>
#define GColorPurple                (GColor8){.argb=GColorPurpleARGB8}

//! <span class="gcolor_sample" style="background-color: #AA00FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA00FF">GColorVividViolet</a>
#define GColorVividViolet           (GColor8){.argb=GColorVividVioletARGB8}

//! <span class="gcolor_sample" style="background-color: #AA5500;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA5500">GColorWindsorTan</a>
#define GColorWindsorTan            (GColor8){.argb=GColorWindsorTanARGB8}

//! <span class="gcolor_sample" style="background-color: #AA5555;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA5555">GColorRoseVale</a>
#define GColorRoseVale              (GColor8){.argb=GColorRoseValeARGB8}

//! <span class="gcolor_sample" style="background-color: #AA55AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA55AA">GColorPurpureus</a>
#define GColorPurpureus             (GColor8){.argb=GColorPurpureusARGB8}

//! <span class="gcolor_sample" style="background-color: #AA55FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AA55FF">GColorLavenderIndigo</a>
#define GColorLavenderIndigo        (GColor8){.argb=GColorLavenderIndigoARGB8}

//! <span class="gcolor_sample" style="background-color: #AAAA00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAAA00">GColorLimerick</a>
#define GColorLimerick              (GColor8){.argb=GColorLimerickARGB8}

//! <span class="gcolor_sample" style="background-color: #AAAA55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAAA55">GColorBrass</a>
#define GColorBrass                 (GColor8){.argb=GColorBrassARGB8}

//! <span class="gcolor_sample" style="background-color: #AAAAAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAAAAA">GColorLightGray</a>
#define GColorLightGray             (GColor8){.argb=GColorLightGrayARGB8}

//! <span class="gcolor_sample" style="background-color: #AAAAFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAAAFF">GColorBabyBlueEyes</a>
#define GColorBabyBlueEyes          (GColor8){.argb=GColorBabyBlueEyesARGB8}

//! <span class="gcolor_sample" style="background-color: #AAFF00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAFF00">GColorSpringBud</a>
#define GColorSpringBud             (GColor8){.argb=GColorSpringBudARGB8}

//! <span class="gcolor_sample" style="background-color: #AAFF55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAFF55">GColorInchworm</a>
#define GColorInchworm              (GColor8){.argb=GColorInchwormARGB8}

//! <span class="gcolor_sample" style="background-color: #AAFFAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAFFAA">GColorMintGreen</a>
#define GColorMintGreen             (GColor8){.argb=GColorMintGreenARGB8}

//! <span class="gcolor_sample" style="background-color: #AAFFFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#AAFFFF">GColorCeleste</a>
#define GColorCeleste               (GColor8){.argb=GColorCelesteARGB8}

//! <span class="gcolor_sample" style="background-color: #FF0000;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF0000">GColorRed</a>
#define GColorRed                   (GColor8){.argb=GColorRedARGB8}

//! <span class="gcolor_sample" style="background-color: #FF0055;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF0055">GColorFolly</a>
#define GColorFolly                 (GColor8){.argb=GColorFollyARGB8}

//! <span class="gcolor_sample" style="background-color: #FF00AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF00AA">GColorFashionMagenta</a>
#define GColorFashionMagenta        (GColor8){.argb=GColorFashionMagentaARGB8}

//! <span class="gcolor_sample" style="background-color: #FF00FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF00FF">GColorMagenta</a>
#define GColorMagenta               (GColor8){.argb=GColorMagentaARGB8}

//! <span class="gcolor_sample" style="background-color: #FF5500;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF5500">GColorOrange</a>
#define GColorOrange                (GColor8){.argb=GColorOrangeARGB8}

//! <span class="gcolor_sample" style="background-color: #FF5555;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF5555">GColorSunsetOrange</a>
#define GColorSunsetOrange          (GColor8){.argb=GColorSunsetOrangeARGB8}

//! <span class="gcolor_sample" style="background-color: #FF55AA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF55AA">GColorBrilliantRose</a>
#define GColorBrilliantRose         (GColor8){.argb=GColorBrilliantRoseARGB8}

//! <span class="gcolor_sample" style="background-color: #FF55FF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FF55FF">GColorShockingPink</a>
#define GColorShockingPink          (GColor8){.argb=GColorShockingPinkARGB8}

//! <span class="gcolor_sample" style="background-color: #FFAA00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFAA00">GColorChromeYellow</a>
#define GColorChromeYellow          (GColor8){.argb=GColorChromeYellowARGB8}

//! <span class="gcolor_sample" style="background-color: #FFAA55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFAA55">GColorRajah</a>
#define GColorRajah                 (GColor8){.argb=GColorRajahARGB8}

//! <span class="gcolor_sample" style="background-color: #FFAAAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFAAAA">GColorMelon</a>
#define GColorMelon                 (GColor8){.argb=GColorMelonARGB8}

//! <span class="gcolor_sample" style="background-color: #FFAAFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFAAFF">GColorRichBrilliantLavender</a>
#define GColorRichBrilliantLavender (GColor8){.argb=GColorRichBrilliantLavenderARGB8}

//! <span class="gcolor_sample" style="background-color: #FFFF00;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFFF00">GColorYellow</a>
#define GColorYellow                (GColor8){.argb=GColorYellowARGB8}

//! <span class="gcolor_sample" style="background-color: #FFFF55;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFFF55">GColorIcterine</a>
#define GColorIcterine              (GColor8){.argb=GColorIcterineARGB8}

//! <span class="gcolor_sample" style="background-color: #FFFFAA;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFFFAA">GColorPastelYellow</a>
#define GColorPastelYellow          (GColor8){.argb=GColorPastelYellowARGB8}

//! <span class="gcolor_sample" style="background-color: #FFFFFF;"></span> <a href="https://developer.getpebble.com/tools/color-picker/#FFFFFF">GColorWhite</a>
#define GColorWhite                 (GColor8){.argb=GColorWhiteARGB8}

// Additional 8bit color values
#define GColorClearARGB8 ((uint8_t)0b00000000)

// Additional GColor values
#define GColorClear ((GColor8){.argb=GColorClearARGB8})

//! @} // group ColorDefinitions

//! @} // group GraphicsTypes

//! @} // group Graphics

