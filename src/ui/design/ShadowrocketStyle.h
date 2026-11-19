#pragma once

#include "UiDesignTokens.h"

namespace mlash::design::ShadowrocketStyle {

inline constexpr Geometry geometry{30,28,8,8, 14,12,12,216, 42,40,34,36, 36,38,52,350, 12,2,16,76,4};
inline constexpr Typography typography{9.5,10.0,0.87,8.25, 15,10,14,12};
inline constexpr const char* artworkSurface="#F2F2F7";

// Header-only immutable values keep lightweight UI consumers link-independent.
inline constexpr Colors light{
    "#FAFAFC","#F5F6F8","#FFFFFF",
    "#1D1D1F","#585E68","#62636B","#989DA6",
    "#DDE1E7","#E7E9ED","#1769C2","#EAF2FF",
    "#1769C2","#FFFFFF","#F0F4F9","#1769C2",
    "#F5F6F8","#E9EDF3","#0E5AA6",
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
