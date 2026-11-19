#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,216, 42,40,34,36, 36,38,52,350, 12,2,16,76,4};
inline constexpr Typography typography{9.5,10.0,0.87,8.25, 15,10,14,12};
inline constexpr const char* artworkSurface="#F2F2F7";

// Header-only immutable values keep lightweight UI consumers link-independent.
inline constexpr Colors light{
    "#FFFFFF","#F3F4F6","#F6F6F8",
    "#1C1C1E","#5D5D66","#62636B","#989CA5",
    "#D9DEE6","#E0E1E7","#0058BA","#EAF1FC",
    "#0063CF","#FFFFFF","#F0F2F6","#0058BA",
    "#F2F3F6","#EDF0F5","#004B9A",
    "#236E46","#855100","#B92330","#7D8490","#F2F2F7"};
inline constexpr Colors dark{
    "#1C1C1E","#171719","#252527",
    "#F2F2F7","#B7B7C2","#B7B7C2","#6F7580",
    "#424249","#38383E","#83BAFF","#263A52",
    "#1767BC","#FFFFFF","#303034","#83BAFF",
    "#2B2B30","#38383E","#0B529A",
    "#83D6AA","#FFC18C","#FF9CA4","#8B8B97","#F2F2F7"};
inline constexpr Colors highContrastLight{
    "#FFFFFF","#FFFFFF","#FFFFFF",
    "#000000","#1A1A1A","#333333","#6B6B6B",
    "#000000","#000000","#0B3D91","#DCE6F7",
    "#0B3D91","#FFFFFF","#EDEDED","#0B63CE",
    "#F2F2F2","#D9D9D9","#082C6B",
    "#00552E","#6B3A00","#A30018","#4A4A4A","#F2F2F7"};
inline constexpr Colors highContrastDark{
    "#000000","#000000","#101010",
    "#FFFFFF","#FFFFFF","#E6E6E6","#8C8C8C",
    "#FFFFFF","#FFFFFF","#FFD60A","#3A2F00",
    "#FFD60A","#000000","#262626","#FFD60A",
    "#1C1C1C","#333333","#FFE44D",
    "#5CFFA8","#FFD60A","#FF8A9A","#FFFFFF","#F2F2F7"};

constexpr const Colors& colors(bool isDark, bool highContrast) {
    if (highContrast) return isDark ? highContrastDark : highContrastLight;
    return isDark ? dark : light;
}

} // namespace mlash::design::ShadowrocketStyle
