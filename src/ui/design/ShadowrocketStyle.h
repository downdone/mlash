#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,216, 42,40,34,36, 36,38,52,350, 12,2,16,76,4};
inline constexpr Typography typography{9.5,10.0,0.87,8.25, 15,10,14,12};
inline constexpr const char* artworkSurface="#F2F2F7";

// Header-only immutable values keep lightweight UI consumers link-independent.
inline constexpr Colors light{
    "#F8F9FB","#F4F6F8","#FFFFFF",
    "#202833","#536171","#596675","#9AA5B1",
    "#DAE2EA","#E8EDF2","#246DB5","#EDF4FC",
    "#246DB5","#FFFFFF","#F1F5F9","#246DB5",
    "#F2F4F7","#E6EDF4","#1D5E9E",
    "#236E46","#855100","#B92330","#8A919C","#F2F2F7","#34C759"};
inline constexpr Colors dark{
    "#222326","#25262A","#2D2E32",
    "#F5F5F7","#CDD0D8","#B9BFC8","#858A93",
    "#50535B","#42454C","#A5C9FF","#2D4564",
    "#2B73BE","#FFFFFF","#36393F","#A5C9FF",
    "#303238","#3D4249","#195B9A",
    "#91DDB4","#FFD09C","#FFAFB5","#969AA3","#F2F2F7","#30D158"};
inline constexpr Colors highContrastLight{
    "#FFFFFF","#FFFFFF","#FFFFFF",
    "#000000","#1A1A1A","#333333","#6B6B6B",
    "#000000","#000000","#0B3D91","#DCE6F7",
    "#0B3D91","#FFFFFF","#EDEDED","#0B63CE",
    "#F2F2F2","#D9D9D9","#082C6B",
    "#00552E","#6B3A00","#A30018","#4A4A4A","#F2F2F7","#00552E"};
inline constexpr Colors highContrastDark{
    "#000000","#000000","#101010",
    "#FFFFFF","#FFFFFF","#E6E6E6","#8C8C8C",
    "#FFFFFF","#FFFFFF","#FFD60A","#3A2F00",
    "#FFD60A","#000000","#262626","#FFD60A",
    "#1C1C1C","#333333","#FFE44D",
    "#5CFFA8","#FFD60A","#FF8A9A","#FFFFFF","#F2F2F7","#5CFFA8"};

constexpr const Colors& colors(bool isDark, bool highContrast) {
    if (highContrast) return isDark ? highContrastDark : highContrastLight;
    return isDark ? dark : light;
}

} // namespace mlash::design::ShadowrocketStyle
